#include <stdint.h>
#include <stdbool.h>

#ifndef __TYPES_H__
#define __TYPES_H__ 1

#define KNULL (void*)0xDEADBEEF

typedef uint32_t* uptr_t;
typedef uint32_t size_t;
typedef uint32_t linear_addr_t;
typedef uint32_t physical_addr_t;

typedef uint8_t(*isr_t)(void*);

#endif /*__TYPES_H__*/
