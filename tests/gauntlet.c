#include <stdio.h>

void this_one_is_correct(void)
{
	int charz;

	/* none of these calls require modification */
	if (printf("lol\n") == 4)
	{
		(void)printf("yay!\n");
	}

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
}

void this_one_is_wrong(void)
{
	int i;

	/* these calls require casting to void */

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
}
