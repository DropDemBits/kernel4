#include <__commons.h>
#include <stdarg.h>

#ifndef STDIO_H
#define STDIO_H

#ifdef __cplusplus
extern "C"
{
#endif

// Macros
#define EOF -1
#define SEEK_SET 1

typedef struct {} FILE;

extern FILE *stderr, *stdin, *stdout;

__EXPORT_SPEC__ int printf(const char *format, ...);
__EXPORT_SPEC__ int sprintf(char *dest, const char *format, ...);
__EXPORT_SPEC__ int vprintf(const char *format, va_list params);
__EXPORT_SPEC__ int vsprintf(char *dest, const char *format, va_list params);
__EXPORT_SPEC__ int snprintf(char *dest, size_t n, const char *format, ...);
__EXPORT_SPEC__ int vsnprintf(char *dest, size_t n, const char *format, va_list params);
__EXPORT_SPEC__ int puts(const char *str);
__EXPORT_SPEC__ int putchar(int ic);

__EXPORT_SPEC__ int vfprintf(FILE *stream, const char *format, va_list params);
__EXPORT_SPEC__ int fprintf(FILE *stream, const char *format, ...);

// File operations
__EXPORT_SPEC__ int fflush(FILE *stream);
__EXPORT_SPEC__ FILE* fopen(const char *restrict path, const char *restrict opts);
__EXPORT_SPEC__ int fclose(FILE *stream);
__EXPORT_SPEC__ size_t fread(void *restrict ptr, size_t size, size_t nitems, FILE *stream);
__EXPORT_SPEC__ size_t fwrite(void *restrict ptr, size_t size, size_t nitems, FILE *stream);
__EXPORT_SPEC__ int fseek(FILE *stream, long offset, int whence);
__EXPORT_SPEC__ long ftell(FILE *stream);
__EXPORT_SPEC__ void setbuf(FILE *strea, char* buf);

#ifdef __cplusplus
}
#endif

#endif
