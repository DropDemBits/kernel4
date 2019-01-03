#include <unistd.h>

#include <common/types.h>
#include <common/tasks/tasks.h>
#include <common/sched/sched.h>

pid_t fork()
{
    // Create a new *process* and clone its data
    process_t* clone = process_create();
    process_t* current = sched_active_process();
}
