/**
 * Copyright (C) 2018 DropDemBits
 * 
 * This file is part of Kernel4.
 * 
 * Kernel4 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Kernel4 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Kernel4.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#include <common/sched.h>
#include <common/kfuncs.h>
#include <common/hal.h>
#include <common/liballoc.h>

extern void switch_stack(
	struct thread_registers *new_state,
	struct thread_registers *old_state
);

struct thread_queue
{
	thread_t *queue_head;
	thread_t *queue_tail;
};

struct sleep_node
{
	thread_t *thread;
	uint64_t delta;
	struct sleep_node* next;
};

static process_t* active_process = KNULL;
static thread_t* active_thread = KNULL;
static struct thread_queue thread_queues[PRIORITY_COUNT];
static struct thread_queue sleep_queue;
static struct thread_queue blocked_queue;
static struct thread_queue exit_queue;
static struct sleep_node* sleepers = KNULL;
static struct sleep_node* done_sleepers = KNULL;
static bool cleanup_needed = false;
static bool clean_sleepers = false;
static bool preempt_enabled = false;
static bool idle_entry = false;

static unsigned int get_timeslice(enum thread_priority priority)
{
	switch(priority)
	{
		case PRIORITY_KERNEL: return 30;
		case PRIORITY_HIGHER: return 20;
		case PRIORITY_HIGH: return 15;
		case PRIORITY_NORMAL: return 10;
		case PRIORITY_LOW: return 5;
		case PRIORITY_LOWER: return 3;
		case PRIORITY_IDLE: return 1;
		default: return 0;
	}
}

static isr_retval_t sched_timer()
{
	// We may not return to this thing here before we exit, so eoi early.
	ic_eoi(0);

	if(sleepers != KNULL)
	{
		if(sleepers->delta > 0)
		{
			// Case 1: We still have some time to kill, so decrement quanta
			sleepers->delta--;
		} else
		{
			// Case 2: We are scheduled to wakeup at this time, so start waking
			// threads up.
			int max_priority = PRIORITY_IDLE;
			struct sleep_node* tail_sleepers = KNULL;

			// Iterate through all the done sleeper nodes
			while(sleepers != KNULL && sleepers->delta == 0)
			{
				// Get the max priority of the woken threads and store it
				if(sleepers->thread->priority > max_priority)
					max_priority = sleepers->thread->priority;
				
				sched_set_thread_state(sleepers->thread, STATE_RUNNING);

				// Append sleeper node to queue for cleanup
				if(done_sleepers == KNULL)
					done_sleepers = sleepers;

				// Move tail pointer forward one
				tail_sleepers = sleepers;
				
				clean_sleepers = true;
				sleepers = sleepers->next;
			}

			// Clear tail node's next to prevent cleanup of active nodes
			tail_sleepers->next = KNULL;
			
			if(active_thread == KNULL || (active_thread->priority < max_priority))
			{
				sched_switch_thread();
				// Once we return, the quanta check will be done below us.
			}
		}
	}

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
	if(thread == queue->queue_head)
	{
		queue->queue_head = thread->next;
	} else
	{
		if(thread->prev != KNULL) thread->prev->next = thread->next;
		if(thread->next != KNULL) thread->next->prev = thread->prev;
		if(queue->queue_tail == thread) queue->queue_tail = thread->prev;
		thread->prev = KNULL;
	}

	thread->next = KNULL;
}

static thread_t* sched_next_thread()
{
	for(int i = 0; i < PRIORITY_COUNT; i++)
	{
		struct thread_queue *queue = &(thread_queues[PRIORITY_COUNT-1-i]);

		if(queue->queue_head != KNULL)
		{
			thread_t *next_thread = queue->queue_head;
			while(next_thread->current_state > STATE_RUNNING)
			{
				next_thread = next_thread->next;
				if(next_thread == KNULL) break;
			}

			if(next_thread == KNULL) continue;
			return next_thread;
		}
	}
	return KNULL;
}

void sched_init()
{
	for(int i = 0; i < PRIORITY_COUNT; i++)
	{
		thread_queues[i].queue_head = KNULL;
		thread_queues[i].queue_tail = KNULL;
	}
	exit_queue.queue_head = KNULL;
	exit_queue.queue_tail = KNULL;
	sleep_queue.queue_head = KNULL;
	sleep_queue.queue_tail = KNULL;
	irq_add_handler(0, sched_timer);
}

void sched_gc()
{
	if(cleanup_needed)
	{
		cleanup_needed = false;
		struct thread_queue *queue = &exit_queue;

		if(queue->queue_head != KNULL)
		{
			thread_t *next_thread = queue->queue_head;
			thread_t *current_thread = KNULL;

			while(next_thread != KNULL)
			{
				current_thread = next_thread;
				next_thread = next_thread->next;
				sched_queue_remove(current_thread, queue);
				thread_destroy(current_thread);
			}
		}
	}

	if(done_sleepers != KNULL && clean_sleepers)
	{
		clean_sleepers = false;
		struct sleep_node* node = done_sleepers;

		do
		{
			kfree(node);
			node = node->next;
		}
		while(node != KNULL);

		done_sleepers = KNULL;
	}
}

void sched_queue_thread_to(thread_t *thread, struct thread_queue *queue)
{
	if(thread == KNULL) return;

	if(thread->timeslice == 0)
		thread->timeslice = get_timeslice(thread->priority);

	if(queue->queue_head == KNULL)
	{
		queue->queue_head = thread;
		queue->queue_tail = thread;
		active_process = thread->parent;
	} else
	{
		if(queue->queue_tail != KNULL)
			queue->queue_tail->next = thread;
		
		// Link pointers together
		thread->prev = queue->queue_tail;
		queue->queue_tail = thread;
	}
}

void sched_queue_thread(thread_t *thread)
{
	if(thread == KNULL) return;
	struct thread_queue *queue = &(thread_queues[(thread->priority)]);

	sched_queue_thread_to(thread, queue);
}

void sched_sleep_millis(uint64_t millis)
{
	if(active_thread == KNULL)
		// Can't sleep when we haven't initialized
		return;

	struct sleep_node* node = kmalloc(sizeof(struct sleep_node));
	node->next = KNULL;
	node->delta = millis;
	node->thread = active_thread;

	if(sleepers == KNULL)
	{
		// Create a new queue
		sleepers = node;
	} else
	{
		// There are some sleeping threads.
		struct sleep_node* last_node = KNULL;
		struct sleep_node* next_node = sleepers;

		while(next_node != KNULL && node->delta > next_node->delta)
		{
			// Iterate through the entire list and adjust the delta accordingly
			node->delta -= next_node->delta;
			last_node = next_node;
			next_node = next_node->next;
		}

		if(next_node == KNULL)
		{
			// We are at the end of the list, so simply append
			last_node->next = node;
		} else if(node->delta <= next_node->delta)
		{
			if(last_node != KNULL)
			{
				// Insert between the last and next node
				last_node->next = node;
			} else
			{
				// Make our new node the new beginning of the list
				sleepers = node;
			}
			node->next = next_node;

			// Adjust delta of larger node
			next_node->delta = next_node->delta - node->delta;
		}
	}

	sched_set_thread_state(active_thread, STATE_SLEEPING);
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

	// Case 1: Just initialized
	if(active_thread == KNULL)
	{
		active_thread = sched_next_thread();

		if(active_thread == KNULL)
		{
			return;
		}
		idle_entry = false;

		sched_queue_remove(active_thread, &(thread_queues[active_thread->priority]));
	}

	// Case 2: Current thread has changed state
	if(active_thread->current_state > STATE_RUNNING)
	{
		thread_t *old_thread = active_thread;
		active_thread = sched_next_thread();

		if(active_thread == KNULL)
		{
			return;
		}
		idle_entry = false;

		sched_queue_remove(active_thread, &(thread_queues[active_thread->priority]));
		active_process = active_thread->parent;
		preempt_enable();
		switch_stack(active_thread->register_state, old_thread->register_state);
	}

	thread_t *old_thread = active_thread;
	// Case 3: First execution of the thread, so switch to stack.
	// Really only useful during the transition from kmain to core_fini.
	// Otherwise, just switches to the same stack.
	if(old_thread->current_state == STATE_INITIALIZED)
	{
		preempt_enable();
		switch_stack(old_thread->register_state, KNULL);
	}

	thread_t *next_thread = sched_next_thread();

	// Case 4: Next available thread has a lower priority than the
	// current thread, and the current thread isn't yielding (quanta == 0).
	// By now, the current thread is already to be confirmed to be running
	// (otherwise would have fallen to case 2 or 3), so there is no point
	// to check the current state.
	if(next_thread != KNULL && next_thread->priority < old_thread->priority)
	{
		if(old_thread->timeslice == 0)
		{
			// We aren't yielding, so reset timeslice to continue
			old_thread->timeslice = get_timeslice(old_thread->priority);
			preempt_enable();
			return;
		}
	}

	// Case 5: There is no available next thread. This really shouldn't need to
	// happen because the idle thread(s) should be the last threads to run.
	// In the event that it does (ie. it is the very last thread running) and
	// we are an idle thread, then return.
	if(next_thread == KNULL && old_thread->priority == PRIORITY_IDLE)
	{
		preempt_enable();
		return;
	} else if (next_thread != NULL)
	{
		// Case 6: The current thread is the same priorty as the new thread,
		// and it either has used it's quanta or is yielding
		sched_queue_remove(next_thread, &(thread_queues[next_thread->priority]));
		sched_queue_thread(old_thread);
		active_process = next_thread->parent;
		active_thread = next_thread;
		preempt_enable();
		switch_stack(active_thread->register_state, old_thread->register_state);
	} else
	{
		// Just in case..
		kpanic("Achieved a bad state. No next threads and current priority isn't idle");
	}
}

void sched_set_thread_state(thread_t *thread, enum thread_state new_state)
{
	if(thread == KNULL) return;

	if(new_state == STATE_SLEEPING)
	{
		/*
		 * We remove the thread from it's run queue first to ensure that it isn't
		 * taking other threads off the run queue.
	 	 */
		sched_queue_remove(thread, &(thread_queues[thread->priority]));
		sched_queue_thread_to(thread, &sleep_queue);
	} else if(new_state == STATE_EXITED)
	{
		cleanup_needed = true;
		sched_queue_remove(thread, &(thread_queues[thread->priority]));
		sched_queue_thread_to(thread, &exit_queue);
	} else if(new_state == STATE_BLOCKED)
	{
		sched_queue_remove(thread, &(thread_queues[thread->priority]));
		sched_queue_thread_to(thread, &blocked_queue);
	} else if(new_state == STATE_RUNNING)
	{
		if (thread->current_state == STATE_SLEEPING)
		{
			sched_queue_remove(thread, &sleep_queue);
		}
		else if (thread->current_state == STATE_BLOCKED)
			sched_queue_remove(thread, &blocked_queue);
		sched_queue_thread(thread);
	}

	thread->current_state = new_state;

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
