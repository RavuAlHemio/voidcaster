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
#include <assert.h>
#include <unistd.h>

#include <clang-c/Index.h>

#include "msa.h"
#include "treemunger.h"

/* make getopt accept -g option iff a system include path was specified */
#ifdef GCC_SYSINCLUDE
#define GETOPT_G "g"
#else
#define GETOPT_G ""
#endif

/** Has there been a suggestion? */
static bool suggest = false;

/* initialize here */
const char *progname = "<not set>";

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
		"Report voidcaster bugs on the home page.\n"
		"voidcaster home page: http://github.com/RavuAlHemio/voidcaster\n",
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
		ret = processFile(
			idx,
			argv[i],
			clangargs.count,
			(const char **)clangargs.arr,
			actUponMissingVoid,
			actUponSuperfluousVoid
		);

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
