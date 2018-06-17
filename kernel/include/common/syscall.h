#include <common/types.h>

#ifndef __SYSCALLS_H_
#define __SYSCALLS_H_ 1

#define NR_SYSCALLS 32768

typedef syscall_ret(*syscall_func_t)(struct syscall_arguments*);

void syscall_init();
void syscall_add(uint64_t number, syscall_func_t entry_point);

#endif /* __SYSCALLS_H_ */