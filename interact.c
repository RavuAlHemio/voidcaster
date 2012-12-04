/**
 * @file interact.c
 *
 * @brief Interactive mode for the Voidcaster.
 *
 * @author Ondřej Hošek <ondrej.hosek@tuwien.ac.at>
 */

#include <stdbool.h>
#include <stdlib.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "interact.h"

/** A modification to be performed on the code. */
typedef struct modif_s
{
	/** The file to modify. */
	char *file;

	/** The type of the modification. */
	enum
	{
		/** Insert text at a given location. */
		MODIF_INSERT,

		/** Remove text between two given locations. */
		MODIF_REMOVE
	} type;

	/** The pseudopolymorphic union for the types of modification. */
	union
	{
		/** The value of an insertion node. */
		struct
		{
			/** The location where to insert an element. */
			module_loc_t where;

			/** The text to insert. */
			char *what;
		} insert;

		/** The value of a removal node. */
		struct
		{
			/** The inclusive location where the removal starts. */
			module_loc_t fromWhere;

			/** The inclusive location where the removal ends. */
			module_loc_t toWhere;
		} remove;
	} m;
} modif_t;

/** The modifications to be performed in interactive mode. */
static modif_t *modifs = NULL;

/** The number of modifications. */
static size_t numModifs = 0;

/**
 * Renames a file, copying-and-deleting if the rename fails.
 *
 * @param oldpath The current path to the file to rename.
 * @param newpath The future path to the file to rename.
 * @return 0 on success, -1 on failure. In the latter case,
 * errno is set as well.
 */
static int robustRename(const char *oldpath, const char *newpath)
{
	FILE *rf, *wf;
	char buf[256];
	size_t rb, wb;

	/* try "normal" rename first */
	if (rename(oldpath, newpath) == 0)
	{
		/* worked! */
		return 0;
	}

#if 0
	if (errno != EXDEV)
	{
		/* not a cross-device move; bail out */
		return -1;
	}
#endif

	/* time to do this the crazy way */
	rf = fopen(oldpath, "rb");
	if (rf == NULL)
	{
		return -1;
	}

	wf = fopen(newpath, "wb");
	if (wf == NULL)
	{
		(void)fclose(rf);
		return -1;
	}

	do
	{
		rb = fread(buf, sizeof(buf[0]), sizeof(buf)/sizeof(buf[0]), rf);
		wb = fwrite(buf, sizeof(buf[0]), rb, wf);

		if (rb != wb)
		{
			/* wrote fewer bytes than read -- something went wrong */
			(void)fclose(rf);
			(void)fclose(wf);
			return -1;
		}
	}
	while (rb > 0);

	if (ferror(rf) || ferror(wf))
	{
		/* something went wrong during the loop */
		(void)fclose(rf);
		(void)fclose(wf);
		return -1;
	}

	if (fclose(wf) == EOF)
	{
		(void)fclose(rf);
		return -1;
	}
	if (fclose(rf) == EOF)
	{
		return -1;
	}

	/* now remove the original file */
	return unlink(oldpath);
}

/**
 * Returns the characteristic location of the given modification.
 *
 * @param mod pointer to modification whose location to return
 */
static inline module_loc_t modifCharacteristicLoc(const modif_t *mod)
{
	switch (mod->type)
	{
		case MODIF_INSERT:
			return mod->m.insert.where;
		case MODIF_REMOVE:
			return mod->m.remove.fromWhere;
		default:
			break;
	}
	assert(0 && "Unknown modification location.");
	abort();
}

/**
 * Compares two modifications. Useful for qsort(3), bsearch(3),
 * etc.
 *
 * @param left the left modification to compare
 * @param right the right modification to compare
 * @return a number less than zero if left is ordered before right,
 * zero if left and right are equally ordered,
 * a number above zero if left is ordered after right
 */
