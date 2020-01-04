#ifndef __TASKS_H__
#define __TASKS_H__

#include <common/types.h>
#include <common/mm/mm.h>

#include <arch/cpufuncs.h>

#define THREAD_STACK_SIZE 4096*4

// Forward declares
struct thread;
struct thread_queue;

struct bitmap
{
    uint64_t* bitmaps;
    size_t bitmap_len;
};

struct thread_queue
{
    struct thread *queue_head;
    struct thread *queue_tail;
};

// Depends on "thread_queue"
#include <common/util/locks.h>

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

struct filedesc;

typedef struct process
{
    unsigned int pid;
    struct thread *threads;

    struct process *parent;
    struct process *sibling;
    struct process *child;

    const char* name;

    // ???: Do we want to implement separate address spaces for exploit mitigation?
    paging_context_t* page_context_base;

    struct mutex* fd_lock;           // Protects fd_*
    struct bitmap fd_alloc;         // Bitmap to allocated file descriptors
    struct filedesc* *fd_map;   // Pointer to flexible array of file descriptors
    size_t fd_map_len;          // Size of map
} process_t;

typedef struct thread
{
    process_t *parent;
    struct thread *next;

    // Architechtural State //
    // Editing the stack pointer location requires modification of usermode_entry
    unsigned long kernel_sp;
    unsigned long user_sp;
    unsigned long kernel_stacktop;
    unsigned long user_stacktop;
    // Only used to preserve the flag state across non-interrupt task switches
    cpu_flags_t saved_flags;

    // Thread Info //
    enum thread_state current_state;

    unsigned int tid;
    unsigned int timeslice;
    uint64_t sleep_until;
    enum thread_priority priority;
    const char *name;

    struct thread *sibling;

    // Message Passing
    void* pending_msgs; // Avoids circular dependency between message.h and tasks.h
    struct thread_queue pending_senders;

} thread_t;

void tasks_init(char* init_name, void* init_entry);
process_t* process_create(const char *name);
thread_t* thread_create(process_t *parent, void *entry_point, enum thread_priority priority, const char* name, void* params);
void thread_destroy(thread_t *thread);

#endif /* __TASKS_H__ */
