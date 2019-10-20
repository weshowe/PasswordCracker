#ifndef _TOOLBOX_H_
#define _TOOLBOX_H_

#include <stdbool.h>
/**
 * My shitty library.
 */

void clearSTDIN(); // Clears the STDIN by eating chars one at a time.

// Finds string length up to, not including the delimiter.
int strlendel(char *inputString, char delimiter);

/*
 * Converts to an integer, provided in the given range.
 * Outputs 0 if no conversion can be made.
 */
int atoiRange(char * input, int min, int max);

bool isChar(char * inputString); // Returns a char from a string. Null char on error.

int parseConfirm(char * input); // Determines if a given char is y/n (not case sensitive).

void timeConvert(unsigned long seconds);

#endif
