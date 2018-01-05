#include <tasks.h>
#include <sched.h>
#include <string.h>
#include <stdio.h>
#include <mm.h>

extern void init_register_state(struct thread_registers *registers, uint64_t *entry_point);

static unsigned int pid_counter = 0;
static unsigned int tid_counter = 0;

process_t* process_create()
{
	process_t *process = kmalloc(sizeof(process_t));

	process->child_threads = KNULL;
	process->pid = pid_counter++;
	process->current_state = STATE_SLEEPING;
	process->child_count = 0;

	return process;
}

void process_add_child(process_t *parent, thread_t *child)
{
	if(parent->child_threads == KNULL)
	{
		parent->child_threads = kcalloc(4, sizeof(thread_t*));
		if(parent->child_threads == NULL)
			return;

		parent->child_threads[0] = child;
		return;
	} else
	{
		if(parent->child_count % 4 == 0)
			parent->child_threads = krealloc(parent->child_threads, (parent->child_count+4)*sizeof(thread_t*));

		parent->child_threads[parent->child_count++] = child;
	}
}

thread_t* thread_create(process_t *parent, uint64_t *entry_point)
{
	thread_t *thread = kmalloc(sizeof(thread_t));

	thread->parent = parent;
	thread->next = KNULL;
	thread->current_state = STATE_INITIALIZED;
	thread->tid = tid_counter++;
	thread->register_state = kmalloc(sizeof(struct thread_registers));
	memset(thread->register_state, 0, sizeof(struct thread_registers));
	init_register_state(thread->register_state, entry_point);

	process_add_child(parent, thread);

	sched_queue_thread(thread);
	return thread;
}
