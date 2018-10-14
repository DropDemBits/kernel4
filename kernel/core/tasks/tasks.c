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

#include <string.h>
#include <stdio.h>

#include <common/mm/mm.h>
#include <common/sched/sched.h>
#include <common/tasks/tasks.h>

extern void init_register_state(thread_t *thread, uint64_t *entry_point, unsigned long* kernel_stack, void* params);
extern void cleanup_register_state(thread_t *thread);
extern unsigned long bootstack_top;

void process_add_child(process_t *parent, thread_t *child);

static unsigned int pid_counter = 2;
static unsigned int tid_counter = 2;
process_t init_process = {};
static thread_t init_thread = {};

void tasks_init(char* init_name, void* init_entry)
{
    // Build init process
    init_process.page_context_base = mmu_current_context();
    init_process.child_threads = KNULL;
    init_process.child_count = 0;
    init_process.pid = 1;

    // Build init thread
    init_thread.current_state = STATE_RUNNING;
    init_thread.name = init_name;
    init_thread.parent = &init_process;
    init_thread.next = KNULL;
    init_thread.tid = 1;
    init_thread.priority = PRIORITY_NORMAL;
    init_register_state(&init_thread, init_entry, &bootstack_top, NULL);
    process_add_child(&init_process, &init_thread);
    sched_queue_thread(&init_thread);
}

process_t* process_create()
{
    process_t *process = kmalloc(sizeof(process_t));

    process->child_threads = KNULL;
    process->pid = pid_counter++;
    process->child_count = 0;
    process->page_context_base = mmu_create_context();

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

thread_t* thread_create(process_t *parent, void *entry_point, enum thread_priority priority, const char* name, void* params)
{
    thread_t *thread = kmalloc(sizeof(thread_t));
    memset(thread, 0, sizeof(thread_t));

    thread->parent = parent;
    thread->next = KNULL;
    thread->current_state = STATE_READY;
    thread->tid = tid_counter++;
    thread->priority = priority;
    thread->name = name;
    // TODO: Use a dedicated aligned stack allocator (ie. buddy)
    init_register_state(thread, entry_point, NULL, params);

    process_add_child(parent, thread);

    if(priority == PRIORITY_IDLE)
        sched_setidle(thread);

    sched_queue_thread(thread);
    return thread;
}

void thread_destroy(thread_t *thread)
{
    if(thread == KNULL) return;
    cleanup_register_state(thread);
    bool shiftback = false;

    if(thread->parent != KNULL)
    {
        for(uint64_t i = 0; i < thread->parent->child_count; i++)
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
    }

    kfree(thread);
}

/**
 * Initializes the thread state after being created for the first time
 * For the assembly entry, see switch_stack.S
 */
void initialize_thread(thread_t* thread)
{
    sched_unlock();
}
