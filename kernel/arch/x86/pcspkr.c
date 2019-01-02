#include <common/types.h>
#include <common/util/klog.h>

#include <arch/pit.h>
#include <arch/io.h>

#define KBD_SPKRGATE                0x61
#define PIT_PCSPKR                  1
#define PIT_MODE_HARDWARE_ONESHOT   (1 << 1) // Start upon gate input (Speaker port only)
#define PIT_MODE_HARDWARE_STROBE    (5 << 1) // Retriggerable delay activated by gate input (Speaker port only)

void pcspkr_set_enable(bool enabled)
{
    klog_logln(DEBUG, "PGATE: %x", inb(KBD_SPKRGATE));
    uint8_t temp = inb(KBD_SPKRGATE) & 0xFC;
    temp |= 0x3 * (enabled & 1);
    outb(KBD_SPKRGATE, temp);
    klog_logln(DEBUG, "SGATE: %x", inb(KBD_SPKRGATE));
}

void pcspkr_write_freq(uint16_t freq)
{
    pit_set_counter(PIT_PCSPKR, freq);
}

void pcspkr_init()
{
    pcspkr_set_enable(false);
    pit_init_counter(PIT_PCSPKR, 440, PIT_MODE_HARDWARE_STROBE);
}
