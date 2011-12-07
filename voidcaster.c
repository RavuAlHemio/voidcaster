/**
 * @file voidcaster.c
 *
 * @brief Magical tool which suggests casts to void.
 *
 * A tool which uses the LLVM project's Clang C language frontend to add casts
 * to void where it deems it appropriate. It's actually not that magical at all,
 * but I had you fooled there, didn't I?
 *
 * @author Ondřej Hošek <ondrej.hosek@tuwien.ac.at>
 */
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

#include <clang-c/Index.h>

#include "msa.h"

/* make getopt accept -g option iff a system include path was specified */
#ifdef GCC_SYSINCLUDE
#define GETOPT_G "g"
#else
#define GETOPT_G ""
#endif

/** The possible exit codes of this program. */
enum exitcodes_e
{
	/**
	 * No problems were encountered. If extended status mode is activated
	 * (command-line flag -s), no suggestions for casts were given.
	 */
	EXITCODE_OK = 0,

	/** Command-line arguments have been specified incorrectly. */
	EXITCODE_USAGE = 1,

	/** A file could not be opened. */
	EXITCODE_FILE_OPEN = 2,

	/** A file could not be parsed. */
	EXITCODE_FILE_PARSE = 3,

	/** Extended status mode is active and a suggestion was given. */
	EXITCODE_EXT_SUGGEST = 4,

	/** Clang-related internal failure. */
	EXITCODE_CLANG_FAIL = 5,

	/** Memory management error. */
	EXITCODE_MM = 6
};

/**
 * A structure containing the state of the descent through the AST.
 */
typedef struct
{
	/** The current recursion depth. */
	size_t level;

	/** Are we preceded by a cast to void? */
	bool voidCastAbove;

	/** Are we preceded by a compound statement? */
	bool compoundStmtAbove;

	/** The location of the cast to void. Valid iff voidCastAbove. */
	struct
	{
		/** The file. */
		const char *file;

		/** The line where the cast begins. */
		size_t startLn;

		/** The column where the cast begins. */
		size_t startCol;

		/** The line where the cast ends. */
		size_t endLn;

		/** The column where the cast ends. */
		size_t endCol;
	} castLoc;
} descent_state;

/** Has there been a suggestion? */
static bool suggest = false;

/** The name of the program binary, taken from argv[0]. */
static const char *progname = "<not set>";

/**
 * Prints usage information about this program and exits with return code 1.
 * @note This function does not return.
 */
static void usage(void) __attribute__((noreturn));

static void usage(void)
{
	fprintf(stderr,
		"Usage: %s [OPTION]... FILE...\n"
		"Proposes locations for casts to void in a C program.\n"
		"\n"
		"  -D<macro>[=<value>]    macro to define\n"
#ifdef GCC_SYSINCLUDE
		"  -g                     don't add the include path of the installed GCC\n"
		"                         automatically\n"
#endif
		"  -i                     interactive mode\n"
		"  -I<path>               add a path where the preprocessor shall search\n"
		"                         for includes\n"
		"  -s                     exit with code 4 if a suggestion is given\n"
		"\n"
		"Exit status:\n"
		" 0  if OK\n"
		" 1  if command-line arguments where specified incorrectly\n"
		" 2  if a file could not be opened\n"
		" 3  if a file could not be parsed\n"
		" 4  if -s is set and a suggestion was given\n"
		" 5  if memory management fails\n"
		"\n"
		"Report voidcaster bugs to ondrej.hosek@tuwien.ac.at\n"
		"voidcaster home page: none yet\n",
		progname
	);
	exit(EXITCODE_USAGE);
}

/**
 * Acts upon a missing cast to void.
 *
 * @param file the name of the file where the cast is missing
 * @param func the name of the function called
 * @param line the line on which the cast should be inserted
 * @param col the column in which the cast should be inserted
 */
static void actUponMissingVoid(const char *file, const char *func, size_t line, size_t col)
{
	fprintf(stderr,
		"%s:%zu:%zu: Missing cast to void when calling function %s.\n",
		file, line, col, func
	);
}

/**
 * Acts upon a superfluous cast to void.
 *
 * @param file the name of the file containing the cast
 * @param func the name of the function called
 * @param startLine the line on which the cast starts
 * @param startCol the column in which the cast starts
 * @param stopLine the line on which the cast ends
 * @param stopCol the column in which the cast ends
 */
