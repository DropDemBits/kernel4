#include <string.h>

char* strtok_r(char* str, const char* delim, char** lasts)
{
	char* token;

	if(str == NULL)
		str = *lasts;

	str += strspn(str, delim);

	if(*str == '\0')
	{
		*lasts = str;
		return NULL;
	}
	token = str++;

	while(*str)
	{
		for(const char *c = delim; *c; c++)
		{
			if(*str == *c)
				goto tokend;
		}
		str++;
	}

	*lasts = str;
	goto done;

	tokend:
	*str = '\0';
	*lasts = str + 1;

	done:
	return token;
}