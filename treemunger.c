/**
 * @file treemunger.c
 *
 * @author Ondřej Hošek
 *
 * @brief C Abstract Syntax Tree--munging code
 */

#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

#include "treemunger.h"

/**
 * A structure containing the state of the descent through the AST.
 */
typedef struct
{
	/** Missing void cast callback. */
	missingVoidProc missProc;

	/** Superfluous void cast callback. */
	superfluousVoidProc superProc;

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
		.missProc = dstate->missProc,
		.superProc = dstate->superProc,
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
						dstate->superProc(
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
						dstate->missProc(
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

/* this is the big one */
enum exitcodes_e processFile(
	CXIndex idx,
	const char *filename,
	unsigned int argcount,
	const char * const *args,
	missingVoidProc missProc,
	superfluousVoidProc superProc
)
{
	unsigned int i;
	descent_state dstate = {
		.missProc = missProc,
		.superProc = superProc,
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
