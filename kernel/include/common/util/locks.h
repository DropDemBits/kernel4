#ifndef __LOCKS_H__
#define __LOCKS_H__

#include <common/types.h>
#include <common/sched/sched.h>

// Forward declares
struct thread_queue;

typedef struct
{
    uint32_t value;
} spinlock_t;

// TODO: Add spinlocks to protect the mutexes and semaphores
typedef struct semaphore
{
    long count;
    long max_count;
    struct thread_queue waiting_threads;
} semaphore_t;

// TODO: Eventually add an optimized version of mutexes
typedef struct mutex
{
    long count;
    long max_count;
    struct thread_queue waiting_threads;
} mutex_t;

semaphore_t* semaphore_create(long max_count);
void semaphore_construct(semaphore_t* semaphore, long max_count);
void semaphore_destruct(semaphore_t* semaphore);
void semaphore_destroy(semaphore_t* semaphore);

void semaphore_acquire(semaphore_t* semaphore);
void semaphore_release(semaphore_t* semaphore);
bool semaphore_can_acquire(semaphore_t* semaphore);

mutex_t* mutex_create();
void mutex_construct(mutex_t* mutex);
void mutex_destruct(mutex_t* mutex);
void mutex_destroy(mutex_t* mutex);

void mutex_acquire(mutex_t* mutex);
void mutex_release(mutex_t* mutex);
bool mutex_can_acquire(mutex_t* semapore);

spinlock_t* spinlock_create();
void spinlock_destroy(spinlock_t* spinlock);
void spinlock_acquire(spinlock_t* spinlock);
void spinlock_release(spinlock_t* spinlock);
bool spinlock_can_acquire(spinlock_t* spinlock);

#endif
