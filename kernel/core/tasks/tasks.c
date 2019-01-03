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

#include <common/mm/liballoc.h>
#include <common/mm/mm.h>
#include <common/sched/sched.h>
#include <common/tasks/tasks.h>
#include <common/ipc/message.h>

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
    init_process.child = KNULL;
    init_process.sibling = KNULL;
    init_process.parent = KNULL;
    init_process.threads = KNULL;
    init_process.name = "init";
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

process_t* process_create(const char *name)
{
    process_t *process = kmalloc(sizeof(process_t));

    process->parent = sched_active_process();
    process->child = KNULL;
    process->threads = KNULL;
    process->sibling = KNULL;
    process->name = name;
    process->pid = pid_counter++;
    process->page_context_base = mmu_create_context();

    if(process->parent != KNULL)
    {
        // Append the process to the head of the parent child list
        process->sibling = process->parent->child;
        process->parent->child = process;
    }

    return process;
}

void process_add_child(process_t *parent, thread_t *child)
{
    child->sibling = parent->threads;
    parent->threads = child;
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
    thread->pending_msgs = kmalloc(sizeof(struct ipc_message_queue));
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

    // TODO: Do something with pending messages and senders
    kfree(thread->pending_msgs);

    if(thread->parent != KNULL)
    {
        // Push back thread list
        thread_t* current = thread->parent->threads;
        thread_t* prev = KNULL;

        while(current != thread)
        {
            prev = current;
            current = current->sibling;
        }

        if(prev != KNULL)
            prev->sibling = current->sibling;
        else
            thread->parent->threads = current->sibling;

        /*if(thread->parent->threads == KNULL)
        {
            // Destroy the process
            process_destroy(thread->parent);
        }*/
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
