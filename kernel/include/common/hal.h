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

#include <common/types.h>

#ifndef __HAL_H__
#define __HAL_H__ 1

#define IRQ_HANDLED 0
#define IRQ_NOT_HANDLED 1

// Forward Decleration
struct timer_dev;
struct ic_dev;
struct intr_stack;

typedef void (*timer_handler_t)(struct timer_dev* dev);
typedef int irq_ret_t;
typedef irq_ret_t (*irq_function_t)(struct ic_dev* dev);

struct heap_info
{
    size_t base;
    size_t length;
};

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

typedef void (*ic_enable_func)(void);
typedef void (*ic_disable_func)(void);

typedef void (*ic_mask_func)(uint8_t irq);
typedef void (*ic_unmask_func)(uint8_t irq);
typedef bool (*ic_spurious_func)(uint8_t irq);
typedef void (*ic_eoi_func)(uint8_t irq);

typedef int (*ic_alloc_func)(uint8_t irq);
typedef int (*ic_free_func)(uint8_t irq);
typedef int (*ic_handle_func)(uint8_t irq, irq_function_t handler);

struct ic_dev
{
    // Default ic functions
    ic_enable_func enable;
    ic_disable_func disable;
    ic_mask_func mask;
    ic_unmask_func unmask;
    ic_spurious_func is_spurious;
    ic_eoi_func eoi;
    ic_alloc_func alloc_irq;
    ic_free_func free_irq;
    ic_handle_func handle_irq;
};

enum irq_type
{
    LEGACY,
    INTX,
    MSI,
    MSIX
};

struct irq_handler
{
    struct irq_handler* next;
    irq_function_t function;
    enum irq_type type;
    uint8_t irq;
};

void hal_init();
void hal_enable_interrupts();
void hal_disable_interrupts();
void hal_save_interrupts();
void hal_restore_interrupts();
void busy_wait();
void intr_wait();

unsigned long timer_add(struct timer_dev* device, enum timer_type type);
void timer_set_default(unsigned long id);
// For the timer related interfaces, passing an id of zero means the default counter
void timer_add_handler(unsigned long id, timer_handler_t handler_function);
void timer_broadcast_update(unsigned long id);
uint64_t timer_read_counter(unsigned long id);

// Deprecated interface
void timer_config_counter(uint16_t id, uint16_t frequency, uint8_t mode);
void timer_reset_counter(uint16_t id);
void timer_set_counter(uint16_t id, uint16_t frequency);

struct ic_dev* hal_get_ic();

// Wrappers around ic_dev
void ic_mask(uint16_t irq);
void ic_unmask(uint16_t irq);
bool ic_check_spurious(uint16_t irq);

/**
 * @brief  Sends an end of interrupt
 * @note   This should only be called from inside an irq_handler
 * @param  irq: The irq to handle
 * @retval None
 */
void ic_eoi(uint8_t irq);

/**
 * @brief  Handles an IRQ.
 * @note   The IRQ has no correlation to PC IRQs
 * @param  irq: The IRQ to handle
 * @param  type: The type of irq to handle
 * @param  handler: The handler of the irq
 * @retval The irq_handler for reference
 */
struct irq_handler* ic_irq_handle(uint8_t irq, enum irq_type type, irq_function_t handler);

/**
 * @brief  Frees a handler
 * @note   
 * @param  handler: The handler allocated from ic_irq_handle
 * @retval None
 */
void ic_irq_free(struct irq_handler* handler);

void irq_add_handler(uint16_t irq, isr_t handler);
void dump_registers(struct intr_stack *stack);

struct heap_info* get_heap_info();

#endif /* __HAL_H__ */
