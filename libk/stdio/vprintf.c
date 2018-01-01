#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>

static void print(const char* data, size_t data_length)
{
    for(size_t i = 0; i < data_length; i++)
        putchar((int) ((const unsigned char*) data)[i]);
}

int vprintf(const char* format, va_list params)
{
    size_t amount;
    bool rejected_formater = false;
    bool pound = false;
    int length = 0;
    int written = 0;
    char buffer[65];
    size_t index = 0;

    while(*format)
    {
        if(*format != '%')
        {
            printc:
                amount = 1;
                while(format[amount] && format[amount] != '%')
                    amount++;
                print(format, amount);
                written += amount;
                format += amount;
                continue;
        }

        if(*(++format) == '%')
            goto printc;

        const char* format_start = format;

        if(rejected_formater)
        {
            bad_parsing:
                rejected_formater = true;
                format = format_start;
                goto printc;
        }

        //Format parsing
        if(*format == '#')
        {
            pound = true;
            format++;
        }

        //length
        if(*format == 'l' || *format == 'h')
        {
            while(*format == 'h')
            {
                length--;
                format++;
            }

            if(length != 0)
                goto bad_formatting;

            while(*format == 'l')
            {
                length++;
                format++;
            }
        }

        //Specifiers
        if(*format == 'c')
        {
            format++;
            char c = (char) va_arg(params, int /* Gets promoted to char*/);
            print(&c, sizeof(c));
        }
        else if(*format == 's')
        {
            format++;
            const char* s = va_arg(params, const char*);
            print(s, strlen(s));
        }
        else if(*format == 'd' || *format == 'i')
        {
            //Signed integer
            long long number;
            switch(length)
            {
            case 1:
                number = va_arg(params, long);
                break;
            case 2:
                number = va_arg(params, long long);
                break;
            default:
                number = va_arg(params, int);
                break;
            }

            amount += strlen(lltoa(number, buffer, 10));
            print(buffer, strlen(buffer));
            format++;
        }
        else if(*format == 'u')
        {
            //Unsigned integer
            unsigned long long number;
            switch(length)
            {
            case 1:
                number = va_arg(params, unsigned long);
                break;
            case 2:
                number = va_arg(params, unsigned long long);
                break;
            default:
                number = va_arg(params, unsigned int);
                break;
            }

            amount += strlen(ulltoa(number, buffer, 10));
            print(buffer, strlen(buffer));
            format++;
        }
        else if(*format == 'x')
        {
            //Hex (lowercase)
            unsigned long long number;
            switch(length)
            {
            case 1:
                number = va_arg(params, unsigned long);
                break;
            case 2:
                number = va_arg(params, unsigned long long);
                break;
            default:
                number = va_arg(params, unsigned int);
                break;
            }

            if(pound) print("0x", strlen("0x"));

            amount += strlen(ulltoa(number, buffer, 16));
            for(size_t i = 0; i < strlen(buffer); i++)
                buffer[i] = tolower(buffer[i]);
            print(buffer, strlen(buffer));
            format++;
        }
        else if(*format == 'X')
        {
            //Hex (uppercase)
            unsigned long long number;
            switch(length)
            {
            case 1:
                number = va_arg(params, unsigned long);
                break;
            case 2:
                number = va_arg(params, unsigned long long);
                break;
            default:
                number = va_arg(params, unsigned int);
                break;
            }

            if(pound) print("0X", strlen("0X"));

            amount += strlen(ulltoa(number, buffer, 16));
            print(buffer, strlen(buffer));
            format++;
        }
        else {
        bad_formatting:
            length = 0;
            amount = 0;
            pound = false;
            rejected_formater = true;
            goto bad_parsing;
        }

        length = 0;
        written += amount;
        amount = 0;
        pound = false;
    }

    return written;
}
