/*
 * toolbox.c
 *
 *  Created on: Sep. 10, 2019
 *      Author: icarus
 */

#include <stdio.h>
#include "toolbox.h"
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>

/*
 * Clears STDIN because fflush(stdin) apparently sucks and isn't properly defined.
 */
extern void clearSTDIN()
{
	// Note to self: Find implementation of this that doesn't suck.
}

/*
 * Converts a string to an integer, provided it is in the
 * given range (inclusive). Extends atoi's functionality.
 */
extern int atoiRange(char * input, int min, int max)
{
	int i = atoi(input);

	if(i >= min && i <= max)
		return i;

	else
		return 0;

}

/**
 * Uses pointer arithmetic to find the length of a string up
 * to, but not including a delimiter.
 *
 * Not efficient, but still fun!
 */
extern int strlendel(char *inputString, char delimiter)
{
	char * test = strchr(inputString, delimiter);

	if(test != NULL)
		return (test - inputString) / sizeof(char); // We're using pointer arithmetic. Inefficient, but fun!

	else return 0;
}

/**
 * Determines if the string in question is a single char, not including a newline.
 *
 * More broad in functionality than isalpha().
 */
extern bool isChar(char * inputString)
{
	if(strlendel(inputString, '\n') == 1 || (strlen(inputString) == 1 && inputString[0] !='\n'))
		return true;

	else
		return false;
}

/*
 * Determines if input string is a confirmation y/n.
 * 1 if y, 0 if n, -1 if neither.
 *
 * Handy to have this.
 */
extern int parseConfirm(char * input)
{
	if (isChar(input) == true)
	{
		if(tolower(input[0]) == 'y')
			return 1;

		if(tolower(input[0]) == 'n')
			return 0;
	}

	return -1;
}

/*
 * Converts seconds into larger time intervals for ease of understanding.
 *
 * TODO
 *
 * Make dynamic version that can include months and years.
 */
extern void timeConvert(unsigned long seconds)
{
	/* Note to self:
	 *
	 * minutes = 60
	 * hours = 3600
	 * days = 86400
	 */

	int days = 0, hours = 0, minutes = 0;

	if(seconds % 86400 != seconds)
	{
		days = seconds / 86400;
		seconds -= 86400*days;
	}

	if(seconds % 3600 != seconds)
	{
		hours = seconds / 3600;
		seconds -= 3600*hours;
	}

	if(seconds % 60 != seconds)
	{
		minutes = seconds / 60;
		seconds -= 60*minutes;
	}

	printf("Estimated Time Remaining: %d days, %d hours, %d minutes, %d seconds\n", days, hours, minutes, (int)seconds);
}





