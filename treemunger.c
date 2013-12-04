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

		/** The location where the cast begins. */
		module_loc_t startLoc;

		/** The location where the cast ends. */
		module_loc_t endLoc;
	} castLoc;
} descent_state;

/**
 * Stores the location information from the cursor into the parameters
 * passed by reference.
 *
 * @param cur cursor whose location to obtain
 * @param locFileName by-ref to string specifying the filename
 * @param loc by-ref to location
 */
static inline void cursorLocation(CXCursor cur, CXString *locFileName, module_loc_t *loc)
{
	unsigned int l, c;

	CXSourceLocation cloc = clang_getCursorLocation(cur);
	clang_getPresumedLocation(cloc, locFileName, &l, &c);
	loc->line = l;
	loc->col = c;
}

/**
 * Returns the extent of the cast referenced by the given cursor.
 *
 * @param cur cursor to C-style cast whose extent to obtain
 * @param start by-ref to the location where the cast begins
 * @param end by-ref to the location where the cast ends
 */
static inline void castExtent(CXCursor cur, module_loc_t *start, module_loc_t *end)
{
	CXToken *toks;
	CXCursor *curs;
	CXTranslationUnit tu = clang_Cursor_getTranslationUnit(cur);
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
			start->line = newSL;
			start->col = newSC;
			startSet = true;
		}

		end->line = newEL;
		end->col = newEC;
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

	if (curKind == CXCursor_CompoundStmt || curKind == CXCursor_CaseStmt)
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
				&kiddstate.castLoc.startLoc,
				&kiddstate.castLoc.endLoc
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
		module_loc_t loc;

		disposeFileName = true;

		cursorLocation(cur, &locFileName, &loc);

		if (
			clang_Cursor_isNull(target) ||
			clang_equalLocations(clang_getCursorLocation(cur), clang_getCursorLocation(target))
		)
		{
			/* function decl not found */
			(void)fprintf(stderr,
				"%s:%zu:%zu: Warning: can't check call to %s (can't find original definition).\n",
				clang_getCString(locFileName), loc.line, loc.col,
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
							dstate->castLoc.startLoc,
							dstate->castLoc.endLoc
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
							loc
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
		module_loc_t loc;

		CXString cursDesc = clang_getCursorDisplayName(cur);
		CXString cursKind = clang_getCursorKindSpelling(clang_getCursorKind(cur));

		disposeFileName = true;

		cursorLocation(cur, &locFileName, &loc);

		(void)printf(
			"At level %zu, visiting node of kind %s named %s at %s:%zu:%zu.\n",
			dstate->level,
			clang_getCString(cursKind),
			clang_getCString(cursDesc),
			clang_getCString(locFileName),
			loc.line, loc.col
		);

		clang_disposeString(cursKind);
		clang_disposeString(cursDesc);
	}
#endif

	/* continue recursively */
	(void)clang_visitChildren(
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
		(void)fprintf(stderr, "%s: error parsing %s\n", progname, filename);
		return EXITCODE_CLANG_FAIL;
	}

	/* loop over all diagnostics */
	for (i = 0; i < clang_getNumDiagnostics(tu); ++i)
	{
		CXDiagnostic diag = clang_getDiagnostic(tu, i);
		CXString diagStr = clang_formatDiagnostic(diag, clang_defaultDiagnosticDisplayOptions());

		/* output the diagnostic message */
		(void)fprintf(stderr, "%s\n", clang_getCString(diagStr));

		if (clang_getDiagnosticSeverity(diag) >= CXDiagnostic_Error)
		{
			(void)fprintf(stderr, "Aborting parse.\n");
			clang_disposeString(diagStr);
			clang_disposeDiagnostic(diag);
			clang_disposeTranslationUnit(tu);
			return EXITCODE_FILE_PARSE;
		}

		clang_disposeString(diagStr);
		clang_disposeDiagnostic(diag);
	}

	/* okay, time do to the magic */
	(void)clang_visitChildren(
		clang_getTranslationUnitCursor(tu),
		visitation,
		(CXClientData)&dstate
	);

	clang_disposeTranslationUnit(tu);	/* with greetings to TU Wien */

	return EXITCODE_OK;
}
