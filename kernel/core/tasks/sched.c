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
static thread_t* idle_thread;
static thread_t* sleep_filo_head = KNULL;
static int sched_semaphore = 0;
static int taskswitch_semaphore = 0;
static bool taskswitch_postponed = false;
// Set if we are in the idle state (used for postponing postponed task switches)
static bool is_idle = false;

// To be moved into the generic timer code
// 1000000000 / (1193181 / 1000)
#define TIMER_RESOLUTION (838222)
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

	taskswitch_disable();
	ticks_since_boot += TIMER_RESOLUTION;

	// Iterate through the sleep stack
	thread_t* current_node = KNULL;
	// Use a next node in order to not use current_node->next
	thread_t* next_node = sleep_filo_head;
	sleep_filo_head = KNULL;


	while(next_node != KNULL)
	{
		current_node = next_node;
		next_node = current_node->next;

		if(ticks_since_boot >= current_node->sleep_until)
		{
			// Wakeup task
			current_node->next = KNULL;
			current_node->prev = KNULL;
			sched_unblock_thread(current_node);
		} else
		{
			current_node->next = sleep_filo_head;
			sleep_filo_head = current_node;
		}
	}

	taskswitch_enable();
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
	}

	thread->prev = KNULL;
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

	return idle_thread;
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

void sched_sleep_until(uint64_t when)
{
	taskswitch_disable();

	if(when < ticks_since_boot)
	{
		// Only enable the scheduler in order to not fire scheduling
		sched_unlock();
		return;
	}

	active_thread->sleep_until = when;

	// Append thread to the sleep list, filo/stack fashion
	active_thread->next = sleep_filo_head;
	sleep_filo_head = active_thread;

	taskswitch_enable();

	sched_block_thread(STATE_SLEEPING);
}

void sched_sleep_ns(uint64_t ns)
{
	sched_sleep_until(ticks_since_boot + ns);
}

void sched_sleep(uint64_t ms)
{
	sched_sleep_ns(ms * 1000000);
}

void sched_sleep_millis(uint64_t millis)
{
	sched_sleep(millis);
	/*
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

	sched_block_thread(STATE_SLEEPING);*/
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
	active_process = active_thread->parent;

	// Remove next thread from run queue
	sched_queue_remove(next_thread, &run_queue);
	next_thread->current_state = STATE_RUNNING;

	// Add current thread to run queue
	if(old_thread != KNULL)
	{
		if(old_thread->current_state == STATE_RUNNING)
			old_thread->current_state = STATE_READY;
		
		if(old_thread->current_state < STATE_SLEEPING)
			// Requeue thread if it isn't sleeping or exited
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
	if(next_thread != KNULL)
	{
		if(next_thread == idle_thread)
		{
			if(active_thread->current_state == STATE_RUNNING)
			{
				// There is no next thread, but current thread is still running, so return.
				return;
			}
		}

		// We have a next thread, so switch to it
		switch_to_thread(next_thread);
	}
	/*else if(active_thread != KNULL && active_thread->current_state == STATE_RUNNING)
	{
		// Current thread is still running, so just return
		return;
	}
	else
	{
		// Current thread is blocked, enter idle state
		thread_t* thread = active_thread;
		// In the future, we may want to start a timer until the next power state
		//uint64_t idle_start = ticks_since_boot;

		active_thread = KNULL;

		// Unlock the scheduler to temporarily enable interrupts
		is_idle = true;
		do
		{
			hal_enable_interrupts();
			intr_wait();
			hal_disable_interrupts();
		}
		while(sched_next_thread() == KNULL);
		is_idle = false;

		// Return the thread
		active_thread = thread;

		sched_track_swaps();
		// Switch to next active thread (unless it was the previously running thread)
		thread_t* next_thread = sched_next_thread();
		if(next_thread != thread)
			switch_to_thread(next_thread);
	}*/
}

// Debugs below
void sched_print_queues()
{
	thread_t* node = run_queue.queue_head;
	printf("Run Queue: ");

	if(active_thread != KNULL)
		printf("[%p] -> ", active_thread);
	
	int i = 0;
	while(node != KNULL && i < 5)
	{
		printf("%p -> ", node);
		node = node->next;
		i++;
		tty_reshow();
	}
	puts("NULL");
}

unsigned long long tswp_counter = 0;
void sched_track_swaps()
{
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

	if(sched_next_thread() == KNULL || active_thread == idle_thread)
	{
		// Should there be no next task to run, pre-empt the current one
		switch_to_thread(thread);

		if(taskswitch_postponed)
		{
			// If the task switch was postponed (ie. during sleeper wakeup), just queue the thread for later
			sched_queue_thread(thread);
		}
	}
	else
	{
		// Otherwise, add back to run queue
		sched_queue_thread(thread);
	}

	sched_unlock();
}

void sched_setidle(thread_t* thread)
{
	idle_thread = thread;
}

void sched_lock()
{
	//hal_save_interrupts();

	hal_disable_interrupts();
	sched_semaphore++;
}

void sched_unlock()
{
	sched_semaphore--;
	if(sched_semaphore == 0)
		hal_enable_interrupts();
		//hal_restore_interrupts();
}

void taskswitch_disable()
{
	sched_lock();
	taskswitch_semaphore++;
}

void taskswitch_enable()
{
	taskswitch_semaphore--;
	// Don't do a task switch if we are in the idle state (will be handled by idle loop exit)
	if(taskswitch_semaphore == 0 && taskswitch_postponed && !is_idle)
	{
		taskswitch_postponed = false;
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
