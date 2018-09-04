#include <common/syscall.h>
#include <stack_state.h>

extern syscall_func_t syscalls[];

void syscall_common(struct syscall_args *frame)
{
    if(frame->retidx < NR_SYSCALLS && syscalls[frame->retidx] != KNULL)
    {
        syscall_func_t syscall_handler = syscalls[frame->retidx];

        frame->retidx = syscall_handler(frame);
    }
}
