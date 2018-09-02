#include <common/sched.h>

#ifndef __LOCKS_H__
#define __LOCKS_H__

// TODO: Add spinlocks to protect the mutexes and semaphores
typedef struct
{
    long count;
    long max_count;
    struct thread_queue waiting_threads;
} semaphore_t;

// TODO: Eventually add an optimized version of mutexs
typedef semaphore_t mutex_t;

semaphore_t* semaphore_create(long max_count);
void semaphore_destroy(semaphore_t* semaphore);

void semaphore_acquire(semaphore_t* semapore);
void semaphore_release(semaphore_t* semapore);
bool semaphore_can_acquire(semaphore_t* semapore);

mutex_t* mutex_create();
void mutex_destroy(mutex_t* mutex);

void mutex_acquire(mutex_t* mutex);
void mutex_release(mutex_t* mutex);
bool mutex_can_acquire(mutex_t* semapore);

#endif