static void actUponSuperfluousVoid(const char *file, const char *func, size_t startLine, size_t startCol, size_t stopLine, size_t stopCol)
{
	fprintf(stderr,
		"%s:%zu:%zu: Pointless cast to void when calling function %s.\n",
		file, startLine, startCol, func
	);
}

/**
 * Stores the location information from the cursor into the parameters
 * passed by reference.
 *
 * @param cur cursor whose location to obtain
 * @param locFileName by-ref to string specifying the filename
 * @param locLn by-ref to integer specifying the line
 * @param locCol by-ref to integer specifying the column
 */
static inline void cursorLocation(CXCursor cur, CXString *locFileName, unsigned int *locLn, unsigned int *locCol)
{
	CXSourceLocation loc = clang_getCursorLocation(cur);
	clang_getPresumedLocation(loc, locFileName, locLn, locCol);
}

/**
 * Returns the extent of the cast referenced by the given cursor.
 *
 * @param cur cursor to C-style cast whose extent to obtain
 * @param startLine the line where the cast begins
 * @param startCol the column where the cast begins
 * @param stopLine the line where the cast ends
 * @param stopCol the column where the cast ends
 */
static inline void castExtent(CXCursor cur, size_t *startLine, size_t *startCol, size_t *stopLine, size_t *stopCol)
{
	CXToken *toks;
	CXCursor *curs;
	CXTranslationUnit tu = clang_Cursor_getTranslationUnit(cur);
	CXSourceRange curExt;
	unsigned int numToks, i;
	bool startSet = false;

	assert(clang_getCursorKind(cur) == CXCursor_CStyleCastExpr);

	/* where is it? */
	CXSourceRange rng = clang_getCursorExtent(cur);

	/* tokenize the cast */
	clang_tokenize(tu, rng, &toks, &numToks);

	/* annotate it */
	curs = malloc(numToks * sizeof(CXCursor));
	if (curs == NULL)
	{
		perror("malloc");
		exit(EXITCODE_MM);
	}
	clang_annotateTokens(tu, toks, numToks, curs);

	/* find the tokens which correspond to the cursor */
	for (i = 0; i < numToks; ++i)
	{
		CXSourceRange tokExt;
		unsigned int newEL, newEC;

		if (
			clang_getCursorKind(cur) != clang_getCursorKind(curs[i]) ||
			!clang_equalTypes(clang_getCursorType(cur), clang_getCursorType(curs[i]))
		)
		{
			/* not part of our cast anymore */
			break;
		}

		/* fetch token range */
		tokExt = clang_getTokenExtent(tu, toks[i]);
		clang_getPresumedLocation(clang_getRangeEnd(tokExt), NULL, &newEL, &newEC);

		if (!startSet)
		{
			unsigned int newSL, newSC;
			clang_getPresumedLocation(clang_getRangeStart(tokExt), NULL, &newSL, &newSC);
			*startLine = newSL;
			*startCol = newSC;
			startSet = true;
		}

		*stopLine = newEL;
		*stopCol = newEC;
	}

	/* free cursors, free tokens */
	free(curs);
	clang_disposeTokens(tu, toks, numToks);
}

/**
 * Called upon every node visited in a translation unit.
 *
 * @param cur the cursor pointing to the node
 * @param parent the cursor pointing to the parent
 * @param dta data passed to clang_visitCursorChildren()
 */
static enum CXChildVisitResult visitation(CXCursor cur, CXCursor parent, CXClientData dta)
{
	CXString locFileName;
	bool disposeFileName = false;

	descent_state *dstate = (descent_state *)dta;
	descent_state kiddstate = {
		.level = dstate->level + 1,
		.voidCastAbove = false,
		.compoundStmtAbove = false
	};

	/* kind of cursor */
	enum CXCursorKind curKind = clang_getCursorKind(cur);

