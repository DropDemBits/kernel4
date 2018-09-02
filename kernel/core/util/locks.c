#include <common/locks.h>

semaphore_t* semaphore_create(long max_count)
{
    semaphore_t* semaphore = kmalloc(sizeof(semaphore_t));

    if(semaphore != NULL)
    {
        semaphore->count = 0;
        semaphore->max_count = max_count;
        semaphore->waiting_threads.queue_head = KNULL;
        semaphore->waiting_threads.queue_tail = KNULL;
    }
}

void semaphore_destroy(semaphore_t* semaphore)
{
    // Release the semaphores
    while(semaphore->count > 0)
        semaphore_release(semaphore);
    
    taskswitch_disable();
    kfree(semaphore);
    taskswitch_enable();
}

void semaphore_acquire(semaphore_t* semapore)
{
    taskswitch_disable();
    if(semapore->count < semapore->max_count)
    {
        // Can acquire it now
        semapore->count++;
    }
    else
    {
        // Have to wait now
        sched_active_thread()->next;

        // Enqueue onto wait queue
        sched_queue_thread_to(sched_active_thread(), &(semapore->waiting_threads));

        // Use the general-purpose suspended state
        sched_block_thread(STATE_SUSPENDED);
    }
    taskswitch_enable();
}

void semaphore_release(semaphore_t* semapore)
{
    taskswitch_disable();
    if(semapore->waiting_threads.queue_head != KNULL)
    {
        // There is a waiting thread
        // We don't need to decrement the count, as it will be immediately re-acquired
        thread_t* thread = semapore->waiting_threads.queue_head;
        sched_queue_remove(thread, &(semapore->waiting_threads));
        sched_unblock_thread(thread);
    }
    else
    {
        // No-one else is waiting for the lock, so decrement
        semapore->count--;
    }
    taskswitch_enable();
}

mutex_t* mutex_create()
{
    return semaphore_create(1);
}

void mutex_destroy(mutex_t* mutex)
{
    semaphore_destroy(mutex);
}

void mutex_acquire(mutex_t* mutex)
{
    semaphore_acquire(mutex);
}

void mutex_release(mutex_t* mutex)
{
    semaphore_release(mutex);
}
