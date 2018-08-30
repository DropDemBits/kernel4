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
	thread_t *new_state,
	thread_t *old_state,
	paging_context_t* new_context
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
static bool preempt_enabled = false;
// Queue of threads that can be run, but aren't actively running
struct thread_queue run_queue;
static int sched_semaphore = 0;
static int taskswitch_semaphore = 0;
static bool taskswitch_postponed = false;

#define TIMER_RESOLUTION (1193181 / 1000)
unsigned long long ticks_since_boot = 0;

static struct thread_queue thread_queues[PRIORITY_COUNT];
static struct thread_queue sleep_queue;
static struct thread_queue blocked_queue;
static struct thread_queue exit_queue;
static struct sleep_node* sleepers = KNULL;
static struct sleep_node* done_sleepers = KNULL;
static bool cleanup_needed = false;
static bool clean_sleepers = false;
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

static void sched_timer()
{
	// We may not return to this thing here before we exit, so eoi early.
	ic_eoi(0);

	ticks_since_boot += TIMER_RESOLUTION;


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
	struct thread_queue *queue = &run_queue;
	if(queue->queue_head != KNULL)
	{
		thread_t *next_thread = queue->queue_head;
		while(next_thread->current_state > STATE_READY)
		{
			next_thread = next_thread->next;
			if(next_thread == KNULL) break;
		}

		if(next_thread != KNULL)
			return next_thread;
	}
	return KNULL;
}

void sched_init()
{
	run_queue.queue_head = KNULL;
	run_queue.queue_tail = KNULL;
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
	// struct thread_queue *queue = &(thread_queues[(thread->priority)]);

	sched_queue_thread_to(thread, &run_queue);
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

static void switch_to_thread(thread_t* next_thread)
{
	if(taskswitch_semaphore != 0)
	{
		taskswitch_postponed = true;
		return;
	}

	thread_t* old_thread = active_thread;
	active_thread = next_thread;

	// Remove next thread from run queue
	sched_queue_remove(next_thread, &run_queue);
	next_thread->current_state = STATE_RUNNING;

	// Add current thread to run queue
	if(old_thread != KNULL)
	{
		if(old_thread->current_state == STATE_RUNNING)
			old_thread->current_state = STATE_READY;
		sched_queue_thread(old_thread);
	}

	mmu_set_context(next_thread->parent->page_context_base);
	switch_stack(next_thread, old_thread, next_thread->parent->page_context_base);
}

void sched_switch_thread()
{
	if(taskswitch_semaphore != 0)
	{
		taskswitch_postponed = true;
		return;
	}

	sched_track_swaps();
	thread_t* next_thread = sched_next_thread();
	switch_to_thread(next_thread);
}

// Debugs below
void sched_print_queues()
{
	thread_t* node = run_queue.queue_head;
	printf("%dRun Queue: ", sched_semaphore);

	if(active_thread != KNULL)
		printf("[%s] -> ", active_thread->name);
	
	while(node != KNULL)
	{
		printf("%s -> ", node->name);
		node = node->next;
		tty_reshow();
	}
	puts("NULL");
}

unsigned long long tswp_counter = 0;
long long tswp_timer = 0;
void sched_track_swaps()
{
	if(tswp_timer == 0)
		tswp_timer = ticks_since_boot;
	
	if(tswp_timer - ticks_since_boot <= 0)
	{
		printf("[Switches/s: %d]\n", tswp_counter);
		tswp_counter = 0;
		tswp_timer += 1000 * TIMER_RESOLUTION;
	}
	tswp_counter++;
}

void sched_block_thread(enum thread_state new_state)
{
	sched_lock();
	active_thread->current_state = new_state;
	sched_switch_thread();
	sched_unlock();
}

void sched_unblock_thread(thread_t* thread)
{
	sched_lock();
	thread->current_state = STATE_READY;

	if(sched_next_thread() == KNULL)
	{
		// Should there be no next task to run, pre-empt the current one
		switch_to_thread(thread);
	}
	else
	{
		// Otherwise, add back to run queue
		sched_queue_thread(thread);
	}

	sched_unlock();
}

void sched_set_thread_state(thread_t *thread, enum thread_state new_state)
{
	if(thread == KNULL) return;

	if(new_state == STATE_SLEEPING)
	{
		// We remove the thread from it's run queue first to ensure that it isn't
		// taking other threads off the run queue.
	 	
		sched_queue_remove(thread, &(thread_queues[thread->priority]));
		sched_queue_thread_to(thread, &sleep_queue);
	} else if(new_state == STATE_EXITED)
	{
		cleanup_needed = true;
		sched_queue_remove(thread, &(thread_queues[thread->priority]));
		sched_queue_thread_to(thread, &exit_queue);
	} else if(new_state == STATE_RUNNING)
	{
		if (thread->current_state == STATE_SLEEPING)
		{
			sched_queue_remove(thread, &sleep_queue);
		}
		sched_queue_thread(thread);
	}

	thread->current_state = new_state;

	if(thread == active_thread)
		sched_switch_thread();
}

void sched_lock()
{
	hal_save_interrupts();
	sched_semaphore++;
}

void sched_unlock()
{
	sched_semaphore--;
	if(sched_semaphore == 0)
		hal_restore_interrupts();
}

void taskswitch_disable()
{
	sched_lock();
	taskswitch_semaphore++;
}

void taskswitch_enable()
{
	taskswitch_semaphore--;
	if(taskswitch_semaphore == 0 && taskswitch_postponed)
	{
		sched_switch_thread();
	}

	sched_unlock();
}

void preempt_disable()
{
	preempt_enabled = false;
}

void preempt_enable()
{
	preempt_enabled = true;
}

process_t *sched_active_process()
{
	return active_process;
}

thread_t *sched_active_thread()
{
	return active_thread;
}
