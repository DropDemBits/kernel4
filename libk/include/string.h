#include <__commons.h>

#ifndef STRING_H
#define STRING_H

#ifdef __cplusplus
extern "C"
{
#endif

__EXPORT_SPEC__ int memcmp(const void* str1, const void* str2, size_t num);
__EXPORT_SPEC__ void* memcpy(void* dest, const void* src, size_t num);
__EXPORT_SPEC__ void* memmove(void* dest, const void* src, size_t num);
__EXPORT_SPEC__ void* memset(void* str, const int c, size_t num);

__EXPORT_SPEC__ size_t strlen(const char* str);
__EXPORT_SPEC__ int strcmp(const char* str1, const char* str2);
__EXPORT_SPEC__ int strncmp(const char* str1, const char* str2, size_t num);
__EXPORT_SPEC__ void* strcpy(char* dest, const char* src);
__EXPORT_SPEC__ void* strncpy(char* dest, const char* src, size_t num);
__EXPORT_SPEC__ char* strcat (char* dest, const char* src);
__EXPORT_SPEC__ char* strncat (char* dest, const char* src, size_t num);
__EXPORT_SPEC__ const char* strstr (const char* str1, const char* str2);
__EXPORT_SPEC__ const char* strrchr(const char* str, int chr);
__EXPORT_SPEC__ const char* strchr(const char* str, int chr);

__EXPORT_SPEC__ size_t strspn(const char* str, const char* delim);
__EXPORT_SPEC__ char* strtok_r(char* str, const char* delim, char** lasts);

//Non standard
__EXPORT_SPEC__ int strnicmp(const char* str1, const char* str2, size_t length);

__EXPORT_SPEC__ char* _strupr(char* s);
#define strupr(s) _strupr(s);

#ifndef __cplusplus__strings__
__EXPORT_SPEC__ int stricmp(const char* str1, const char* str2, size_t length);
#endif

#ifdef __cplusplus
}
#endif

#endif
