#include <stdint.h>
#include <stdbool.h>

#ifndef __TYPES_H__
#define __TYPES_H__ 1

#define KNULL (void*)0xDEADBEEFUL
#define KMEM_POISON 0xFEEE1UL

typedef uint32_t* uptr_t;
typedef uint32_t uintptr_t;
typedef uint32_t size_t;
typedef int32_t ssize_t;
typedef uint32_t linear_addr_t;
typedef uint32_t physical_addr_t;
typedef uint32_t ino_t;

typedef void(*isr_t)(void*);

#endif /*__TYPES_H__*/
