#include <stdio.h>

#include <common/syscall.h>
#include <common/sched/sched.h>

syscall_func_t syscalls[NR_SYSCALLS];

syscall_ret syscall_yield(struct syscall_args* frame)
{
    sched_lock();
    sched_switch_thread();
    sched_unlock();
    return 0;
}

syscall_ret syscall_print(struct syscall_args* frame)
{
    return printf((char*)frame->arg1);
}

syscall_ret syscall_sleep(struct syscall_args* frame)
{
    sched_sleep_ms(frame->arg1);
    return 0;
}

syscall_ret syscall_exit(struct syscall_args* frame)
{
    sched_terminate();
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
    syscall_add(3, syscall_exit);
}

void syscall_add(uint64_t number, syscall_func_t entry_point)
{
    if(number > NR_SYSCALLS) return;
    syscalls[number] = entry_point;
}
