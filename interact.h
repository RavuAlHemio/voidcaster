/**
 * @file interact.h
 *
 * @brief Interactive mode for the Voidcaster.
 *
 * @author Ondřej Hošek <ondrej.hosek@tuwien.ac.at>
 */

#ifndef __INTERACT_H__
#define __INTERACT_H__

#include "shared.h"
#include "treemunger.h"

/**
 * Prepares to interactively fix a missing void cast.
 *
 * @param file the name of the file where the cast is missing
 * @param func the name of the function called
 * @param loc the location where the cast should be inserted
 */
void interactMissingVoid(const char *file, const char *func, module_loc_t loc);

/**
 * Prepares to interactively fix a superfluous cast to void.
 *
 * @param file the name of the file containing the cast
 * @param func the name of the function called
 * @param start the location where the cast starts
 * @param end the location where the cast ends
 */
void interactSuperfluousVoid(const char *file, const char *func, module_loc_t start, module_loc_t end);

/**
 * Disposes of all modifications. Call to clean up.
 */
void disposeModifs(void);

/**
 * Performs the queued moficiations. Call after completing AST traversal.
 */
void performModifs(void);

#endif
