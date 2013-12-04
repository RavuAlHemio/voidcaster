#include <stdio.h>

/* non-prototype function declaration */
void not_a_prototype_v();
int not_a_prototype_i();

void i_return_nothing(void)
{
}

void this_one_is_correct(void)
{
	int charz;

	/* none of these calls require modification */
	if (printf("lol\n") == 4)
	{
		(void)printf("yay!\n");
	}

	not_a_prototype_v(69);

	while (printf("what\n") == 42 && 1 == 3)
	{
		(void)printf("this never happens...\n");
	}

	for (charz = printf("\n"); charz < printf("abc\n"); ++charz)
	{
		/* yeah, that was the dumbest use ever... */
		(void)printf("... but it works!\n");
	}

	charz = printf("characters?\n");

	switch (charz)
	{
	case 1:
		printf("one\n");
		break;
	case 2:
		(void)printf("two\n");
		break;
	}
}

void this_one_is_wrong(void)
{
	int i;

	/* this call is to an undeclared function */
	call_to_undeclared_function();

	/* these calls require casting to void */

	/* this call is to an unprototyped function */
	not_a_prototype_i(1, 2, 3);

	/* directly in a function */
	printf("This is a test.\n");

	if (1 > 0)
	{
		/* multiple calls in a compound statement (if) */
		printf("first\n");
		printf("second\n");
	}

	/* abuse the condition field */
	for (i = 0; printf("omg\n"), i < 12; ++i)
	{
		/* do nothing, but do something anyway! */
	}

	/* finally, a pointless cast to void */
	(void)i_return_nothing();

	/* and the same, split over multiple lines */
	(
		/* we interrupt this cast to bring you breaking news */
		void
		/* sorry, false alarm */
	)

	/* try to fill the spaces in between */

	i_return_nothing();

	/* okay, I went overboard with the comments. */
	(



	void






	)i_return_nothing();
}
