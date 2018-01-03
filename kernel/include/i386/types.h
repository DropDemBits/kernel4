#include <stdint.h>
#include <stdbool.h>

#ifndef __TYPES_H__
#define __TYPES_H__ 1

#define KNULL (void*)0xDEADBEEFUL
#define KMEM_POISON 0xFEEE1UL

typedef enum
{
	ISR_NOT_HANDLED = 0,
	ISR_HANDLED = 1,
} isr_retval_t;

typedef uint32_t* uptr_t;
typedef uint32_t uintptr_t;
typedef uint32_t size_t;
typedef uint32_t linear_addr_t;
typedef uint32_t physical_addr_t;

typedef isr_retval_t(*isr_t)(void*);

#endif /*__TYPES_H__*/
