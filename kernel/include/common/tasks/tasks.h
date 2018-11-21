#include <common/types.h>
#include <common/mm/mm.h>

#ifndef __TASKS_H__
#define __TASKS_H__

#define THREAD_STACK_SIZE 4096*4

struct thread;

struct thread_queue
{
    struct thread *queue_head;
    struct thread *queue_tail;
};

enum thread_state
{
    STATE_RUNNING,
    STATE_READY,
    STATE_SUSPENDED,
    STATE_SLEEPING,
    STATE_EXITED,
    STATE_BLOCKED,
};

enum thread_priority
{
    PRIORITY_KERNEL = 6,
    PRIORITY_HIGHER = 5,
    PRIORITY_HIGH = 4,
    PRIORITY_NORMAL = 3,
    PRIORITY_LOW = 2,
    PRIORITY_LOWER = 1,
    PRIORITY_IDLE = 0,
    PRIORITY_COUNT = 7,
};

typedef struct process
{
    unsigned int pid;
    uint64_t child_count;
    struct thread **child_threads;

    // TODO: Do we want to implement separate address spaces for exploit mitigation?
    paging_context_t* page_context_base;
} process_t;

typedef struct thread
{
    process_t *parent;
    struct thread *next;

    // Editing the stack pointer location requires modification of usermode_entry
    unsigned long kernel_sp;
    unsigned long user_sp;
    unsigned long kernel_stacktop;
    unsigned long user_stacktop;

    enum thread_state current_state;

    unsigned int tid;
    unsigned int timeslice;
    uint64_t sleep_until;
    enum thread_priority priority;
    const char* name;

    // Message Passing
    void* pending_msgs; // Avoids circular dependency between message.h and tasks.h
    struct thread_queue pending_senders;

} thread_t;

void tasks_init(char* init_name, void* init_entry);
process_t* process_create();
thread_t* thread_create(process_t *parent, void *entry_point, enum thread_priority priority, const char* name, void* params);
void thread_destroy(thread_t *thread);

#endif /* __TASKS_H__ */
