#include <types.h>

#ifndef __HAL_H__
#define __HAL_H__ 1

#define TM_MODE_INTERVAL 1
#define TM_MODE_ONESHOT  2

void hal_init();

void timer_config_counter(uint16_t id, uint16_t frequency, uint8_t mode);
void timer_set_counter(uint16_t id, uint64_t value);

void ic_mask(uint16_t irq);
void ic_unmask(uint16_t irq);
void ic_check_spurious(uint16_t irq);
void ic_eoi(uint16_t irq);

void irq_add_handler(uint16_t irq, isr_t handler);

#endif /* __HAL_H__ */
