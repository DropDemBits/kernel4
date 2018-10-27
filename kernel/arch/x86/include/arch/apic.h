#include <common/types.h>

#ifndef __APIC_H__
#define __APIC_H__ 1

#define APIC_DELMODE_FIXED          0
#define APIC_DELMODE_LOWEST_PRIO    1
#define APIC_DELMODE_SMI            2

#define APIC_DELMODE_NMI            4
#define APIC_DELMODE_INIT           5
#define APIC_DELMODE_SIPI           6
#define APIC_DELMODE_EXTINT         7

#define IOAPIC_POLARITY_HIGH 0
#define IOAPIC_POLARITY_LOW  1
#define IOAPIC_TRIGGER_EDGE  0
#define IOAPIC_TRIGGER_LEVEL 1

#define IOAPIC_INVALID_INT_NUM 0xFFFFFFFF

/**
 * @brief  Initializes the processor Local APIC
 * @note   There may exist multiple Local APIC in a system (eg. per-core) 
 * @param  phybase: The physical base of the APIC
 * @retval None
 */
void apic_init(uint64_t phybase);

void apic_set_lint_entry(uint8_t lint_entry, uint8_t polarity, uint8_t trigger_mode, uint8_t delivery_mode);

/**
 * @brief  Initializes an IOAPIC
 * @note   
 * @param  phybase: The physical base of the IO APIC
 * @param  irq_base: The interrupt base of the IO APic
 * @retval None
 */
void ioapic_init(uint64_t phybase, uint32_t irq_base);

/**
 * @brief  Routes a global IRQ to the specified bus IRQ
 * @note   
 * @param  global_source: The source global IRQ
 * @param  bus_source: The bus IRQ to map to
 * @param  polarity: The polarity of the mapped IRQ
 * @param  trigger_mode: The trigger mode of the mapped IRQ
 * @retval None
 */
void ioapic_route_line(uint32_t global_source, uint32_t bus_source, uint8_t polarity, uint8_t trigger_mode);

uint32_t ioapic_map_irq(uint8_t isa_irq);

struct ic_dev* ioapic_get_dev();
#endif