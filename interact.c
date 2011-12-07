/**
 * @file interact.c
 *
 * @brief Interactive mode for the Voidcaster.
 *
 * @author Ondřej Hošek <ondrej.hosek@tuwien.ac.at>
 */
#include <stdbool.h>
#include <stdlib.h>

#include "interact.h"

/** The modifications to be performed in interactive mode. */
static modifn_t *modifs = NULL;

/** The number of modifications. */
static size_t numModifs = 0;

/**
 * Adds a modification to the array of modifications.
 * @param toadd the modification to add
 * @return whether the add was successful
 */
bool addModifn(modifn_t toadd)
{
	modifn_t *newModifs;

	newModifs = realloc(modifs, (numModifs + 1) * sizeof(modifn_t));
	if (newModifs == NULL)
	{
		/* that went belly-up */
		perror("realloc");
		return false;
	}

	modifs = newModifs;

	modifs[numModifs++] = toadd;

	return true;
}

void interactMissingVoid(const char *file, const char *func, module_loc_t loc)
{
	printf(
		"File %s, line %zu:\n"
		"Missing cast to void when calling function '%s'.\n"
		"The line, currently:\n"
		"%s\n"
		"The line, after its modification:\n"
		"%s\n"
		"Apply fix? (y/n) ",
		file, loc.line, func, "FIXME", "FIX(void)ME"
	);
	fflush(stdout);
}

void interactSuperfluousVoid(const char *file, const char *func, module_loc_t start, module_loc_t end)
{
	printf(
		"File %s, lines %zu through %zu:\n"
		"Superfluous cast to void when calling function '%s'.\n"
		"The lines, currently:\n"
		"%s\n"
		"The lines, after their modification:\n"
		"%s\n"
		"Apply fix? (y/n) ",
		file, start.line, end.line, func, "(void)FIXME", "FIXME"
	);
}
