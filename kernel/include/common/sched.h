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

#include <common/tasks.h>

#ifndef __SCHED_H__
#define __SCHED_H__

struct thread_queue
{
	thread_t *queue_head;
	thread_t *queue_tail;
};

void sched_init();
void sched_gc();
void sched_queue_thread(thread_t *thread);
void sched_switch_thread();
void sched_block_thread(enum thread_state state);
void sched_unblock_thread(thread_t *thread);
void sched_sleep_until(uint64_t when);
void sched_sleep_ns(uint64_t ns);
void sched_sleep(uint64_t ms);

process_t* sched_active_process();
thread_t* sched_active_thread();

// Locking methods
void sched_lock();
void sched_unlock();
void preempt_disable();
void preempt_enable();

// Deprecated methods
void sched_set_thread_state(thread_t *thread, enum thread_state state);
void sched_sleep_millis(uint64_t millis);
void switch_thread(thread_t *old_thread, thread_t *new_thread);

#endif /* __SCHED_H__ */