	if (curKind == CXCursor_CompoundStmt)
	{
		/* compound statement above. means the function call tosses away its value. */
		kiddstate.compoundStmtAbove = true;
	}
	else if (curKind == CXCursor_CStyleCastExpr)
	{
		/* it's a cast. is it to void? */
		if (clang_getCursorType(cur).kind == CXType_Void)
		{
			/* yay! */
			disposeFileName = true;

			/* fetch location info */
			castExtent(
				cur,
				&kiddstate.castLoc.startLn,
				&kiddstate.castLoc.startCol,
				&kiddstate.castLoc.endLn,
				&kiddstate.castLoc.endCol
			);

			/* store it for the kid */
			kiddstate.voidCastAbove = true;
			kiddstate.castLoc.file = clang_getCString(locFileName);
		}
	}
	else if (curKind == CXCursor_CallExpr)
	{
		/* name of the function */
		CXString funcName = clang_getCursorSpelling(cur);
		/* the function declaration */
		CXCursor target = clang_getCursorReferenced(cur);
		/* the location info */
		unsigned int locLn, locCol;
		/* the type of the function and its return type */
		CXType retType;

		disposeFileName = true;

		cursorLocation(cur, &locFileName, &locLn, &locCol);

		if (clang_Cursor_isNull(target) || clang_getCursorType(target).kind == CXType_FunctionNoProto)
		{
			/* function decl not found */
			fprintf(stderr,
				"%s:%d:%d: Warning: can't check call to %s (can't find original definition).\n",
				clang_getCString(locFileName), locLn, locCol,
				clang_getCString(funcName)
			);
		}
		else
		{
			switch (clang_getCursorResultType(target).kind)
			{
				case CXType_Void:
					if (dstate->voidCastAbove)
					{
						/* magic! */
						actUponSuperfluousVoid(
							clang_getCString(locFileName),
							clang_getCString(funcName),
							dstate->castLoc.startLn,
							dstate->castLoc.startCol,
							dstate->castLoc.endLn,
							dstate->castLoc.endCol
						);
					}
					break;
				case CXType_Invalid:
				case CXType_Unexposed:
					/* can't judge; skip */
					break;
				default:
					if (dstate->compoundStmtAbove && !dstate->voidCastAbove)
					{
						actUponMissingVoid(
							clang_getCString(locFileName),
							clang_getCString(funcName),
							locLn,
							locCol
						);
					}
					break;
			}
		}

		clang_disposeString(funcName);
	}
	else if (curKind == CXCursor_BinaryOperator)
	{
		/* FIXME: check for void casts in the comma operator? */
	}
#ifdef DEBUG
	else
	{
		/* the location info */
		unsigned int locLn, locCol;

		CXString cursDesc = clang_getCursorDisplayName(cur);
		CXString cursKind = clang_getCursorKindSpelling(clang_getCursorKind(cur));

		disposeFileName = true;

		cursorLocation(cur, &locFileName, &locLn, &locCol);

		printf(
			"At level %zu, visiting node of kind %s named %s at %s:%u:%u.\n",
			dstate->level,
			clang_getCString(cursKind),
			clang_getCString(cursDesc),
			clang_getCString(locFileName),
			locLn, locCol
		);

		clang_disposeString(cursKind);
		clang_disposeString(cursDesc);
	}
#endif

	/* continue recursively */
	clang_visitChildren(
		cur,
		visitation,
		(CXClientData)&kiddstate
	);

	if (disposeFileName)
		clang_disposeString(locFileName);

	return CXChildVisit_Continue;
}

/**
 * Processes one file of source code.
 * @param idx the Clang index to use
 * @param filename the name of the file to process
 * @param argcount number of arguments to Clang
 * @param args aruments to Clang, or NULL if argcount is zero
 * @return EXITCODE_OK, or the exit code which should be returned after cleanup
 */
