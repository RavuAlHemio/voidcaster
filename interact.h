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

/** A modification to be performed on the code. */
typedef struct modifn_s
{
	/** The type of the modification. */
	enum
	{
		/** Insert text at a given location. */
		MODIFN_INSERT,

		/** Remove text between two given locations. */
		MODIFN_REMOVE
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
			const char *what;
		} insert;

		/** The value of a removal node. */
		struct
		{
			/** The inclusive location where the removal starts. */
			module_loc_t fromWhere;

			/** The inclusive location where the removal ends. */
			module_loc_t toWhere;
		} remove;
	};
} modifn_t;

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

#endif
