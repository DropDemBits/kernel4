#include <stdio.h>
#include <string.h>

#include <common/syscall.h>
#include <common/sched/sched.h>
#include <common/fs/vfs.h>
#include <common/util/klog.h>

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
    // TODO: TEMP Acqure tty file & write to it
    // TODO: Replace with acquiring filedesc & calling write
    struct vfs_mount *mount = vfs_get_mount("/");
    if(mount == NULL)
        return -1;

    struct dnode *tty = vfs_finddir(mount->instance->root, "/dev/tty1");

    // Oppsie
    if(tty == NULL)
        return -1;

    char* print_out = (char*)frame->arg1;
    return vfs_write(tty->inode, 0, strlen(print_out), print_out);
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
