#include <__commons.h>

#ifndef STDARG_H
#define STDARG_H

#ifdef __cplusplus
extern "C"
{
#endif
/*
typedef unsigned char *va_list;

#define STACKITEM int

#define VA_SIZE(TYPE) \
    ((sizeof(TYPE) + sizeof(STACKITEM) - 1)	& ~(sizeof(STACKITEM) - 1))
#define va_start(AP, LAST_ARG) \
    (AP=((va_list)&(LAST_ARG)+VA_SIZE(LAST_ARG)))
#define va_arg(AP,TYPE) \
    (AP += VA_SIZE(TYPE), *((TYPE *)(AP - VA_SIZE(TYPE))))
#define va_end(AP)
*/

typedef __builtin_va_list va_list;
#define va_start(v, l) __builtin_va_start(v, l)
#define va_arg(v, t)   __builtin_va_arg(v, t)
#define va_end(v)      __builtin_va_end(v)

#ifdef __cplusplus
}
#endif

#endif
