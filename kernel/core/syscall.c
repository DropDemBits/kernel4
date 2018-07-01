#include <stdio.h>

#include <common/sched.h>
#include <common/syscall.h>

syscall_func_t syscalls[NR_SYSCALLS];

syscall_ret syscall_yield(struct syscall_args* frame)
{
    sched_switch_thread();
    return 0;
}

syscall_ret syscall_print(struct syscall_args* frame)
{
    return printf((char*)frame->arg1);
}

syscall_ret syscall_sleep(struct syscall_args* frame)
{
    sched_sleep_millis(frame->arg1);
    return 0;
}

void syscall_init()
{
    for(size_t i = 0; i < NR_SYSCALLS; i++)
    {
        syscalls[i] = KNULL;
    }
    
    syscall_add(0, syscall_yield);
    syscall_add(1, syscall_print);
    syscall_add(2, syscall_sleep);
}

void syscall_add(uint64_t number, syscall_func_t entry_point)
{
    if(number > NR_SYSCALLS) return;
    syscalls[number] = entry_point;
}
