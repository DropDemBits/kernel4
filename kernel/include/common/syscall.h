#include <common/types.h>

#ifndef __SYSCALLS_H_
#define __SYSCALLS_H_ 1

#define NR_SYSCALLS 32768

typedef unsigned long syscall_ret;
struct syscall_args
{
    unsigned long retidx; // rax
    unsigned long arg1; // rbx
    unsigned long arg2; // rcx
    unsigned long arg3; // rdx
    unsigned long arg4; // rsi
    unsigned long arg5; // rdi
    unsigned long arg6; // rbp
};

typedef syscall_ret(*syscall_func_t)(struct syscall_args*);

void syscall_init();
void syscall_add(uint64_t number, syscall_func_t entry_point);

#endif /* __SYSCALLS_H_ */