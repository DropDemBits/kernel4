#include <common/mm/liballoc.h>
#include <common/sched/sched.h>
#include <common/util/locks.h>
#include <common/util/klog.h>
#include <arch/cpufuncs.h>

semaphore_t* semaphore_create(long max_count)
{
    semaphore_t* semaphore = kmalloc(sizeof(semaphore_t));

    if(semaphore != NULL)
        semaphore_construct(semaphore, max_count);

    return semaphore;
}

void semaphore_construct(semaphore_t* semaphore, long max_count)
{
    semaphore->count = 0;
    semaphore->max_count = max_count;
    semaphore->waiting_threads.queue_head = KNULL;
    semaphore->waiting_threads.queue_tail = KNULL;
}

void semaphore_destruct(semaphore_t* semaphore)
{
    // Release the semaphores
    taskswitch_disable();

    if (semaphore->count > 0)
        klog_logln(LVL_WARN, "Semaphore %p was destroyed while still being acquired");

    while(semaphore->count > 0)
        semaphore_release(semaphore);

    taskswitch_enable();
}

void semaphore_destroy(semaphore_t* semaphore)
{
    // Release the semaphores
    taskswitch_disable();
    semaphore_destruct(semaphore);
    kfree(semaphore);
    taskswitch_enable();
}

void semaphore_acquire(semaphore_t* semaphore)
{
    taskswitch_disable();
    if(semaphore->count < semaphore->max_count)
    {
        // Can acquire it now
        semaphore->count++;
    }
    else
    {
        // Have to wait now
        // sched_active_thread()->next;

        // Enqueue onto wait queue
        sched_queue_thread_to(sched_active_thread(), &(semaphore->waiting_threads));

        // Use the general-purpose suspended state
        sched_block_thread(STATE_SUSPENDED);
    }
    taskswitch_enable();
}

void semaphore_release(semaphore_t* semaphore)
{
    taskswitch_disable();
    if(semaphore->waiting_threads.queue_head != KNULL)
    {
        // There is a waiting thread
        // We don't need to decrement the count, as it will be immediately re-acquired
        thread_t* thread = semaphore->waiting_threads.queue_head;
        sched_queue_remove(thread, &(semaphore->waiting_threads));
        sched_unblock_thread(thread);
    }
    else
    {
        // No-one else is waiting for the lock, so decrement
        semaphore->count--;
    }
    taskswitch_enable();
}

bool semaphore_can_acquire(semaphore_t* semaphore)
{
    return semaphore->count < semaphore->max_count;
}

mutex_t* mutex_create()
{
    return (mutex_t*)semaphore_create(1);
}

// Since mutex are essentially semaphores, the cast is allowed
void mutex_destroy(mutex_t* mutex)
{
    semaphore_destroy((semaphore_t*)mutex);
}

void mutex_acquire(mutex_t* mutex)
{
    semaphore_acquire((semaphore_t*)mutex);
}

void mutex_construct(mutex_t* mutex)
{
    semaphore_construct((semaphore_t*)mutex, 1L);
}

void mutex_destruct(mutex_t* mutex)
{
    semaphore_destruct((semaphore_t*)mutex);
}

void mutex_release(mutex_t* mutex)
{
    semaphore_release((semaphore_t*)mutex);
}

bool mutex_can_acquire(mutex_t* mutex)
{
    // Simplifies to the inverted count
    return !mutex->count;
}

spinlock_t* spinlock_create()
{
    spinlock_t* lock = kmalloc(sizeof(lock));
    lock->value = 0;
    return lock;
}

void spinlock_destroy(spinlock_t* spinlock)
{
    kfree(spinlock);
}

void spinlock_acquire(spinlock_t* spinlock)
{
    uint32_t value = 1;
    while(value)
        value = lock_cmpxchg(&(spinlock->value), 0, 1);
}

void spinlock_release(spinlock_t* spinlock)
{
    lock_xchg(&spinlock->value, 0);
}

bool spinlock_can_acquire(spinlock_t* spinlock)
{
    return lock_read(&spinlock->value) == 0;
}
