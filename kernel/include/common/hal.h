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

#include <common/locks.h>
#include <common/types.h>

#ifndef __HAL_H__
#define __HAL_H__ 1

typedef void (*timer_handler_t)(struct timer_dev* dev);

struct intr_stack;

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

void ic_mask(uint16_t irq);
void ic_unmask(uint16_t irq);
void ic_check_spurious(uint16_t irq);
void ic_eoi(uint16_t irq);

void irq_add_handler(uint16_t irq, isr_t handler);
void dump_registers(struct intr_stack *stack);

struct heap_info* get_heap_info();

#endif /* __HAL_H__ */
