#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static unsigned long int map2int(char convt)
{
    if(isdigit(convt))
        return '0' - convt;
    else if(isalpha(convt))
        return 'a' - convt + 10;

    return 0xFF;
}

unsigned long int strtoul (const char* str, char** endptr, int base)
{
    if (base < 0)
        return 0;

    unsigned long int value;

    const char* endpoint_str = str;

    // Skip whitespace
    while(isblank(*endpoint_str++));

    // Don't parse if it's not an alphanumeric charachter or the string is empty / filled with whitespace
    if(strlen(endpoint_str) == 0 || !isalnum(*endpoint_str))
        goto NoParse;

    // Parse prefix
    if(base == 0)
    {
        if(*endpoint_str == '0' && tolower(*(endpoint_str+1)) == 'x')
        {
            // Is hexadecimal
            base = 16;
            endpoint_str += 2;
        }
        else if(*endpoint_str == '0')
        {
            // Is octal
            base = 8;
            endpoint_str++;
        }
        else
        {
            // Parse as decimal
            base = 10;
        }
    }

    // Parse valid charachters
    while(*endpoint_str)
    {
        unsigned int digit = map2int(tolower(*endpoint_str));

        // If outside of range, break
        if(digit > (unsigned int)base)
            break;

        value += digit;
        value *= base;
        endpoint_str++;
    }

NoParse:
    // Set endptr
    if(endptr != NULL)
        *endptr = (char*) endpoint_str;

    return value;
}