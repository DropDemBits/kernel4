#ifndef __IPC_MSG_H__
#define __IPC_MSG_H__ 1

#include <common/types.h>
#include <common/tasks/tasks.h>

#define MSG_QUEUE_SIZE 8

// Message Types
#define MSG_TYPE_ACK    0
#define MSG_TYPE_DATA   1

#define MSG_XACT_SYNC   0x00000000
#define MSG_XACT_ASYNC  0x00000001

struct ipc_message
{
    thread_t* src;
    thread_t* dest;
    uint32_t type;
    uint32_t sequence;
    size_t payload_len;
    void* data;
};

struct ipc_message_queue
{
    // Circular buffer of messages
    size_t head;
    size_t tail;
    size_t count;
    struct ipc_message* messages[MSG_QUEUE_SIZE];
};

/**
 * @brief  Constructs a message with the specified parameters
 * @note   
 * @param  msg: The message to build
 * @param  type: The type of message to send
 * @param  data: The payload of the message
 * @param  len: The length of the message
 * @retval None
 */
void build_msg(struct ipc_message* msg, uint32_t type, void* data, size_t len);

/**
 * @brief  Sends a message to the specified thread
 * @note   
 * @param  target: The target of the message
 * @param  msg: The message to send
 * @param  flags: Flags used to modify the transaction
 * @param  sequence: The sequence number of the message
 * @retval 0 if everything went okay
 */
int msg_send(thread_t* target, struct ipc_message* msg, uint32_t flags, uint32_t sequence);

/**
 * @brief  Gets a message from the queue and removes it.
 * @note   Blocks if there's nothing on the queue and MSG_XACT_ASYNC is in the flags
 * @param  expected_type: The expected message type
 * @param  msg: The location to store the message pointer to
 * @param  flags: The reciving flags
 * @param  sequence: The expected sequence number of the message
 * @retval 0 if everything went okay
 */
int msg_recv(uint32_t expected_type, struct ipc_message** msg, uint32_t flags, uint32_t sequence);

/**
 * @brief  Gets a message without removing it from the queue
 * @note   
 * @param  expected_type: The expected message type to recieve
 * @param  sequence: The expected sequence number of the message
 * @retval The pointer to the requested message, or NULL if there isn't any
 */
struct ipc_message* msg_peek(uint32_t expected_type, uint32_t sequence);

/**
 * @brief  Initializes the given message queue
 * @note   
 * @param  queue: The queue to initialize
 * @retval None
 */
void msg_queue_init(struct ipc_message_queue* queue);

#endif /* __IPC_MSG_H__ */
