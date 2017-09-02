#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

static void print(char *dest, const char* data, size_t data_length, size_t* offset)
{
    for(size_t i = *offset; i < data_length; i++, (*offset)++)
        dest[i] = data[i];
}

int vsprintf(char *dest, const char *format, va_list params)
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
                print(dest, format, amount, &index);
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
            print(dest, &c, sizeof(c), &index);
        }
        else if(*format == 's')
        {
            format++;
            const char* s = va_arg(params, const char*);
            print(dest, s, strlen(s), &index);
        }
        else if(*format == 'd' || *format == 'i')
        {
            //Signed integer
            size_t number;
            if(length == 1) number = va_arg(params, int32_t);
            else if(length == 2) number = va_arg(params, int64_t);
            else number = va_arg(params, int);

            amount += strlen(itoa(number, buffer, 10));
            print(dest, buffer, strlen(buffer), &index);
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
            print(dest, buffer, strlen(buffer), &index);
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

            if(pound) print(dest, "0x", strlen("0x"), &index);

            amount += strlen(ultoa(number, buffer, 16));
            for(size_t i = 0; i < strlen(buffer); i++)
                buffer[i] = tolower(buffer[i]);
            print(dest, buffer, strlen(buffer), &index);
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

            if(pound) print(dest, "0x", strlen("0x"), &index);

            amount += strlen(ultoa(number, buffer, 16));
            print(dest, buffer, strlen(buffer), &index);
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

    print(dest, "\0", 1, &index);
    return written;
}
