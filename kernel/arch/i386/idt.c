#include <hal.h>
#include <idt.h>
#include <tty.h>
#include <stack_state.h>

#define IDT_TYPE_TASK			0x5
#define IDT_TYPE_INTERRUPT16	0x6
#define IDT_TYPE_TRAP16 		0x7
#define IDT_TYPE_INTERRUPT		0xE
#define IDT_TYPE_TRAP	 		0xF

extern void isr0_entry(); // Divide By 0
extern void isr1_entry(); // Debug
extern void isr2_entry(); // NMI
extern void isr3_entry(); // Breakpoint
extern void isr4_entry(); // Overflow
extern void isr5_entry(); // Bound Range
extern void isr6_entry(); // Invalid Opcode
extern void isr7_entry(); // No Coprocessor
extern void isr8_entry(); // Double Fault
// Reserved 9 (Coprocessor Segment Overrun)
extern void isr10_entry(); // Invalid TSS
extern void isr11_entry(); // Segment Not Present
extern void isr12_entry(); // Stack Fault
extern void isr13_entry(); // General Protection Fault
extern void isr14_entry(); // Page Fault
// Reserved 15
extern void isr16_entry(); // Floating Point Exception
extern void isr17_entry(); // Alignment Check
extern void isr18_entry(); // Machine Check
extern void isr19_entry(); // SIMD FP Exception
extern void isr20_entry(); // Virtualization Exception (AMD64 Reserved)
// Reserved 21-28
extern void isr29_entry(); // Virtualization Exception (Intel Reserved)
extern void isr30_entry(); // Security Exception (Intel Reserved)

extern void irq0_entry();
extern void irq1_entry();
extern void irq2_entry();
extern void irq3_entry();
extern void irq4_entry();
extern void irq5_entry();
extern void irq6_entry();
extern void irq7_entry();
extern void irq8_entry();
extern void irq9_entry();
extern void irq10_entry();
extern void irq11_entry();
extern void irq12_entry();
extern void irq13_entry();
extern void irq14_entry();
extern void irq15_entry();
extern void serial_write(const char* str);
extern void halt();

typedef struct
{
	uint16_t offset_low;
	uint16_t selector;
	uint8_t rsv;
	uint8_t type : 4;
	uint8_t zero : 1;
	uint8_t dpl : 2;
	uint8_t present : 1;
	uint16_t offset_high;
} __attribute__((__packed__)) idt_descriptor_t;
extern idt_descriptor_t idt_table;

const char* fault_names[] = {
	"Division by 0",
	"Debug Exception",
	"NMI Exception",
	"Breakpoint",
	"Overflow Flag Set",
	"Bound Range Exceeded",
	"Invalid Opcode",
	"No Coprocessor",
	"Double Fault",
	"Reserved Fault",
	"Invalid TSS",
	"Segment Not Present",
	"Stack Fault",
	"General Protection Fault",
	"Page Fault",
	"Reserved Fault",
	"Floating Point Exception",
	"Alignment Check Exception",
	"Machine Check Exception",
	"SIMD/FP Exception",
	"VMX Virtualization Exception", // AMD Reserved
	"Control Protection Exception", // AMD Reserved
	"Reserved Fault",
	"Reserved Fault",
	"Reserved Fault",
	"Reserved Fault",
	"Reserved Fault",
	"Reserved Fault",
	"Reserved Fault",
	"SVM Virtualization Exception", // Intel Reserved
	"SVM Security Exception",       // Intel Reserved
	"Reserved Fault",
};
isr_function_t function_table[256];

static void create_descriptor(	uint8_t index,
								uint32_t base,
								uint16_t selector,
								uint8_t type)
{
	//asm volatile ("xchg %%bx, %%bx"::"a"(&idt_table),"d"(&idt_table));
	(&idt_table)[index].offset_low =  (base >> 0) & 0xFFFF;
	(&idt_table)[index].offset_high = (base >> 16) & 0xFFFF;
	(&idt_table)[index].selector = selector & ~0x7;
	(&idt_table)[index].type = type;
	(&idt_table)[index].dpl = selector & 0x3;
	(&idt_table)[index].zero = 0;
	(&idt_table)[index].rsv = 0;
	(&idt_table)[index].present = 1;
}

void isr_add_handler(uint8_t index, isr_t function)
{
	function_table[index].pointer = function;
}

void isr_common(struct intr_stack *frame)
{
	if(function_table[frame->int_num].pointer == KNULL && frame->int_num < 32)
	{
		// Do Generic Fault
		tty_prints("An exception has occured in the kernel\n");
		tty_prints("The exception was \"");
		tty_prints(fault_names[frame->int_num]);
		tty_prints("\"\n");
		halt();
	}
	else {
		function_table[frame->int_num].pointer(frame);
	}
}

