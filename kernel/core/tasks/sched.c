#include <sched.h>
#include <stdio.h>

struct thread_queue
{
	thread_t *queue_head;
	thread_t *queue_tail;
};

static process_t* active_process = KNULL;
static struct thread_queue thread_queue;

void sched_init()
{
	thread_queue.queue_head = KNULL;
	thread_queue.queue_tail = KNULL;
}

void sched_queue_thread(thread_t *thread)
{
	if(thread_queue.queue_head == KNULL)
	{
		thread_queue.queue_head = thread;
		thread_queue.queue_tail = thread;
		active_process = thread->parent;
	} else
	{
		thread_queue.queue_tail->next = thread;
		thread_queue.queue_tail = thread;
	}
}

process_t *sched_active_process()
{
	return active_process;
}

thread_t *sched_active_thread()
{
	return thread_queue.queue_head;
}

void sched_switch_thread()
{
	if(thread_queue.queue_head == KNULL) return;

	thread_t *active_thread = thread_queue.queue_head;
	thread_t *next_thread = thread_queue.queue_head->next;

	if(next_thread == KNULL) return;

	thread_queue.queue_head = next_thread;
	active_thread->next = KNULL;
	active_process = next_thread->parent;
	sched_queue_thread(active_thread);
	switch_thread(active_thread, next_thread);
}
