#include <common/ipc/message.h>
#include <common/mm/liballoc.h>
#include <common/sched/sched.h>

static void msg_enqueue(struct ipc_message_queue* queue, struct ipc_message* msg)
{
    // Queue is full, don't do it
    if(queue->count >= NUM_MSGS)
        return;

    queue->messages[queue->tail++] = msg;
    queue->count++;
    queue->tail %= NUM_MSGS;
}

static struct ipc_message* peek_queue(struct ipc_message_queue* queue)
{
    if(queue->count == 0)
        return NULL;

    return queue->messages[queue->head];
}

static struct ipc_message* msg_dequeue(struct ipc_message_queue* queue)
{
    if(queue->count == 0)
        return NULL;

    struct ipc_message* msg = queue->messages[queue->head++];
    queue->count--;
    queue->head %= NUM_MSGS;

    return msg;
}

void build_msg(struct ipc_message* msg, uint32_t type, void* data, size_t len)
{
    if(msg == NULL)
        return;

    msg->type = type;
    msg->data = data;
    msg->payload_len = len;
}

static int msg_send_async(thread_t* target, struct ipc_message* msg, uint32_t flags, uint32_t sequence)
{
    if(target == NULL)
        return -1;

    taskswitch_disable();
    // Setup message
    msg->src = sched_active_thread();
    msg->sequence = sequence;

    if(((struct ipc_message_queue*)target->pending_msgs)->count == NUM_MSGS)
    {
        // Append to waiting queue
        sched_queue_thread_to(msg->src, &target->pending_senders);
        sched_block_thread(STATE_BLOCKED);
    }

    // Send the message and wait
    msg_enqueue(target->pending_msgs, msg);
    if(target->current_state > STATE_READY)
        sched_unblock_thread(target);
    taskswitch_enable();
    return 0;
}

static int msg_recv_async(uint32_t expected_type, struct ipc_message** dest, uint32_t flags, uint32_t sequence)
{
    struct ipc_message* msg;
    thread_t* this_thread = sched_active_thread();
    thread_t* pending_thread;

    taskswitch_disable();
    // If there aren't any messages, wait
    if(((struct ipc_message_queue*)this_thread->pending_msgs)->count == 0)
        sched_block_thread(STATE_BLOCKED);
    
    msg = msg_dequeue(this_thread->pending_msgs);

    // Wake up all pending senders
    pending_thread = this_thread->pending_senders.queue_head;
    while(pending_thread != KNULL)
    {
        sched_queue_remove(pending_thread, &this_thread->pending_senders);
        sched_unblock_thread(pending_thread);
        pending_thread = this_thread->pending_senders.queue_head;
    }

    taskswitch_enable();
    if(dest)
        *dest = msg;

    return 0;
}

int msg_send(thread_t* target, struct ipc_message* msg, uint32_t flags, uint32_t sequence)
{
    if(sequence & MSG_XACT_ASYNC == MSG_XACT_ASYNC)
    {
        return msg_send_async(target, msg, flags, sequence);
    }
    else
    {
        // Synchronous send
        int status = 0;

        status = msg_send_async(target, msg, flags, sequence);
        if(!status)
            return status;

        // Wait for ACK message
        status = msg_recv_async(MSG_TYPE_ACK, NULL, 0, 0);
        if(!status)
            return status;

        return 0;
    }

    return -1;
}

int msg_recv(uint32_t expected_type, struct ipc_message** dest, uint32_t flags, uint32_t sequence)
{
    if(sequence & MSG_XACT_ASYNC == MSG_XACT_ASYNC)
    {
        return msg_recv_async(expected_type, dest, flags, sequence);
    }
    else if(sequence & MSG_XACT_ASYNC != MSG_XACT_ASYNC)
    {
        // Synchronous recieve
        int status = 0;
        struct ipc_message* tmp;
        
        status = msg_recv_async(expected_type, &tmp, flags, sequence);
        if(!status)
            return status;

        // Construct & send ACK
        struct ipc_message* ack = kmalloc(sizeof(struct ipc_message));

        build_msg(ack, MSG_TYPE_ACK, NULL, 0);
        status = msg_send_async(tmp->src, ack, 0, 0);
        if(!status)
            return status;

        if(dest)
            *dest = tmp;
        return status;
    }

    return -1;
}

struct ipc_message* msg_peek(uint32_t expected_type, uint32_t sequence)
{

}
