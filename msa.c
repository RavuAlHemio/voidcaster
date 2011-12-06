/**
 * @file msa.c
 *
 * @author Ondřej Hošek
 *
 * @brief Magical String Array
 * @details A string array which automagically expands its contents as needed.
 *
 * @date 2011-04-12
 */

#include "msa.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

/** Default MSA capacity. */
static const int DEFAULT_CAPACITY = 2;

/** MSA capacity increment size. */
static const int CAPACITY_INCREMENT = 8;

/* utility functions */

/**
 * Compare two strings using strcmp. Useful for qsort(3), bsearch(3), et al.
 *
 * @param left Pointer to the first string (pointer-to-pointer-to-char).
 * @param right Pointer to the first string (pointer-to-pointer-to-char).
 * @return Less than zero if left ordered before right.
 *         Zero if left and right equally ordered.
 *         More than zero if right ordered before left.
 */
static int comparator_strcmp(const void *left, const void *right)
{
	return strcmp(*(char * const *)left, *(char * const *)right);
}

/* public-facing functions */

int msa_create(msa_t *msa)
{
	msa->capacity = DEFAULT_CAPACITY;
	msa->count = 0;
	msa->arr = malloc(msa->capacity * sizeof(char *));
	if (msa->arr == NULL)
	{
		/* malloc failed */
		msa->capacity = 0;
		return 0;
	}
	return 1;
}

void msa_destroy(msa_t *msa)
{
	size_t i;

	/* free all strings */
	for (i = 0; i < msa->count; ++i)
	{
		free(msa->arr[i]);
	}
	/* reset structure */
	msa->capacity = 0;
	msa->count = 0;
	free(msa->arr);
	msa->arr = NULL;
}

int msa_add(msa_t *msa, const char *str)
{
	char *dupedstr;

	if (msa->count >= msa->capacity)
	{
		/* not enough space; expand array by CAPACITY_INCREMENT */
		char **newarr;
		msa->capacity += CAPACITY_INCREMENT;
		newarr = realloc(msa->arr, msa->capacity * sizeof(char *));
		if (newarr == NULL)
		{
			/* realloc failed; return 0 and don't touch the existing array */
			return 0;
		}

		/* array reallocated successfully; store new pointer */
		msa->arr = newarr;
	}

	/* store new element */
	dupedstr = strdup(str);
	if (dupedstr == NULL)
	{
		/* strdup failed */
		return 0;
	}

	msa->arr[msa->count++] = dupedstr;
	return 1;
}

const char *msa_get(msa_t *msa, size_t idx)
{
	if (idx >= msa->count)
	{
		return NULL;
	}

	return msa->arr[idx];
}

int msa_replace(msa_t *msa, size_t idx, const char *str)
{
	char *dupedstr = strdup(str);
	if (dupedstr == NULL)
	{
		/* strdup failed */
		return 0;
	}

	/* free old and store dup'ed new string */
	free(msa->arr[idx]);
	msa->arr[idx] = dupedstr;
	return 1;
}

void msa_sort(msa_t *msa)
{
	qsort(msa->arr, msa->count, sizeof(char *), comparator_strcmp);
}

int msa_add_prefixed(msa_t *msa, const char *fst, const char *snd)
{
	int ret, olderrno;

	/* size of the new string */
	size_t newsize = strlen(fst) + strlen(snd) + 1;

	/* allocate space for it */
	char *newstr = malloc(newsize);
	if (newstr == NULL)
	{
		/* malloc failed */
		return 0;
	}

	/* concatenate strings */
	(void)snprintf(newstr, newsize, "%s%s", fst, snd);

	/* add */
	ret = msa_add(msa, newstr);
	olderrno = errno;

	/* free the concatenated strings */
	free(newstr);

	/* return */
	errno = olderrno;
	return ret;
}
