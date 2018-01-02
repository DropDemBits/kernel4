#include <stdint.h>
#include <stdbool.h>

#ifndef __TYPES_H__
#define __TYPES_H__ 1

#define KNULL (void*)0xDEAD000000000000ULL
#define KMEM_POISON 0xDEADCEED5ULL

typedef uint64_t* uptr_t;
typedef uint64_t uintptr_t;
typedef uint64_t size_t;
typedef uint64_t linear_addr_t;
typedef uint64_t physical_addr_t;

typedef uint8_t(*isr_t)(void*);

#endif /* __TYPES_H__ */
