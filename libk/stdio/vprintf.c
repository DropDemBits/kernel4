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
        if(*format == 'l')
        {
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
            size_t number;
            if(length == 1) number = va_arg(params, int32_t);
            else if(length == 2) number = va_arg(params, int64_t);
            else number = va_arg(params, int);

            amount += strlen(itoa(number, buffer, 10));
            print(buffer, strlen(buffer));
            format++;
        }
        else if(*format == 'u')
        {
            //Unsigned integer
            uint32_t number;
            if(length == 1) number = va_arg(params, uint32_t);
            else if(length == 2) number = va_arg(params, uint32_t);
            else number = va_arg(params, int);

            amount += strlen(ultoa(number, buffer, 10));
            print(buffer, strlen(buffer));
            format++;
        }
        else if(*format == 'x')
        {
            //Hex (lowercase)
            uint32_t number;
            if(length == 1)
                number = va_arg(params, uint32_t);
            else if(length == 2)
                number = va_arg(params, uint32_t);
            if(pound) print("0x", strlen("0x"));

            amount += strlen(ultoa(number, buffer, 16));
            for(size_t i = 0; i < strlen(buffer); i++)
                buffer[i] = tolower(buffer[i]);
            print(buffer, strlen(buffer));
            format++;
        }
        else if(*format == 'X')
        {
            //Hex (uppercase)
            uint32_t number;
            if(length == 1)
                number = va_arg(params, uint32_t);
            else if(length == 2)
                number = va_arg(params, uint32_t);
                
            if(pound) print("0x", strlen("0x"));

            amount += strlen(ultoa(number, buffer, 16));
            print(buffer, strlen(buffer));
            format++;
        }
        else {
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
