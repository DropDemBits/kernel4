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

#ifndef __HAL_H__
#define __HAL_H__ 1

#include <common/types.h>
#include <arch/cpufuncs.h>

#define IRQ_HANDLED 0
#define IRQ_NOT_HANDLED 1

#define IC_MODE_PIC  0
#define IC_MODE_IOAPIC 1

// Forward Decleration
struct irq_handler;
struct intr_stack;

typedef int irq_ret_t;
typedef irq_ret_t (*irq_function_t)(struct irq_handler* handler);

struct heap_info
{
    size_t base;
    size_t length;
};

#define INT_EOI_FAST    0x00000001  // EOIs are handled in the IC
#define INT_EOI_NORMAL  0x00000000  // EOIs are handled in the handler
#define INT_SRC_LEGACY  (0 << 1)    
#define INT_SRC_INTX    (1 << 1)    
#define INT_SRC_MSI     (2 << 1)    
#define INT_SRC_MSIX    (3 << 1)    
#define INT_SRC_MASK    0x00000006  
#define INT_TRG_LEVEL   0x00000008  // Interrupt source is level-triggered
#define INT_SHARED      0x00000010  // Interrupt can be shared with other IRQ handlers

typedef void (*ic_enable_func)(uint8_t irq_base);
typedef void (*ic_disable_func)(uint8_t disable_base);

typedef void (*ic_mask_func)(uint8_t irq);
typedef void (*ic_unmask_func)(uint8_t irq);
typedef bool (*ic_spurious_func)(uint8_t irq);
typedef void (*ic_eoi_func)(uint8_t irq);

typedef struct irq_handler* (*ic_handle_func)(uint8_t irq, uint32_t flags, irq_function_t handler);
typedef void (*ic_free_func)(struct irq_handler* handler);

struct ic_dev
{
    // Default ic functions
    ic_enable_func enable;
    ic_disable_func disable;
    ic_mask_func mask;
    ic_unmask_func unmask;
    ic_spurious_func is_spurious;
    ic_eoi_func eoi;
    ic_handle_func handle_irq;
    ic_free_func free_irq;
};

struct irq_handler
{
    struct irq_handler* next;
    struct ic_dev* ic;
    irq_function_t function;
    uint32_t flags;
    uint8_t interrupt;
};

void hal_init();

struct ic_dev* hal_get_ic();

// Wrappers around ic_dev
void ic_mask(uint16_t irq);
void ic_unmask(uint16_t irq);
bool ic_check_spurious(uint16_t irq);

/**
 * @brief  Handles an IRQ.
 * @note   The IRQ has no correlation to PC IRQs
 * @param  irq: The IRQ to handle
 * @param  flags: The flags for the IRQ handler
 * @param  handler: The handler of the irq
 * @retval The irq_handler for reference
 */
struct irq_handler* ic_irq_handle(uint8_t irq, uint32_t flags, irq_function_t handler);

/**
 * @brief  Frees a handler
 * @note   
 * @param  handler: The handler allocated from ic_irq_handle
 * @retval None
 */
void ic_irq_free(struct irq_handler* handler);

struct heap_info* get_heap_info();
void* get_klog_base();

#endif /* __HAL_H__ */
