/**
 * @file treemunger.h
 *
 * @author Ondřej Hošek
 *
 * @brief C Abstract Syntax Tree--munging code
 */

#ifndef __TREEMUNGER_H__
#define __TREEMUNGER_H__

#include <clang-c/Index.h>

#include "shared.h"

/**
 * Type of callback which acts upon a missing cast to void.
 *
 * @param file the name of the file where the cast is missing
 * @param func the name of the function called
 * @param loc the location at which the cast should be inserted
 */
typedef void (*missingVoidProc)(const char *file, const char *func, module_loc_t loc);

/**
 * Type of callback which acts upon a superfluous cast to void.
 *
 * @param file the name of the file containing the cast
 * @param func the name of the function called
 * @param start the location where the cast starts
 * @param end the location where the cast ends
 */
typedef void (*superfluousVoidProc)(const char *file, const char *func, module_loc_t start, module_loc_t end);

/**
 * Processes one file of source code.
 * @param idx the Clang index to use
 * @param filename the name of the file to process
 * @param argcount number of arguments to Clang
 * @param args aruments to Clang, or NULL if argcount is zero
 * @param missProc callback if a cast to void is missing
 * @param superProc callback if a cast to void is superfluous
 * @return EXITCODE_OK, or the exit code which should be returned after cleanup
 */
enum exitcodes_e processFile(
	CXIndex idx,
	const char *filename,
	unsigned int argcount,
	const char * const *args,
	missingVoidProc missProc,
	superfluousVoidProc superProc
);

#endif
