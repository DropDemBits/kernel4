#include <ctype.h>
#include <string.h>

size_t strspn(const char* str, const char* delim)
{
	size_t count = 0;

	while(*str)
	{
		const char *c;
		for(c = delim; *c; c++)
		{
			if(*str == *c)
			{
				count++;
				break;
			}
		}
		if(*c == '\0') return count;

		str++;
	}

	return count;
}
