#include <common/types.h>

#ifndef __PIT_H__
#define __PIT_H__

void pit_init();

// Deprecated interface
void pit_init_counter(uint16_t id, uint16_t frequency, uint8_t mode);
void pit_reset_counter(uint16_t id);
void pit_set_counter(uint16_t id, uint16_t frequency);

#endif /* __PIT_H__ */
