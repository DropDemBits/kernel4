#include <sched.h>
#include <kfuncs.h>
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
static thread_t* active_thread = KNULL;
static thread_t* idle_thread = KNULL;
static struct thread_queue thread_queues[6];
static struct thread_queue sleep_queue;
static struct thread_queue blocked_queue;
static struct thread_queue exit_queue;
static bool cleanup_needed = false;
static bool preempt_enabled = false;

static unsigned int get_timeslice(enum thread_priority priority)
{
	switch(priority)
	{
		case PRIORITY_KERNEL: return 30;
		case PRIORITY_HIGHER: return 20;
		case PRIORITY_HIGH: return 15;
		case PRIORITY_NORMAL: return 10;
		case PRIORITY_LOW: return 5;
		case PRIORITY_LOWER: return 1;
		default: return 0;
	}
}

static isr_retval_t sched_timer()
{
	// We may not return to this thing here before we exit.
	ic_eoi(0);
	if(preempt_enabled)
	{
		if(active_thread != KNULL && active_thread->timeslice > 0)
			active_thread->timeslice--;
		else
			sched_switch_thread();
	}
	return ISR_HANDLED;
}

static void sched_queue_remove(thread_t* thread, struct thread_queue *queue)
{
	if(thread->prev != KNULL) thread->prev->next = thread->next;
	if(thread->next != KNULL) thread->next->prev = thread->prev;
	if(queue->queue_tail == thread) queue->queue_tail = thread->prev;
	thread->prev = KNULL;
	thread->next = KNULL;
}

static thread_t* sched_next_thread()
{
	for(int i = 0; i < 6; i++)
	{
		struct thread_queue *queue = &(thread_queues[5-i]);

		if(queue->queue_head != KNULL)
		{
			thread_t *next_thread = queue->queue_head;
			while(next_thread->current_state > STATE_RUNNING)
			{
				next_thread = next_thread->next;
				if(next_thread == KNULL) return KNULL;
			}

			if(next_thread == queue->queue_head)
			{
				queue->queue_head = next_thread->next;
				next_thread->next = KNULL;
			} else
			{
				sched_queue_remove(next_thread, queue);
			}

			return next_thread;
		}
	}
	return KNULL;
}

void sched_init()
{
	for(int i = 0; i < 6; i++)
	{
		thread_queues[i].queue_head = KNULL;
		thread_queues[i].queue_tail = KNULL;
	}
	exit_queue.queue_head = KNULL;
	exit_queue.queue_tail = KNULL;
	irq_add_handler(0, sched_timer);
}

void sched_gc()
{
	if(!cleanup_needed) return;

	cleanup_needed = false;
	struct thread_queue *queue = &exit_queue;

	if(queue->queue_head != KNULL)
	{
		thread_t *next_thread = queue->queue_head;

		while(next_thread != KNULL)
		{
			thread_destroy(next_thread);
			next_thread = next_thread->next;
		}
	}
}

void sched_queue_thread_to(thread_t *thread, struct thread_queue *queue)
{
	if(thread == KNULL) return;

	thread->timeslice = get_timeslice(thread->priority);
	if(queue->queue_head == KNULL)
	{
		queue->queue_head = thread;
		queue->queue_tail = thread;
		active_process = thread->parent;
	} else
	{
		queue->queue_tail->next = thread;
		thread->prev = queue->queue_tail;
		queue->queue_tail = thread;
	}
}

void sched_queue_thread(thread_t *thread)
{
	if(thread == KNULL) return;
	struct thread_queue *queue = &(thread_queues[(thread->priority+2)]);

	sched_queue_thread_to(thread, queue);
}

void sched_sleep_thread(thread_t *thread)
{
	if(thread == KNULL) return;

	sched_queue_thread_to(thread, &sleep_queue);

	struct thread_queue *queue = &(thread_queues[(thread->priority+2)]);
	if(queue->queue_head == thread)
	{
		queue->queue_head = thread->next;
	}
}

void sched_set_idle_thread(thread_t *thread)
{
	idle_thread = thread;
}

process_t *sched_active_process()
{
	return active_process;
}

inline thread_t *sched_active_thread()
{
	return active_thread;
}

void sched_switch_thread()
{
	preempt_disable();
	if(active_thread == KNULL)
	{
		active_thread = sched_next_thread();

		if(active_thread == KNULL && idle_thread != KNULL)
		{
			preempt_enable();
			switch_stack(idle_thread->register_state, KNULL);
		} else if(idle_thread == KNULL)
		{
			return;
		}
	}

	if(active_thread->current_state > STATE_RUNNING)
	{
		thread_t *old_thread = active_thread;
		active_thread = sched_next_thread();

		if(active_thread == KNULL && idle_thread != KNULL)
		{
			preempt_enable();
			switch_stack(idle_thread->register_state, old_thread->register_state);
		} else if(idle_thread == KNULL)
		{
			return;
		}

		active_process = active_thread->parent;
		preempt_enable();
		switch_stack(active_thread->register_state, old_thread->register_state);
	}

	thread_t *old_thread = active_thread;
	if(old_thread->current_state == STATE_INITIALIZED)
	{
		preempt_enable();
		switch_stack(old_thread->register_state, KNULL);
	}

	thread_t *next_thread = sched_next_thread();
	if(next_thread == KNULL && old_thread->current_state == STATE_RUNNING)
	{
		preempt_enable();
		return;
	} else
	{
		active_process = next_thread->parent;
		sched_queue_thread(old_thread);
		active_thread = next_thread;
		preempt_enable();
		switch_stack(active_thread->register_state, old_thread->register_state);
	}
}

void sched_set_thread_state(thread_t *thread, enum thread_state state)
{
	if(thread == KNULL) return;

	if(state == STATE_SLEEPING)
	{
		sched_sleep_thread(thread);
	} else if(state == STATE_EXITED)
	{
		cleanup_needed = true;
		sched_queue_thread_to(thread, &exit_queue);
	} else if(state == STATE_BLOCKED)
	{
		sched_queue_thread_to(thread, &blocked_queue);
	} else if(state == STATE_RUNNING)
	{
		if (thread->current_state == STATE_SLEEPING)
			sched_queue_remove(thread, &sleep_queue);
		else if (thread->current_state == STATE_BLOCKED)
			sched_queue_remove(thread, &blocked_queue);
		sched_queue_thread(thread);
	}

	thread->current_state = state;

	if(thread == active_thread)
		sched_switch_thread();
}

void preempt_disable()
{
	preempt_enabled = false;
}

void preempt_enable()
{
	preempt_enabled = true;
}
