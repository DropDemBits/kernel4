#include <hal.h>
#include <pic.h>
#include <idt.h>
#include <stack_state.h>

extern void irq_common(struct intr_stack *frame);

static bool use_apic = false;

static bool apic_exists()
{
	uint32_t edx;
	asm volatile("cpuid":"=d"(edx):"a"(1));
	return (edx >> 9) & 0x1;
}

void hal_init()
{
	// Check for APIC
	if(apic_exists())
	{
		// TODO: Enable & init apic
		//outb(PIC1_DATA, 0xFF);
		//outb(PIC2_DATA, 0xFF);
	}
	pic_init();

	for(int i = 0; i < 16; i++)
		irq_add_handler(i, irq_common);

	timer_config_counter(0, 1000, TM_MODE_INTERVAL);

	if(!use_apic)
	{
		// Mask PIT & RTC
		pic_mask(0);
		pic_mask(8);
		asm("sti");
		while(pic_read_irr() & ~(0x11));
		asm("cli");
		pic_unmask(0);
		pic_unmask(8);
	}
}

void hal_enable_interrupts()
{
	asm volatile("sti");
}

void timer_config_counter(uint16_t id, uint16_t frequency, uint8_t mode)
{
	pit_init_counter(id, frequency, mode);
}

void timer_reset_counter(uint16_t id)
{
	pit_reset_counter(id);
}

void timer_set_counter(uint16_t id, uint16_t frequency)
{
	pit_set_counter(id, frequency);
}

void ic_mask(uint16_t irq)
{
	pic_mask(irq);
}

void ic_unmask(uint16_t irq)
{
	pic_unmask(irq);
}

void ic_check_spurious(uint16_t irq)
{
	pic_check_spurious(irq);
}

void ic_eoi(uint16_t irq)
{
	pic_eoi(irq);
}

void irq_add_handler(uint16_t irq, isr_t handler)
{
	isr_add_handler(irq + 32, handler);
}
