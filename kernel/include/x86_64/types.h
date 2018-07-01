#include <stdint.h>
#include <stdbool.h>

#ifndef __TYPES_H__
#define __TYPES_H__ 1

#define KNULL (void*)0xFFFFDEAD00000000ULL
#define KMEM_POISON 0xDEADCEED5ULL

typedef uint64_t* uptr_t;
typedef uint64_t uintptr_t;
// typedef uint64_t size_t;
typedef int64_t ssize_t;
typedef uint32_t ino_t;

typedef void(*isr_t)(void*);

typedef uint64_t syscall_ret;
struct syscall_args
{
    uint64_t retidx; // rax
    uint64_t arg1; // rbx
    uint64_t arg2; // rcx
    uint64_t arg3; // rdx
    uint64_t arg4; // rsi
    uint64_t arg5; // rdi
    uint64_t arg6; // rbp
};

#endif /* __TYPES_H__ */
