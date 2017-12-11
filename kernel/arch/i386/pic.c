#include <pic.h>
#include <io.h>

#define PIC1_CMD   0x20
#define PIC1_DATA  0x21
#define PIC2_CMD   0xA0
#define PIC2_DATA  0xA1

// ICWs
#define ICW1_INIT 0x10 // Init start
#define ICW1_IC4  0x01 // (Don't) Read ICW4
#define ICW1_SNGL 0x02 // Single (Cascade) Mode
#define ICW1_ADI  0x04 // 8bit (4bit) CALL Interval
#define ICW1_LTIM 0x08 // (Not) Level interrupt mode
// ICW2 is on DATA port (Vector base)
// ICW3 is on DATA port (Cascade configure)
// ICW4 is on DATA port (Vector base)
#define ICW4_MPM  0x01 // MCS (8086) Mode
#define ICW4_AEOI 0x02 // Normal (Auto) EOI
#define ICW4_BUF  0x04 // (Buffered)/Unbuffered Mode
#define ICW4_MSS  0x08 // (Master)/Slave Type
#define ICW4_SFNM 0x01 // Specially fully nested mode

// OCWs
#define OCW2_EOI  0x20 // EOI current IRQ
#define OCW2_SEL  0x40 // Select specific IRQ
#define OCW2_ROT  0x80 // Rotate priorities

#define OCW3_RREG 0x01 // Read Register
#define OCW3_R_IS 0x02 // Read Interrupt Status Register
#define OCW3_POLL 0x04 // Poll command
#define OCW3_WREN 0x08 // Write to OCW3
#define OCW3_SSMM 0x20 // Set Special Mask
#define OCW3_ESMM 0x40 // Enable writes to Special Mask

uint16_t pic_read_irr()
{
	uint16_t irr = 0;
	outb(PIC1_CMD, OCW3_WREN | OCW3_RREG);
	irr |= inb(PIC1_CMD);
	outb(PIC2_CMD, OCW3_WREN | OCW3_RREG);
	irr |= inb(PIC2_CMD) << 8;
	return irr;
}

void pic_init()
{
	// Save Masks
	uint8_t mask_a = inb(PIC1_DATA);
	uint8_t mask_b = inb(PIC2_DATA);

	// (ICW1) Init PICS
	outb(PIC1_CMD, ICW1_INIT | ICW1_IC4);
	io_wait();
	outb(PIC2_CMD, ICW1_INIT | ICW1_IC4);
	io_wait();

	// (ICW2) Set vector bases
	outb(PIC1_DATA, 0x20);
	io_wait();
	outb(PIC2_DATA, 0x28);
	io_wait();

	// (ICW3) Cascade Setup
	outb(PIC1_DATA, 0b00000100);
	io_wait();
	outb(PIC2_DATA, 0x02);
	io_wait();

	// (ICW4) 8086 Mode
	// Do we need AEOI?
	outb(PIC1_DATA, ICW4_MPM);
	io_wait();
	outb(PIC2_DATA, ICW4_MPM);
	io_wait();

	// Restore Masks
	outb(PIC1_DATA, mask_a);
	outb(PIC2_DATA, mask_b);
}

void pic_mask(uint8_t irq)
{
	uint8_t port;
	if(irq >= 8)
	{
		port = PIC2_DATA;
		irq -= 8;
	} else
	{
		port = PIC1_DATA;
	}

	uint8_t mask = inb(port) | (1 << irq);
	outb(port, mask);
}

void pic_unmask(uint8_t irq)
{
	uint8_t port;
	if(irq >= 8)
	{
		port = PIC2_DATA;
		irq -= 8;
	} else
	{
		port = PIC1_DATA;
	}

	uint8_t mask = inb(port) & ~(1 << irq);
	outb(port, mask);
}

bool pic_check_spurious(uint8_t irq)
{
	if(irq != 7 && irq != 15) return false;
	uint16_t irr = pic_read_irr();

	if(irq == 7  && ~(irr & 0x08))
	{
		return true;
	} else if(irq == 15 && ~(irr & 0x80))
	{
		outb(PIC1_CMD, OCW2_EOI);
		return true;
	}
	return false;
}

void pic_eoi(uint8_t irq)
{
	if(irq >= 8) outb(PIC2_CMD, OCW2_EOI);
	outb(PIC1_CMD, OCW2_EOI);
}
