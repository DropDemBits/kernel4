#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

int atoi(const char* str)
{
	// Skip whitespace
	while(*str)
	{
		if(*str == '\0') return 0;
		if(!isspace(*str)) break;
		str++;
	}

	int retval = 0;
	bool is_negative = false;

	while(*str)
	{
		if(*str == '\0') return 0;
		
		if(*str == '-')
		{
			if(is_negative) break;
			is_negative = true;
			goto skip;
		} else if(!isdigit(*str))
		{
			break;
		}

		retval *= 10;
		retval += *str - '0';

		skip:
		str++;
	}

	if(is_negative)
	{
		if(retval < 0)
			// Overflow
			return -1;
		retval *= -1;
	}
	return retval;
}