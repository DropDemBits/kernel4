#include <common/types.h>

#ifndef __HAL_TIMER_H__
#define __HAL_TIMER_H__ 1

struct timer_dev;
typedef void (*timer_handler_t)(struct timer_dev* dev);

enum timer_type
{
    PERIODIC,
    ONESHOT,
};

struct timer_handler_node
{
    struct timer_handler_node* next;
    timer_handler_t handler;
};

struct timer_dev
{
    // These two need to be updated by the timer itself
    uint64_t resolution;        // Resolution of the timer, in nanos
    uint64_t counter;           // Current count of the timer, in nanos

    // These two are managed by the HAL layer
    unsigned long id;
    struct timer_handler_node* list_head;
};

void timer_init();
unsigned long timer_add(struct timer_dev* device, enum timer_type type);
void timer_set_default(unsigned long id);
// For the timer related interfaces, passing an id of zero means the default counter
void timer_add_handler(unsigned long id, timer_handler_t handler_function);
void timer_broadcast_update(unsigned long id);
uint64_t timer_read_counter(unsigned long id);

#endif