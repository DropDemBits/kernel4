#include <tasks.h>

#ifndef __SCHED_H__
#define __SCHED_H__

void switch_thread(thread_t *old_thread, thread_t *new_thread);

void sched_init();
void sched_queue_thread(thread_t *thread);
void sched_set_idle_thread(thread_t *thread);
process_t* sched_active_process();
thread_t* sched_active_thread();
void sched_switch_thread();
void sched_set_thread_state(thread_t *thread, enum thread_state state);
void preempt_disable();
void preempt_enable();

#endif /* __SCHED_H__ */
