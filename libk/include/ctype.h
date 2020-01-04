#include <__commons.h>
#include <stdint.h>

#ifndef CTYPE_H
#define CTYPE_H

/*
 * 0:  upper        0x0001
 * 1:  lower        0x0002
 * 2:  digit        0x0004
 * 3:  cntrl        0x0008
 * 4:  space        0x0010
 * 5:  punct        0x0020
 * 6:  xdigit       0x0040
 * 7:  blank        0x0080
 */
extern const uint8_t _char_bitmap[256];

#ifdef __cplusplus
extern "C"
{
#endif

#define isupper(c)  (_char_bitmap[(unsigned char)(c)] & 0x01)
#define islower(c)  (_char_bitmap[(unsigned char)(c)] & 0x02)
#define isdigit(c)  (_char_bitmap[(unsigned char)(c)] & 0x04)
#define iscntrl(c)  (_char_bitmap[(unsigned char)(c)] & 0x08)
#define isspace(c)  (_char_bitmap[(unsigned char)(c)] & 0x10)
#define isalpha(c)  (_char_bitmap[(unsigned char)(c)] & 0x03)
#define isalnum(c)  (_char_bitmap[(unsigned char)(c)] & 0x07)
#define isprint(c)  (!(_char_bitmap[(unsigned char)(c)] & 0x08))
#define isgraph(c)  (((_char_bitmap[(unsigned char)(c)] & 0x18) ^ 0x18) == 0x18)
#define ispunct(c)  (_char_bitmap[(unsigned char)(c)] & 0x20)
#define isxdigit(c) (_char_bitmap[(unsigned char)(c)] & 0x40)

#define isblank(c)  (_char_bitmap[(unsigned char)(c)] & 0x80)

int toupper(int c);
int tolower(int c);

#ifdef __cplusplus
}
#endif

#endif
