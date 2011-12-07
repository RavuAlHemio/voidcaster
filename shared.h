/**
 * @file shared.h
 *
 * @author Ondřej Hošek
 *
 * @brief Data structures shared by multiple modules.
 */

#ifndef __SHARED_H__
#define __SHARED_H__

#include <stdlib.h>

/** The name of the running binary, taken from argv[0]. */
extern const char *progname;

/** A location in a code module. */
typedef struct module_loc_s
{
	/** The line in the module. */
	size_t line;

	/** The column in the line. */
	size_t col;
} module_loc_t;

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

#endif
