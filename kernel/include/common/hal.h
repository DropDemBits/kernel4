#include <types.h>

#ifndef __HAL_H__
#define __HAL_H__ 1

#define TM_MODE_INTERVAL 1
#define TM_MODE_ONESHOT  2

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

void timer_config_counter(uint16_t id, uint16_t frequency, uint8_t mode);
void timer_reset_counter(uint16_t id);
void timer_set_counter(uint16_t id, uint16_t frequency);

void ic_mask(uint16_t irq);
void ic_unmask(uint16_t irq);
void ic_check_spurious(uint16_t irq);
void ic_eoi(uint16_t irq);

void irq_add_handler(uint16_t irq, isr_t handler);

struct heap_info* get_heap_info();

#endif /* __HAL_H__ */