static int compareModifs(const void *left, const void *right)
{
	const modif_t *l = (const modif_t *)left;
	const modif_t *r = (const modif_t *)right;
	module_loc_t lLoc, rLoc;

	/* compare by filename first */
	int fncmp = strcmp(l->file, r->file);
	if (fncmp != 0)
		return fncmp;

	/* then by characteristic location */
	lLoc = modifCharacteristicLoc(l);
	rLoc = modifCharacteristicLoc(r);

	if (lLoc.line < rLoc.line)
	{
		return -1;
	}
	else if (lLoc.line > rLoc.line)
	{
		return 1;
	}
	/* int is probably smaller than size_t; */
	/* subtraction may lead to bogus results */
	else if (lLoc.col < rLoc.col)
	{
		return -1;
	}
	else if (lLoc.col > rLoc.col)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

/**
 * Adds a modification to the array of modifications.
 * @param toadd the modification to add
 * @return whether the add was successful
 */
static bool addModif(modif_t toadd)
{
	modif_t *newModifs;

	newModifs = realloc(modifs, (numModifs + 1) * sizeof(modif_t));
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
 * Disposes of all modifications.
 */
void disposeModifs(void)
{
	size_t i;

	for (i = 0; i < numModifs; ++i)
	{
		switch (modifs[i].type)
		{
			case MODIF_INSERT:
				free(modifs[i].m.insert.what);
				break;
			case MODIF_REMOVE:
				break;
		}

		free(modifs[i].file);
	}

	free(modifs);
	modifs = NULL;
}

/**
 * Obtains a boolean response from standard input.
 *
 * @return the response
 */
static bool fetchBoolResponse(void)
{
	char respbuf[16];

	while (true)
	{
		if (fgets(respbuf, sizeof(respbuf), stdin) == NULL)
		{
			/* exit without pomp */
			(void)printf("Okay, exiting.\n");
			exit(EXITCODE_OK);
		}

		if (respbuf[0] != '\0' && respbuf[1] == '\n' && respbuf[2] == '\0')
		{
			switch (respbuf[0])
			{
				case 'y':
				case 'Y':
					return true;
				case 'n':
				case 'N':
					return false;
				default:
					break;
			}
		}

		(void)printf("Please answer y (yes) or n (no): ");
		(void)fflush(stdout);
	}
}

/**
 * Fetches one or more lines of text from the given file.
 *
 * On error, the pointer passed in lines will equal NULL and
 * the line length will be zero.
 *
 * @param file the name of the file
 * @param linenum the 1-based index to the first line to return
 * @param linecount the number of lines to return
 * @param lines will point to requested lines of text; don't forget to free()
 * it when you're done!
 * @param lineslen will contain the length of the lines together
 */
static void fetchFileLines(const char *file, size_t linenum, size_t linecount, char **lines, size_t *lineslen)
{
	int fd;
	char *f;
	off_t i, filelen, lnstart;
	size_t ourline = 1;	/*< we're on this line */
	size_t runlncount;

	assert(linecount > 0);

	*lineslen = 0;

	/* open the file */
	fd = open(file, O_RDONLY | O_CLOEXEC);
	if (fd == -1)
	{
		perror("open");
		*lines = NULL;
		*lineslen = 0;
		return;
	}

	/* find its length */
	filelen = lseek(fd, 0, SEEK_END);
	if (filelen == (off_t)-1)
	{
		perror("lseek");
		(void)close(fd);
		*lines = NULL;
		*lineslen = 0;
		return;
	}

	if (lseek(fd, 0, SEEK_SET) == (off_t)-1)
	{
		perror("lseek");
		(void)close(fd);
		*lines = NULL;
		*lineslen = 0;
		return;
	}

	/* map it into memory */
	f = mmap(NULL, filelen, PROT_READ, MAP_PRIVATE, fd, 0);
	if (f == MAP_FAILED)
	{
		perror("mmap");
		(void)close(fd);
		*lines = NULL;
		*lineslen = 0;
		return;
	}

	/* munge lines */
	for (i = 0; i < filelen; ++i)
	{
		if (f[i] == '\n')
		{
			if ((++ourline) == linenum)
			{
				/* this is the line where to start. */
				break;
			}
		}
	}

	if (i >= filelen)
	{
		/* line past end of source file O_o */
		(void)fprintf(stderr, "Line %zu past end of source file %s.\n", linenum, file);
		(void)munmap(f, filelen);
		(void)close(fd);
		*lines = NULL;
		*lineslen = 0;
		return;
	}

	/* skip past the newline we're currently on */
	++i;

	/* save starting index */
	lnstart = i;

	/* how long are the lines to return? */
	*lineslen = 0;
	runlncount = 1;
	while (i < filelen)
	{
		if (f[i] == '\n')
		{
			++runlncount;
			if (runlncount > linecount)
			{
				break;
			}
		}

		++(*lineslen);
		++i;
	}

	/* allocate space for the lines */
	*lines = malloc(((*lineslen) + 1)*sizeof(char));
	if (*lines == NULL)
	{
		perror("malloc");
		(void)munmap(f, filelen);
		(void)close(fd);
		*lines = NULL;
		*lineslen = 0;
		return;
	}

	/* copy the lines */
	(void)memcpy(*lines, &f[lnstart], *lineslen);
	/* NUL-terminate */
	(*lines)[*lineslen] = '\0';

	/* clean up */
	if (munmap(f, filelen) != 0)
	{
		perror("munmap");
	}
	if (close(fd) != 0)
	{
		perror("close");
	}

	/* it worked out :-) */
}

void interactMissingVoid(const char *file, const char *func, module_loc_t loc)
{
	char *line = NULL;
	size_t linelen = 0;

	/* fetch the line */
	fetchFileLines(file, loc.line, 1, &line, &linelen);

	if (line == NULL)
	{
		/* crud. */
		line = strdup("");
	}

	(void)printf(
		"\n"
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
	(void)fflush(stdout);

	if (fetchBoolResponse())
	{
		/* queue the fix */
		modif_t newFix = {
			.file = strdup(file),
			.type = MODIF_INSERT,
			.m = {
				.insert = {
					.where = loc,
					.what = strdup("(void)")
				}
			}
		};

		if (!addModif(newFix))
		{
			/* it dieded :'-( */
			exit(EXITCODE_MM);
		}
	}

	free(line);
}

void interactSuperfluousVoid(const char *file, const char *func, module_loc_t start, module_loc_t end)
{
	char *lines;
	size_t lineslen;
	size_t linecount = end.line - start.line + 1;

	size_t startOffset = start.col - 1;
	size_t myLine, myCol, endOffset;

	/* fetch the lines */
	fetchFileLines(file, start.line, linecount, &lines, &lineslen);

	/* find the offset where the cast ends */
	myLine = start.line;
	myCol = startOffset + 1;
	for (endOffset = startOffset; endOffset < lineslen; ++endOffset)
	{
		if (myLine == end.line && myCol == end.col)
		{
			break;
		}

		/* increment */
		if (lines[endOffset] == '\n')
		{
			++myLine;
			myCol = 1;
		}
		else
		{
			++myCol;
		}
	}

	/* editors number characters from 1, C from 0 */
	--endOffset;

	(void)printf(
		"\n"
		"File %s, lines %zu through %zu:\n"
		"Superfluous cast to void when calling function '%s'.\n"
		"The lines, currently:\n"
		"%s\n"
		"The lines, after their modification:\n"
		"%.*s%s\n"
		"Apply fix? (y/n) ",
		file, start.line, end.line, func, lines,
		(int)startOffset, lines, lines + endOffset + 1
	);
	(void)fflush(stdout);

	if (fetchBoolResponse())
	{
		/* queue the fix */
		modif_t newFix = {
			.file = strdup(file),
			.type = MODIF_REMOVE,
			.m = {
				.remove = {
					.fromWhere = start,
					.toWhere = end
				}
			}
		};

		if (!addModif(newFix))
		{
			/* it dieded :'-( */
			exit(EXITCODE_MM);
		}
	}

	if (lineslen != 0)
		free(lines);
}

/**
 * Copies the rest of the first file into the other.
 *
 * @param from the file to copy from; must be opened in read mode
 * @param to the file to copy to; must be opened in write mode
 */
static inline void copyRest(FILE *from, FILE *to)
{
	int c;
	while ((c = fgetc(from)) != EOF)
	{
		if (fputc(c, to) == EOF)
		{
			perror("fputc");
			return;
		}
	}
}

/**
 * Overwrites the destination file with the source file, creating a backup copy
 * of the destination file (with the path of the original followed by a tilde)
 * beforehand.
 *
 * If the backup file already exists, it is shamelessly overwritten.
 *
 * @param oldP the path to the destination file
 * @param newP the path to the source file
 */
static inline void overwriteWithBackup(const char *oldP, const char *newP)
{
	/* make a backup copy of the read file */
	char *backFn = malloc(strlen(oldP) + 2);
	if (backFn == NULL)
	{
		perror("malloc");
		return;
	}

	(void)snprintf(backFn, strlen(oldP)+2, "%s~", oldP);

	if (robustRename(oldP, backFn) != 0)
	{
		perror("rename");
		free(backFn);
		return;
	}

	free(backFn);

	/* replace the read file with the write file */
	if (robustRename(newP, oldP) != 0)
	{
		perror("rename");
		return;
	}
}

/**
 * Fast-forwards the reading file from the current location in a code module to
 * another. The stream is assumed to be positioned right before the coordinates
 * given by the current location, and will be positioned right before the
 * coordinates given by the target location.
 * Due to the "right before" mode of operation, the initial position of a file
 * can be considered line 1 column 1.
 *
 * If a writing file is specified, the data will be copied from the reading file
 * as it is read.
 *
 * @param rf the reading file to move
 * @param now the current location of the file; rf must be positioned right
 * before these coordinates
 * @param target the location to move to; rf will be positioned right before
 * these coordinates
 *
 * @return true on success, false on failure
 */
static bool moveFileUntil(FILE *rf, module_loc_t now, module_loc_t target, FILE *wf)
{
	int c;

	while (now.line < target.line || (now.line == target.line && now.col < target.col))
	{
		c = fgetc(rf);
		if (c == EOF)
		{
			/* reached end-of-file... */
			return false;
		}

		if (wf != NULL)
		{
			/* copy! */
			if (fputc(c, wf) == EOF)
			{
				perror("fputc");
				return false;
			}
		}

		if (c == '\n')
		{
			/* reset counter to the next line */
			++now.line;
			now.col = 1;
		}
		else
		{
			/* go ahead */
			++now.col;
		}
	}

	/* nailed it! */
	return true;
}

void performModifs(void)
{
	/* This is where the fun happens. */
	const char *rFn = "", *wFn = "";
	FILE *rf = NULL, *wf = NULL;
	size_t i;
	char tmpfn[22];	/* /tmp/voidcasterXXXXXX */
	int tmpfd;
	module_loc_t curLoc = {
		.line = 1,
		.col = 1
	};

	if (numModifs == 0)
	{
		/* nothing to do */
		return;
	}

	/* first, sort the modifications */
	qsort(modifs, numModifs, sizeof(*modifs), compareModifs);

	for (i = 0; i < numModifs; ++i)
	{
		/* is this still the same file? */
		if (strcmp(rFn, modifs[i].file) != 0)
		{
			/* nope */

			/* copy the rest of the file untouched */
			if (rf != NULL && wf != NULL)
				copyRest(rf, wf);

			/* close both files in any case if needed */
			if (rf != NULL)
				fclose(rf);
			if (wf != NULL)
				fclose(wf);

			/* if all was well... */
			if (rf != NULL && wf != NULL)
			{
				/* replace read file with write file */
				overwriteWithBackup(rFn, wFn);
			}

			/* open the new file for reading */
			rFn = modifs[i].file;
			rf = fopen(rFn, "r");
			if (rf == NULL)
			{
				perror("fopen");
				return;
			}

			/* make a temp file for the output */
			(void)snprintf(tmpfn, sizeof(tmpfn)/sizeof(tmpfn[0]), "/tmp/voidcasterXXXXXX");
			tmpfd = mkstemp(tmpfn);
			if (tmpfd == -1)
			{
				perror("mkstemp");
				(void)fclose(rf);
				return;
			}

			/* update the write filename */
			wFn = tmpfn;

			/* pack the FD into wf */
			wf = fdopen(tmpfd, "w");
			if (wf == NULL)
			{
				perror("fdopen");
				(void)close(tmpfd);
				(void)fclose(rf);
				return;
			}

			/* reset location */
			curLoc.line = 1;
			curLoc.col = 1;

			/* we're ready to rumble */
		}

		/* now, to the overrated replacement algorithm */

		/* what type of modification is it? */
		switch (modifs[i].type)
		{
			case MODIF_INSERT:
				/* copy until given point */
				if (!moveFileUntil(rf, curLoc, modifs[i].m.insert.where, wf))
				{
					/* crud. */
					(void)fprintf(stderr, "I/O troubles...\n");
					(void)fclose(rf);
					(void)fclose(wf);
					return;
				}
				/* write out the string to insert */
				(void)fprintf(wf, "%s", modifs[i].m.insert.what);
				/* update current location */
				curLoc = modifs[i].m.insert.where;
				break;
			case MODIF_REMOVE:
				/* copy until starting point */
				if (!moveFileUntil(rf, curLoc, modifs[i].m.remove.fromWhere, wf))
				{
					/* crud. */
					(void)fprintf(stderr, "I/O troubles...\n");
					(void)fclose(rf);
					(void)fclose(wf);
					return;
				}
				/* skip until the end point */
				if (!moveFileUntil(rf, modifs[i].m.remove.fromWhere, modifs[i].m.remove.toWhere, NULL))
				{
					/* crud. */
					(void)fprintf(stderr, "I/O troubles...\n");
					(void)fclose(rf);
					(void)fclose(wf);
					return;
				}
				/* update current location */
				curLoc = modifs[i].m.remove.toWhere;
				break;
		}
	}

	/* copy the rest of the last file and close both */
	if (rf != NULL && wf != NULL)
		copyRest(rf, wf);
	if (rf != NULL)
		(void)fclose(rf);
	if (wf != NULL)
		(void)fclose(wf);

	/* replace read file with write file one last time */
	overwriteWithBackup(rFn, wFn);
}
