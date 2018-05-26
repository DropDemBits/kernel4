#include <common/syscall.h>
#include <x86_64/stack_state.h>

extern uint64_t syscalls[];

void syscall_common(struct intr_stack *frame)
{
    if(frame->rax < NR_SYSCALLS && syscalls[frame->rax] != KNULL)
    {
        void (*syscall_handler)(void);
        syscall_handler = syscalls[frame->rax];

        syscall_handler();
    }
}
