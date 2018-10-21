#include <string.h>

char* strncat (char* dest, const char* src, size_t num)
{
    size_t src_len = strlen(src);
    size_t dest_len = strlen(dest);

    // Cap source length to num
    if(src_len > num)
        src_len = num;

    // Overlapping, return immediately
    if(dest >= src && (dest + dest_len) <= (src + src_len))
        return dest;

    // Directly append onto the end (Subtract src_len immediately to account for null byte)
    char* dest_chars = dest + dest_len;
    char* src_chars = src;
    while(*src_chars && --src_len)
        *(dest_chars++) = *(src_chars++);

    // Append null terminator
    *(dest_chars) = '\0';

    return dest;
}
