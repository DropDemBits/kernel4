#include <__commons.h>

#ifndef STRING_H
#define STRING_H

#ifdef __cplusplus
extern "C"
{
#endif

int memcmp(const void* str1, const void* str2, size_t num);
void* memcpy(void* dest, const void* src, size_t num);
void* memmove(void* dest, const void* src, size_t num);
void* memset(void* str, const int c, size_t num);
int strcmp(const char* str1, const char* str2);
int strncmp(const char* str1, const char* str2, size_t num);
void* strcopy(char* dest, const char* src);
void* strncopy(char* dest, const char* src, size_t num);
size_t strlen(const char* str);

//Non standard
int strnicmp(const char* str1, const char* str2, size_t length);

#ifdef __cplusplus__strings__
int stricmp(const char* str1, const char* str2, size_t length);
#endif

#ifdef __cplusplus
}
#endif

#endif