static enum exitcodes_e processFile(
	CXIndex idx,
	const char *filename,
	unsigned int argcount,
	const char * const *args
)
{
	unsigned int i;
	descent_state dstate = {
		.level = 0,
		.voidCastAbove = false,
		.compoundStmtAbove = false
	};

	/* parse! */
	CXTranslationUnit tu = clang_parseTranslationUnit(
		idx,		/* index */
		filename,	/* source filename */
		args,		/* commandline args */
		argcount,	/* commandline arg count */
		NULL,		/* unsaved files */
		0,		/* unsaved file count */
		CXTranslationUnit_None	/* flags, or lack thereof */
	);
	if (tu == NULL)
	{
		fprintf(stderr, "%s: error parsing %s\n", progname, filename);
		return EXITCODE_CLANG_FAIL;
	}

	/* loop over all diagnostics */
	for (i = 0; i < clang_getNumDiagnostics(tu); ++i)
	{
		CXDiagnostic diag = clang_getDiagnostic(tu, i);
		CXString diagStr = clang_formatDiagnostic(diag, clang_defaultDiagnosticDisplayOptions());

		/* output the diagnostic message */
		fprintf(stderr, "%s\n", clang_getCString(diagStr));

		if (clang_getDiagnosticSeverity(diag) >= CXDiagnostic_Error)
		{
			fprintf(stderr, "Aborting parse.\n");
			clang_disposeString(diagStr);
			clang_disposeDiagnostic(diag);
			clang_disposeTranslationUnit(tu);
			return EXITCODE_FILE_PARSE;
		}

		clang_disposeString(diagStr);
		clang_disposeDiagnostic(diag);
	}

	/* okay, time do to the magic */
	clang_visitChildren(
		clang_getTranslationUnitCursor(tu),
		visitation,
		(CXClientData)&dstate
	);

	clang_disposeTranslationUnit(tu);	/* with greetings to TU Wien */

	return EXITCODE_OK;
}

/**
 * Prints out a warning that it is pointless to specify the given option
 * multiple times.
 *
 * @param option the option to warn about, including the hyphen
 */
static inline void pointless(const char *option)
{
	(void)fprintf(stderr, "Warning: it is pointless to specify %s multiple times.\n", option);
}

/**
 * The main entry point of the application.
 * @param argc the number of command-line arguments
 * @param argv the array of command-line arguments
 */
int main(int argc, char **argv)
{
	int opt, i;
	bool interactive = false;
	bool extstatus = false;
	enum exitcodes_e ret = EXITCODE_OK;
#ifdef GCC_SYSINCLUDE
	bool inclgcc = true;
#endif

	msa_t clangargs;

	if (argc > 0)
	{
		progname = argv[0];
	}

	/* allocate space for clangargs */
	if (msa_create(&clangargs) == 0)
	{
		perror("msa_create");
		return EXITCODE_MM;
	}

	while ((opt = getopt(argc, argv, "D:I:is" GETOPT_G)) != -1)
	{
		switch (opt)
		{
			case 'D':
				if (msa_add_prefixed(&clangargs, "-D", optarg) == 0)
				{
					perror("msa_add");
					return EXITCODE_MM;
				}
				break;
			case 'I':
				if (msa_add_prefixed(&clangargs, "-I", optarg) == 0)
				{
					perror("msa_add");
					return EXITCODE_MM;
				}
				break;
#ifdef GCC_SYSINCLUDE
			case 'g':
				if (!inclgcc)
					pointless("-g");
				inclgcc = false;
				break;
#endif
			case 'i':
				if (interactive)
					pointless("-i");
				interactive = true;
				break;
			case 's':
				if (extstatus)
					pointless("-s");
				extstatus = true;
				break;
			case '?':
				usage();
			default:
				assert(0 && "Default case in getopt switch.");
		}
	}

	if (optind == argc)
	{
		/* no file has been specified */
		fprintf(stderr, "%s: no file specified\n", progname);
		msa_destroy(&clangargs);
		usage();
	}

#ifdef GCC_SYSINCLUDE
	/* add GCC include path */
	if (inclgcc)
	{
		if (msa_add_prefixed(&clangargs, "-I", GCC_SYSINCLUDE) == 0)
		{
			perror("msa_add");
			return EXITCODE_MM;
		}
	}
#endif

	/* fetch clang index */
	CXIndex idx = clang_createIndex(0, 0);
	if (idx == NULL)
	{
		fprintf(stderr, "%s: clang index creation failed\n", progname);
		msa_destroy(&clangargs);
		exit(EXITCODE_CLANG_FAIL);
	}

	/* process each file in turn */
	for (i = optind; i < argc; ++i)
	{
		ret = processFile(idx, argv[i], clangargs.count, (const char **)clangargs.arr);
		if (ret != EXITCODE_OK)
		{
			/* process_file already printed a diagnostic; just stop */
			break;
		}
	}

	/* clean up */
	clang_disposeIndex(idx);
	msa_destroy(&clangargs);

	return ret;
}
