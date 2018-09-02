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

#define TM_MODE_INTERVAL 1
#define TM_MODE_ONESHOT  2

struct intr_stack;

struct heap_info
{
    size_t base;
    size_t length;
};

void hal_init();
void hal_enable_interrupts();
void hal_disable_interrupts();
void hal_save_interrupts();
void hal_restore_interrupts();
void busy_wait();
void intr_wait();

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
