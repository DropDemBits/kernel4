#include <__commons.h>

#ifndef STDARG_H
#define STDARG_H

#ifdef __cplusplus
extern "C"
{
#endif

typedef unsigned char *va_list;

#define STACKITEM int

#define VA_SIZE(TYPE) \
    ((sizeof(TYPE) + sizeof(STACKITEM) - 1)	& ~(sizeof(STACKITEM) - 1))
#define va_start(AP, LAST_ARG) \
    (AP=((va_list)&(LAST_ARG)+VA_SIZE(LAST_ARG)))
#define va_arg(AP,TYPE) \
    (AP += VA_SIZE(TYPE), *((TYPE *)(AP - VA_SIZE(TYPE))))
#define va_end(AP)

#ifdef __cplusplus
}
#endif

#endif
