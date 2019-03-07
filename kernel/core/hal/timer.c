#include <common/hal/timer.h>
#include <common/util/klog.h>
#include <common/mm/liballoc.h>

#define MAX_TIMERS 64

// ID 0 is reserved (1 = free, 0 = set)
static uint64_t timer_id_bitmap = ~1;
// Default timer (index to array below)
static unsigned int default_timer = ~0x0;
static struct timer_dev* timers[MAX_TIMERS];

// Valid timer id's are in the range of 1 - 64
static unsigned int next_timer_id()
{
    for(unsigned int i = 0; i < 64; i++)
    {
        if((timer_id_bitmap & (1ULL << i)) == 0)
            continue;
        
        timer_id_bitmap &= ~(1ULL << i);
        return i+1;
    }

    return 0;
}

void timer_init()
{
    for(int i = 0; i < MAX_TIMERS; i++)
        timers[i] = KNULL;
}

unsigned long timer_add(struct timer_dev* device, enum timer_type type)
{
    if(device == KNULL)
        return 0;
    
    device->id = next_timer_id();
    device->list_head = KNULL;

    if(!device->id)
        return 0;
    
    timers[device->id - 1] = device;

    return device->id;
}

void timer_set_default(unsigned long id)
{
    if(id == 0)
    {
        // ID 0 is reserved for default timer
        return;
    }

    default_timer = id - 1;
}

void timer_add_handler(unsigned long id, timer_handler_t handler_function)
{
    // Timer id is an index to the array, not the actual id itself
    unsigned long timer_id = id - 1;
    if(id == 0)
        timer_id = default_timer;
    if(timer_id >= MAX_TIMERS)
        return;
    if(timers[timer_id] == KNULL || timers[timer_id] == NULL)
        return;

    struct timer_handler_node* node = kmalloc(sizeof(struct timer_handler_node));
    klog_logln(LVL_INFO, "Timer handler add: %p (%d -> %p)", handler_function, id, timers[timer_id]);
    klog_logln(LVL_INFO, "Timer handler node: %p", node);
    if(node == NULL)
        return;
    
    node->handler = handler_function;
    node->next = KNULL;

    if(timers[timer_id]->list_head == KNULL)
    {
        // The list is empty, so this node begins the new list
        timers[timer_id]->list_head = node;
        return;
    }

    // We'll have to go through the entire loop to append the node
    struct timer_handler_node* current_node = KNULL;
    struct timer_handler_node* next_node = timers[timer_id]->list_head;


    while(next_node != KNULL)
    {
        current_node = next_node;
        next_node = next_node->next;
    }

    if(current_node == KNULL)
    {
        kfree(node);
        return;
    }

    current_node->next = node;
}

void timer_broadcast_update(unsigned long id)
{
    // Timer id is an index to the array, not the actual id itself
    unsigned int timer_id = id - 1;
    if(id == 0)
        timer_id = default_timer;
    if(timer_id >= MAX_TIMERS)
        return;
    if(timers[timer_id] == KNULL || timers[timer_id] == NULL)
        return;

    if(timers[timer_id]->list_head == KNULL)  // No point in going through the loop
        return;

    struct timer_handler_node* node = timers[timer_id]->list_head;

    while(node != KNULL)
    {
        node->handler(timers[timer_id]);
        node = node->next;
    }
}

uint64_t timer_read_counter(unsigned long id)
{
    unsigned int timer_id = id - 1;
    if(id == 0)
        timer_id = default_timer;
    if(timer_id >= MAX_TIMERS)
        return 0;
    if(timers[timer_id] == KNULL || timers[timer_id] == NULL)
        return 0;
    
    return timers[timer_id]->counter;
}