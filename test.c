#include <stdio.h>

int add1(int a)
{
	return a + 1;
}

void voided(void)
{
	/* do nothing */
}

void stuff(void)
{
	add1(2);
	printf("Your head a splode.\n");
	(void)voided();
}
