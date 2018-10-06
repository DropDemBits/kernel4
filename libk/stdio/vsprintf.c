#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#include <archtypes.h>

static void print(char *dest, const char* data, size_t data_length, size_t* offset)
{
    for(size_t i = 0; i < data_length; i++)
        dest[i + *offset] = data[i];
    *offset += data_length;
}

int vsprintf(char *dest, const char *format, va_list params)
{
    size_t amount = 0;
    bool rejected_formater = false;
    bool pound = false;
    int length = 0;
    int written = 0;
    char buffer[65];
    size_t index = 0;

    // Padding
    size_t min_chars = 0;
    char padding_char = ' ';

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

                // Since we never jump to the reset portion, reset amount here
                amount = 0;
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

        // Flag parsing
        if(*format == '#')
        {
            pound = true;
            format++;
        }
        else if(*format == ' ')
        {
            padding_char = ' ';
            format++;
        }
        else if(*format == '0')
        {
            padding_char = '0';
            format++;
        }

        // Width Parsing
        parse_next_digit:
        if(isdigit(*format))
        {
            // Number here is base 10
            min_chars *= 10;
            min_chars += *format - '0';
            format++;
            goto parse_next_digit;
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
            print(dest, &c, sizeof(c), &index);
            amount += 1;
        }
        else if(*format == 's')
        {
            format++;
            const char* s = va_arg(params, const char*);

            size_t buf_len = strlen(s);

            if(buf_len < min_chars)
            {
                // Put padding chars on until we match the limit
                int i = 0;
                for(; min_chars > buf_len; i++, min_chars--)
                    dest[i + index] = padding_char;
                index += i;
                amount += i;
            }

            print(dest, s, buf_len, &index);
            amount += buf_len;
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

            size_t buf_len = strlen(lltoa(number, buffer, 10));

            if(buf_len < min_chars)
            {
                // Put padding chars on until we match the limit
                int i = 0;
                for(; min_chars > buf_len; i++, min_chars--)
                    dest[i + index] = padding_char;
                index += i;
                amount += i;
            }

            amount += buf_len;
            print(dest, buffer, strlen(buffer), &index);
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

            size_t buf_len = strlen(ulltoa(number, buffer, 10));

            if(buf_len < min_chars)
            {
                // Put padding chars on until we match the limit
                int i = 0;
                for(; min_chars > buf_len; i++, min_chars--)
                    dest[i + index] = padding_char;
                index += i;
                amount += i;
            }

            amount += buf_len;
            print(dest, buffer, strlen(buffer), &index);
            format++;
        }
        else if(*format == 'X' || *format == 'x')
        {
            // Hex
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

            size_t buf_len = strlen(ulltoa(number, buffer, 16));

            if(buf_len < min_chars && padding_char == ' ')
            {
                // Put spaces on until we match the limit (done before in order to shift num)
                int i = 0;
                for(; min_chars > buf_len; i++, min_chars--)
                    dest[i + index] = padding_char;
                index += i;
                amount += i;
            }

            if(pound)
            {
                print(dest, "0", 1, &index);
                print(dest, format, 1, &index);
                amount += 2;
            }

            if(*format == 'x')
            {
                // Convert all charachters to lowercase
                for(size_t i = 0; i < buf_len; i++)
                    buffer[i] = tolower(buffer[i]);
            }
            
            if(buf_len < min_chars)
            {
                // Put padding chars on until we match the limit
                int i = 0;
                for(; min_chars > buf_len; i++, min_chars--)
                    dest[i + index] = padding_char;
                index += i;
                amount += i;
            }

            amount += buf_len;
            print(dest, buffer, buf_len, &index);

            format++;
        }
        else if(*format == 'p')
        {
            //Pointer (uppercase)
            unsigned long long number = GET_POINTER(params);

            size_t buf_len = strlen(ulltoa(number, buffer, 16));

            if(buf_len < min_chars && padding_char == ' ')
            {
                // Put spaces on until we match the limit (done before in order to shift num)
                int i = 0;
                for(; min_chars > buf_len; i++, min_chars--)
                    dest[i + index] = padding_char;
                index += i;
                amount += i;
            }

            if(pound)
            {
                print(dest, "0x", 2, &index);
                amount += 2;
            }
            
            if(buf_len < min_chars)
            {
                // Put padding chars on until we match the limit
                int i = 0;
                for(; min_chars > buf_len; i++, min_chars--)
                    dest[i + index] = padding_char;
                index += i;
                amount += i;
            }

            amount += buf_len;
            print(dest, buffer, buf_len, &index);

            format++;
        }
        else {
        bad_formatting:
            min_chars = 0;
            length = 0;
            amount = 0;
            pound = false;
            rejected_formater = true;
            goto bad_parsing;
        }

        padding_char = ' ';
        min_chars = 0;
        length = 0;
        written += amount;
        amount = 0;
        pound = false;
    }

    print(dest, "\0", 1, &index);
    return written;
}
