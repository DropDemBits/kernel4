#include <common/types.h>

#ifndef __SYSCALLS_H_
#define __SYSCALLS_H_ 1

#define NR_SYSCALLS 32768

void syscall_init();
void syscall_add(uint64_t number, uint64_t entry_point);

#endif /* __SYSCALLS_H_ */