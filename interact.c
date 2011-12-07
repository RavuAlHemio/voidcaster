/**
 * @file interact.c
 *
 * @brief Interactive mode for the Voidcaster.
 *
 * @author Ondřej Hošek <ondrej.hosek@tuwien.ac.at>
 */
#include <stdbool.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "interact.h"

/** The modifications to be performed in interactive mode. */
static modifn_t *modifs = NULL;

/** The number of modifications. */
static size_t numModifs = 0;

/** The source file. */
static char *src = NULL;

/** The length of the source file. */
static size_t srcLen = 0;

/**
 * Adds a modification to the array of modifications.
 * @param toadd the modification to add
 * @return whether the add was successful
 */
static bool addModifn(modifn_t toadd)
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

/**
 * Fetches a line of text from the given file.
 *
 * On error, the pointer passed in line will equal NULL and
 * the line length will be zero.
 *
 * @param file the name of the file
 * @param linenum the 1-based index to the line in the file
 * @param line will point to requested line of text; don't forget to free()
 * it when you're done!
 * @param linelen will contain the length of the line
 */
static void fetchFileLine(const char *file, size_t linenum, char **line, size_t *linelen)
{
	int fd;
	char *f;
	off_t filelen;
	size_t ourline = 1;	/*< we're on this line */
	size_t i, lnstart;

	*linelen = 0;

	/* open the file */
	fd = open(file, O_RDONLY | O_CLOEXEC);
	if (fd == -1)
	{
		perror("open");
		*line = NULL;
		*linelen = 0;
		return;
	}

	/* find its length */
	filelen = lseek(fd, 0, SEEK_END);
	if (filelen == (off_t)-1)
	{
		perror("lseek");
		close(fd);
		*line = NULL;
		*linelen = 0;
		return;
	}

	if (lseek(fd, 0, SEEK_SET) == (off_t)-1)
	{
		perror("lseek");
		close(fd);
		*line = NULL;
		*linelen = 0;
		return;
	}

	/* map it into memory */
	f = mmap(NULL, filelen, PROT_READ, MAP_PRIVATE, fd, 0);
	if (f == MAP_FAILED)
	{
		perror("mmap");
		close(fd);
		*line = NULL;
		*linelen = 0;
		return;
	}

	/* munge lines */
	for (i = 0; i < filelen; ++i)
	{
		if (f[i] == '\n')
		{
			if ((++ourline) == linenum)
			{
				/* this is the line. */
				break;
			}
		}
	}

	if (i >= filelen)
	{
		/* line past end of source file O_o */
		fprintf(stderr, "Line %zu past end of source file %s.\n", linenum, file);
		munmap(f, filelen);
		close(fd);
		*line = NULL;
		*linelen = 0;
		return;
	}

	/* skip past the newline we're currently on */
	++i;

	/* save starting index */
	lnstart = i;

	/* how long is this line? */
	*linelen = 0;
	while (i < filelen)
	{
		if (f[i] == '\n')
			break;

		++(*linelen);
		++i;
	}

	/* allocate space for the line */
	*line = malloc(((*linelen) + 1)*sizeof(char));
	if (*line == NULL)
	{
		perror("malloc");
		munmap(f, filelen);
		close(fd);
		*line = NULL;
		*linelen = 0;
		return;
	}

	/* copy the line */
	memcpy(*line, &f[lnstart], *linelen);
	/* NUL-terminate */
	(*line)[*linelen] = '\0';

	/* clean up */
	munmap(f, filelen);
	close(fd);

	/* it worked out :-) */
}

void interactMissingVoid(const char *file, const char *func, module_loc_t loc)
{
	char *line;
	size_t linelen;

	/* fetch the line */
	fetchFileLine(file, loc.line, &line, &linelen);

	if (line == NULL)
	{
		/* crud. */
		line = "";
	}

	printf(
		"File %s, line %zu:\n"
		"Missing cast to void when calling function '%s'.\n"
		"The line, currently:\n"
		"%s\n"
		"The line, after its modification:\n"
		"%.*s(void)%s\n"
		"Apply fix? (y/n) ",
		file, loc.line,
		func,
		line,
		(int)loc.col-1, line, line + loc.col-1
	);
	fflush(stdout);
	fgetc(stdin);	/* TODO */

	if (linelen != 0)
		free(line);
}

void interactSuperfluousVoid(const char *file, const char *func, module_loc_t start, module_loc_t end)
{
	char **lines;
	size_t *linelens;
	size_t linecount = end.line - start.line + 1;

	/* fetch the line */
	fetchFileLine(file, loc.line, &line, &linelen);

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
