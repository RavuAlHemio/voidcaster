/**
 * @file msa.h
 *
 * @author Ondřej Hošek
 *
 * @brief Magical String Array
 * @details A string array which automagically expands its contents as needed.
 *
 * @date 2011-04-12
 */

#ifndef __MSA_H__
#define __MSA_H__

#include <stdlib.h>

/** The Magical String Array structure. */
typedef struct
{
	/** How many items can the array house? */
	size_t capacity;

	/** How many items is the array housing right now? */
	size_t count;

	/** The string array itself. */
	char **arr;
} msa_t;

/**
 * Create an empty Magical String Array.
 *
 * @param msa Pointer to fill with a MSA structure.
 * @return 1 on success, 0 on failure (setting errno appropriately).
 */
int msa_create(msa_t *msa);

/**
 * Destroy a Magical String Array.
 *
 * @param msa Pointer to a MSA structure.
 */
void msa_destroy(msa_t *msa);

/**
 * Add a duplicate of a string to the end of a Magical String Array.
 *
 * @param msa Pointer to a MSA structure.
 * @param str String whose duplicate is to be appended to the MSA.
 * @return 1 on success, 0 on failure (setting errno appropriately).
 */
int msa_add(msa_t *msa, const char *str);

/**
 * Obtain a specific element in the Magical String Array.
 *
 * @param msa Pointer to a MSA structure.
 * @param idx Index to fetch from.
 */
const char *msa_get(msa_t *msa, size_t idx);

/**
 * Replace a specific string in the Magical String Array with another one.
 *
 * @param msa Pointer to a MSA structure.
 * @param idx Index to replace at.
 * @param str String whose duplicate will replace the element.
 * @return 1 on success, 0 on failure (setting errno appropriately).
 */
int msa_replace(msa_t *msa, size_t idx, const char *str);

/**
 * Sort the Magical String Array using qsort(3) and strcmp(3).
 *
 * @param msa Pointer to a MSA structure.
 */
void msa_sort(msa_t *msa);

/**
 * Add the concatenation of two strings to the end of a Magical String Array.
 *
 * @param msa Pointer to a MSA structure.
 * @param fst The first part of the string to add.
 * @param snd The second part of the string to add.
 * @return 1 on success, 0 on failure (setting errno appropriately).
 */
int msa_add_prefixed(msa_t *msa, const char *fst, const char *snd);

#endif