void irq_common(struct intr_stack *frame)
{
	ic_eoi(frame->int_num - 32);
}

void setup_idt()
{
	for(int i = 0; i < 256; i++)
	{
		function_table[i].pointer = KNULL;
	}

	create_descriptor( 0, (uint32_t) isr0_entry, 0x08, IDT_TYPE_INTERRUPT);
	create_descriptor( 1, (uint32_t) isr1_entry, 0x08, IDT_TYPE_INTERRUPT);
	create_descriptor( 2, (uint32_t) isr2_entry, 0x08, IDT_TYPE_INTERRUPT);
	create_descriptor( 3, (uint32_t) isr3_entry, 0x08, IDT_TYPE_INTERRUPT);
	create_descriptor( 4, (uint32_t) isr4_entry, 0x08, IDT_TYPE_INTERRUPT);
	create_descriptor( 5, (uint32_t) isr5_entry, 0x08, IDT_TYPE_INTERRUPT);
	create_descriptor( 6, (uint32_t) isr6_entry, 0x08, IDT_TYPE_INTERRUPT);
	create_descriptor( 7, (uint32_t) isr7_entry, 0x08, IDT_TYPE_INTERRUPT);
	create_descriptor( 8, (uint32_t) isr8_entry, 0x08, IDT_TYPE_INTERRUPT);
	create_descriptor(10, (uint32_t)isr10_entry, 0x08, IDT_TYPE_INTERRUPT);
	create_descriptor(11, (uint32_t)isr11_entry, 0x08, IDT_TYPE_INTERRUPT);
	create_descriptor(12, (uint32_t)isr12_entry, 0x08, IDT_TYPE_INTERRUPT);
	create_descriptor(13, (uint32_t)isr13_entry, 0x08, IDT_TYPE_INTERRUPT);
	create_descriptor(14, (uint32_t)isr14_entry, 0x08, IDT_TYPE_INTERRUPT);
	// RSV 15
	create_descriptor(16, (uint32_t)isr16_entry, 0x08, IDT_TYPE_INTERRUPT);
	create_descriptor(17, (uint32_t)isr17_entry, 0x08, IDT_TYPE_INTERRUPT);
	create_descriptor(18, (uint32_t)isr18_entry, 0x08, IDT_TYPE_INTERRUPT);
	create_descriptor(19, (uint32_t)isr19_entry, 0x08, IDT_TYPE_INTERRUPT);
	// RSV 20-28
	create_descriptor(29, (uint32_t)isr29_entry, 0x08, IDT_TYPE_INTERRUPT);
	create_descriptor(30, (uint32_t)isr30_entry, 0x08, IDT_TYPE_INTERRUPT);
	// RSV 31
	// IRQs
	// Master PIC
	create_descriptor(32, (uint32_t) irq0_entry, 0x08, IDT_TYPE_INTERRUPT);
	create_descriptor(33, (uint32_t) irq1_entry, 0x08, IDT_TYPE_TRAP);
	create_descriptor(34, (uint32_t) irq2_entry, 0x08, IDT_TYPE_TRAP);
	create_descriptor(35, (uint32_t) irq3_entry, 0x08, IDT_TYPE_TRAP);
	create_descriptor(36, (uint32_t) irq4_entry, 0x08, IDT_TYPE_TRAP);
	create_descriptor(37, (uint32_t) irq5_entry, 0x08, IDT_TYPE_TRAP);
	create_descriptor(38, (uint32_t) irq6_entry, 0x08, IDT_TYPE_TRAP);
	// Spurious #1
	create_descriptor(39, (uint32_t) irq7_entry, 0x08, IDT_TYPE_TRAP);
	// Slave PIC
	create_descriptor(40, (uint32_t) irq8_entry, 0x08, IDT_TYPE_TRAP);
	create_descriptor(41, (uint32_t) irq9_entry, 0x08, IDT_TYPE_TRAP);
	create_descriptor(42, (uint32_t)irq10_entry, 0x08, IDT_TYPE_TRAP);
	create_descriptor(43, (uint32_t)irq11_entry, 0x08, IDT_TYPE_TRAP);
	create_descriptor(44, (uint32_t)irq12_entry, 0x08, IDT_TYPE_TRAP);
	create_descriptor(45, (uint32_t)irq13_entry, 0x08, IDT_TYPE_TRAP);
	create_descriptor(46, (uint32_t)irq14_entry, 0x08, IDT_TYPE_TRAP);
	// Spurious #2 (normal in APIC)
	create_descriptor(47, (uint32_t)irq15_entry, 0x08, IDT_TYPE_TRAP);

	// Cleanup Remaining IRQs
	pic_init();

	for(int i = 0; i < 16; i++)
		isr_add_handler(i+32, irq_common);
}