#include <common/types.h>
#include <common/mm.h>

#ifndef __TASKS_H__
#define __TASKS_H__

enum thread_state
{
	STATE_INITIALIZED,
	STATE_RUNNING,
	STATE_BLOCKED,
	STATE_SLEEPING,
	STATE_EXITED,
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

	// TODO: Do we want to implement separate address spaces for exploit
	// mitigation?
	paging_context_t* page_context_base;
} process_t;

typedef struct thread
{
	process_t *parent;
	struct thread *next;
	struct thread *prev;

	enum thread_state current_state;

	unsigned int tid;
	unsigned int timeslice;
	enum thread_priority priority;
	struct thread_registers *register_state;
	const char* name;
} thread_t;

process_t* process_create();
thread_t* thread_create(process_t *parent, void *entry_point, enum thread_priority priority, const char* name);
void thread_destroy(thread_t *thread);

#endif /* __TASKS_H__ */
