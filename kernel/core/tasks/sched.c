#include <sched.h>
#include <stdio.h>
#include <hal.h>

extern void switch_stack(
	struct thread_registers *new_state,
	struct thread_registers *old_state
);

struct thread_queue
{
	thread_t *queue_head;
	thread_t *queue_tail;
};

static process_t* active_process = KNULL;
static struct thread_queue thread_queue;

static bool preempt_enabled = false;

isr_retval_t sched_timer()
{
	// We may not return to this thing here before we exit.
	ic_eoi(0);
	if(preempt_enabled)
	{
		if(thread_queue.queue_head != KNULL && thread_queue.queue_head->timeslice > 0)
			thread_queue.queue_head->timeslice--;
		else
			sched_switch_thread();
	}
	return ISR_HANDLED;
}

void sched_init()
{
	thread_queue.queue_head = KNULL;
	thread_queue.queue_tail = KNULL;
	irq_add_handler(0, sched_timer);
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

inline thread_t *sched_active_thread()
{
	return thread_queue.queue_head;
}

void sched_switch_thread()
{
	if(thread_queue.queue_head == KNULL) return;

	thread_t *active_thread = thread_queue.queue_head;
	thread_t *next_thread = thread_queue.queue_head->next;
	active_thread->timeslice = 10;

	if(next_thread == KNULL && active_thread->current_state != STATE_INITIALIZED)
	{
		return;
	} else if(active_thread->current_state == STATE_INITIALIZED)
	{
		switch_stack(active_thread->register_state, KNULL);
	} else
	{
		thread_queue.queue_head = next_thread;
		active_thread->next = KNULL;
		active_process = next_thread->parent;
		sched_queue_thread(active_thread);
		switch_stack(next_thread->register_state, active_thread->register_state);
	}
}

void preempt_disable()
{
	preempt_enabled = false;
}

void preempt_enable()
{
	preempt_enabled = true;
}
