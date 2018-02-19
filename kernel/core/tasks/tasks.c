#include <tasks.h>
#include <sched.h>
#include <string.h>
#include <stdio.h>
#include <mm.h>

extern void init_register_state(thread_t *thread, uint64_t *entry_point);
extern void cleanup_register_state(thread_t *thread);

static unsigned int pid_counter = 0;
static unsigned int tid_counter = 0;

process_t* process_create()
{
	process_t *process = kmalloc(sizeof(process_t));

	process->child_threads = KNULL;
	process->pid = pid_counter++;
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

thread_t* thread_create(process_t *parent, uint64_t *entry_point, enum thread_priority priority)
{
	thread_t *thread = kmalloc(sizeof(thread_t));

	thread->parent = parent;
	thread->next = KNULL;
	thread->prev = KNULL;
	thread->current_state = STATE_INITIALIZED;
	thread->tid = tid_counter++;
	thread->priority = priority;
	thread->register_state = kmalloc(sizeof(struct thread_registers));
	memset(thread->register_state, 0, sizeof(struct thread_registers));
	init_register_state(thread, entry_point);

	process_add_child(parent, thread);

	if(priority != PRIORITY_IDLE)
		sched_queue_thread(thread);
	else
		sched_set_idle_thread(thread);
	return thread;
}

void thread_destroy(thread_t *thread)
{
	if(thread == KNULL) return;
	cleanup_register_state(thread);
	bool shiftback = false;

	for(int i = 0; i < thread->parent->child_count; i++)
	{
		if(shiftback)
		{
			thread->parent->child_threads[i-1] = thread->parent->child_threads[i];
		} else if(thread->parent->child_threads[i] == thread)
		{
			thread->parent->child_threads[i] = NULL;
			shiftback = true;
		}
	}
	thread->parent->child_count--;

	kfree((void*)thread->register_state);
	kfree(thread);
}
