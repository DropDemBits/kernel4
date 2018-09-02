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

#define QUANTA_LENGTH (15 * 1000000) // 15000000ns =  15ms

static thread_t* active_thread = KNULL;
static thread_t* idle_thread;
static thread_t* cleanup_thread;

// Queue of threads that can be run, but aren't actively running
struct thread_queue run_queue = {.queue_head = KNULL, .queue_tail = KNULL};
static struct thread_queue exit_queue = {.queue_head = KNULL, .queue_tail = KNULL};
static thread_t* sleep_filo_head = KNULL;

static uint64_t current_timeslice = 0;

static int sched_semaphore = 0;
static int taskswitch_semaphore = 0;
static bool taskswitch_postponed = false;
// Set if we are in the idle state (used for postponing postponed task switches)
static bool is_idle = false;
static bool preempt_enabled = false;

static struct thread_queue thread_queues[PRIORITY_COUNT];
static struct thread_queue sleep_queue;
static struct thread_queue blocked_queue;
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

static void sched_timer(struct timer_dev* dev)
{
    taskswitch_disable();
    uint64_t now = dev->counter;

    // Iterate through the sleep stack
    thread_t* current_node = KNULL;
    // Use a next node in order to not use current_node->next
    thread_t* next_node = sleep_filo_head;
    sleep_filo_head = KNULL;

    while(next_node != KNULL)
    {
        current_node = next_node;
        next_node = current_node->next;

        if(now >= current_node->sleep_until)
        {
            // Don't pull the entire sleep queue along with us
            current_node->next = KNULL;

            // Wakeup task
            sched_unblock_thread(current_node);
        } else
        {
            current_node->next = sleep_filo_head;
            sleep_filo_head = current_node;
        }
    }

    // Deal with the time-slice if we aren't in the idle thread
    if(current_timeslice != 0)
    {
        // There is a time quanta (as we can't achieve a 1 ns timer resolution)

        if(current_timeslice <= dev->resolution)
            sched_switch_thread();
        else
            current_timeslice -= dev->resolution;
    }

    taskswitch_enable();
}

static void cleanup_task()
{
    while(1)
    {
        taskswitch_disable();
        struct thread_queue *queue = &exit_queue;

        if(queue->queue_head != KNULL)
        {
            while(queue->queue_head != KNULL)
            {
                thread_t* thread = queue->queue_head;
                queue->queue_head = queue->queue_head->next;
                thread_destroy(thread);
            }
        }

        sched_block_thread(STATE_SUSPENDED);
        taskswitch_enable();
    }
}

void sched_queue_remove(thread_t* thread, struct thread_queue *queue)
{
    // ???: Do we need a (spin)lock to this?
    queue->queue_head = thread->next;
    if(queue->queue_head == KNULL)
        queue->queue_tail = KNULL;

    thread->next = KNULL;
}

void sched_queue_thread_to(thread_t *thread, struct thread_queue *queue)
{
    // ???: Do we need a (spin)lock to this?
    if(queue->queue_head == KNULL)
    {
        queue->queue_head = thread;
        queue->queue_tail = thread;
    } else
    {
        queue->queue_tail->next = thread;
        queue->queue_tail = thread;
    }
}

void sched_queue_thread(thread_t *thread)
{
    sched_queue_thread_to(thread, &run_queue);
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
    if(run_queue.queue_head != KNULL)
        cleanup_thread = thread_create(run_queue.queue_head->parent, cleanup_task, PRIORITY_LOW, "cleanup_task");

    timer_add_handler(0, sched_timer);
}

void sched_sleep_until(uint64_t when)
{
    taskswitch_disable();

    uint64_t now = timer_read_counter(0);
    if(when < now)
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
    uint64_t now = timer_read_counter(0);
    sched_sleep_until(now + ns);
}

void sched_sleep_ms(uint64_t ms)
{
    sched_sleep_ns(ms * 1000000);
}

/*void sched_sleep_ms(uint64_t millis)
{
    sched_sleep_ms(millis);
    
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

    sched_block_thread(STATE_SLEEPING);
}*/

static void switch_to_thread(thread_t* next_thread)
{
    thread_t* old_thread = active_thread;

    if(taskswitch_semaphore != 0)
    {
        taskswitch_postponed = true;
        return;
    }

    // Add current thread to run queue
    if(active_thread != KNULL)
    {
        if(active_thread->current_state == STATE_RUNNING)
            active_thread->current_state = STATE_READY;

        // Requeue thread if it is only ready
        if(active_thread->current_state == STATE_READY)
            sched_queue_thread(active_thread);
    }

    active_thread = next_thread;

    if(next_thread == idle_thread)
        current_timeslice = 0;
    else
        current_timeslice = QUANTA_LENGTH;

    next_thread->current_state = STATE_RUNNING;
    mmu_set_context(next_thread->parent->page_context_base);
    switch_stack(next_thread, old_thread, next_thread->parent->page_context_base);
}

/**
 * NOTE: This must be surrounded using a sched_lock/unlock pair for sync reasons,
 *          or a taskswitch_disable/enable pair when unblocking threads (ie. during interrupts)
 */
void sched_switch_thread()
{
    if(taskswitch_semaphore != 0)
    {
        taskswitch_postponed = true;
        return;
    }

    sched_track_swaps();
    if(run_queue.queue_head != KNULL)
    {
        // We have a next thread, so switch to it
        thread_t* next_thread = run_queue.queue_head;
        run_queue.queue_head = next_thread->next;

        if(next_thread == idle_thread)
        {
            // Next thread is the idle thread, although there may be other options
            if(run_queue.queue_head != KNULL)
            {
                // Still a thread in the queue, so swap places with idle thread.
                next_thread = run_queue.queue_head;
                idle_thread->next = next_thread->next;
                run_queue.queue_head = idle_thread;

                // Should idle thread be the last thread in the queue, set it to be the tail
                if(idle_thread->next == KNULL)
                    run_queue.queue_tail = idle_thread;

                next_thread->next = KNULL;
            }
            else if (active_thread->current_state == STATE_RUNNING)
            {
                // No other threads in the queue, but the current one is still running. Just return
                sched_queue_thread(next_thread);
                return;
            }
            else {
                // Idle thread is the only one left.
                sched_queue_remove(idle_thread, &run_queue);
            }
        }
        else {
            // Remove next thread from run queue
            sched_queue_remove(next_thread, &run_queue);
        }

        switch_to_thread(next_thread);
    }
}

// Debugs below
void sched_print_queues()
{
    thread_t* node = run_queue.queue_head;
    printf("Run Queue: ");

    if(active_thread != KNULL)
        printf("[%s] -> ", active_thread->name);
    
    while(node != KNULL)
    {
        printf("%s -> ", node->name);
        node = node->next;
        // tty_reshow();
    }
    
    if(run_queue.queue_tail != KNULL)
        printf("<%s>\n", run_queue.queue_tail->name);
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

    if(run_queue.queue_head == KNULL || (active_thread == idle_thread))
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

void sched_terminate()
{
    taskswitch_disable();

    // Put thread onto exit queue
    sched_lock();
    sched_queue_thread_to(active_thread, &exit_queue);
    sched_unlock();

    // Block that thread
    sched_block_thread(STATE_EXITED);

    // Wakeup the cleaner
    sched_unblock_thread(cleanup_thread);
    taskswitch_enable();

    // In case we failed to exit
    while(1);
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
    if(taskswitch_semaphore == 0 && taskswitch_postponed)
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
    return active_thread->parent;
}

thread_t *sched_active_thread()
{
    return active_thread;
}
