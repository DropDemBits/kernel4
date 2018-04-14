#include <common/sched.h>
#include <common/syscall.h>

uint64_t syscalls[NR_SYSCALLS];

void yield_syscall()
{
    //uart_writec('a');
    sched_switch_thread();
    //sched_sleep_millis(1);
}

void syscall_init()
{
    for(size_t i = 0; i < NR_SYSCALLS; i++)
    {
        syscalls[i] = KNULL;
    }
    
    syscall_add(0, yield_syscall);
}

void syscall_add(uint64_t number, uint64_t entry_point)
{
    if(number > NR_SYSCALLS) return;
    syscalls[number] = entry_point;
}
