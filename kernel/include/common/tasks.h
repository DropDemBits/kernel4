#include <types.h>
#include <stack_state.h>

#ifndef __TASKS_H__
#define __TASKS_H__

enum thread_state
{
	STATE_INITIALIZED,
	STATE_RUNNING,
	STATE_BLOCKED,
};

enum process_state
{
	STATE_SLEEPING,
	STATE_ACTIVE,
};

typedef struct process
{
	unsigned int pid;
	uint64_t child_count;
	struct thread **child_threads;
	enum process_state current_state;
} process_t;

typedef struct thread
{
	process_t *parent;
	struct thread *next;

	enum thread_state current_state;

	unsigned int tid;
	struct thread_registers *register_state;
} thread_t;

process_t* process_create();
thread_t* thread_create(process_t *parent, uint64_t *entry_point);

#endif /* __TASKS_H__ */
