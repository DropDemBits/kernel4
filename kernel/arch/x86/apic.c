#include <string.h>

#include <arch/apic.h>
#include <arch/idt.h>
#include <arch/iobase.h>

#include <common/hal.h>
#include <common/mm/mm.h>
#include <common/sched/sched.h>
#include <common/util/klog.h>

#define APIC_EOIR       0xB0
#define APIC_SIVR       0xF0
#define APIC_ISR_BASE   0x100
#define APIC_TMR_BASE   0x180
#define APIC_IRR_BASE   0x200
#define APIC_ESR        0x280
#define APIC_ICR1       0x300
#define APIC_ICR2       0x310

// LVT
#define APIC_LVT_CMCI   0x2F0
#define APIC_LVT_TIMER  0x320
#define APIC_LVT_TSENS  0x330
#define APIC_LVT_PMCR   0x340
#define APIC_LVT_LINT0  0x350
#define APIC_LVT_LINT1  0x360

// APIC Timer
#define APIC_TIMER_RCR  0x380
#define APIC_TIMER_CCR  0x390
#define APIC_TIMER_DCR  0x3E0

#define APIC_LVT_VEC         0x000FF
#define APIC_LVT_DELMODE_SHF 8
#define APIC_LVT_POL_SHF     13
#define APIC_LVT_TRIG_SHF    15
#define APIC_LVT_DEST_SHF    24

/** IO APIC Constants**/
#define IOAPIC_ID       0x00
#define IOAPIC_VER      0x01
#define IOAPIC_ARB      0x02
#define IOAPIC_REDTBL   0x10

// Masks
#define IOAPIC_REDIR_VEC     0x000FF
#define IOAPIC_REDIR_DELMODE 0x00700
#define IOAPIC_REDIR_DESMODE 0x00800
#define IOAPIC_REDIR_PENDING 0x01000
#define IOAPIC_REDIR_POL     0x02000
#define IOAPIC_REDIR_RECV    0x04000
#define IOAPIC_REDIR_TRIG    0x08000
#define IOAPIC_REDIR_MASK    0x10000

// Bit shifts
#define IOAPIC_REDIR_DELMODE_SHF 8
#define IOAPIC_REDIR_DESMODE_SHF 11
#define IOAPIC_REDIR_POL_SHF     13
#define IOAPIC_REDIR_TRIG_SHF    15
#define IOAPIC_REDIR_DEST_SHF    24

#define NR_IOAPIC_IRQS 24

struct ioapic_dev
{
    void* address;
    uint32_t irq_base;
    uint8_t redirect_len;
    struct irq_handler* handler_list[NR_IOAPIC_IRQS];
};

struct irq_mapping
{
    struct irq_mapping* next;
    uint32_t global_irq;
    uint8_t isa_irq;
};

static const char* apic_delmode_names[] = 
{
    "Fixed",
    "LowestPrio",
    "SMI",
    "Rsvd",
    "NMI",
    "INIT",
    "SIPI",
    "ExtInt",
};

// TODO: Replace with a real IO Space virtual mem allocator
uint64_t apic_map = MMIO_MAP_BASE + 0x08000000;
uint64_t ioapic_map = MMIO_MAP_BASE + 0x08001000;
static struct ioapic_dev main_ioapic = {};
static struct irq_mapping* mapping_head = NULL;
static struct irq_mapping* mapping_tail = NULL;

static uint32_t apic_read(uint32_t reg)
{
    uint32_t volatile *data = (uint32_t*)(apic_map);
    return data[reg >> 2];
}

static void apic_write(uint32_t reg, uint32_t value)
{
    uint32_t volatile *data = (uint32_t*)(apic_map);
    data[reg >> 2] = value;
}

static void apic_isr_handler(void* params, uint8_t int_num)
{
    taskswitch_disable();

    uint8_t irq = int_num - 32;
    struct irq_handler* node = main_ioapic.handler_list[irq];

    while(node != NULL)
    {
        if(node->function(node) == IRQ_HANDLED)
            break;
        node = node->next;
    }

    if(node == NULL || (node->flags & INT_EOI_FAST) == INT_EOI_FAST)
        apic_write(APIC_EOIR, 0);

    // This is after sending the EOI to allow other interrupts to pass through once we're done with the current one
    taskswitch_enable();
}

static uint32_t ioapic_read(void* ioapic_base, uint32_t reg)
{
    uint32_t volatile *ioapic = (uint32_t*)(ioapic_base);
    ioapic[0] = (reg & 0xFF);
    return ioapic[4];
}

static uint32_t ioapic_write(void* ioapic_base, uint32_t reg, uint32_t value)
{
    uint32_t volatile *ioapic = (uint32_t*)(ioapic_base);
    ioapic[0] = (reg & 0xFF);
    ioapic[4] = value;
}

void apic_init(uint64_t phybase)
{
    klog_early_logln(INFO, "Initializing APIC @ %p", phybase);

    mmu_map(apic_map, (uintptr_t)phybase, MMU_ACCESS_RW | MMU_CACHE_UC);

    // Enable LAPIC && Set SIV to FF
    apic_write(APIC_SIVR, apic_read(APIC_SIVR) | 0x1FF);
}

void apic_set_lint_entry(uint8_t lint_entry, uint8_t polarity, uint8_t trigger_mode, uint8_t delivery_mode)
{
    // Can't be greater than LINT1, or be a fixed or external delivery
    if(lint_entry > 1 || delivery_mode == APIC_DELMODE_FIXED || delivery_mode == APIC_DELMODE_EXTINT)
        return;

    klog_early_logln(INFO, "Setting up LINT%d %s vector (Level: %d, Low: %d)", lint_entry, apic_delmode_names[delivery_mode], trigger_mode, polarity);
    apic_write(APIC_LVT_LINT0 + (lint_entry << 4), (polarity << APIC_LVT_POL_SHF) | (trigger_mode << APIC_LVT_TRIG_SHF) | (delivery_mode << APIC_LVT_DELMODE_SHF));
}

void ioapic_set_mask(uint8_t line, bool is_masked)
{
    uint32_t redir_reg = (line * 2) + IOAPIC_REDTBL;

    if(is_masked)
        ioapic_write(main_ioapic.address, redir_reg, (ioapic_read(main_ioapic.address, redir_reg) | IOAPIC_REDIR_MASK));
    else
        ioapic_write(main_ioapic.address, redir_reg, (ioapic_read(main_ioapic.address, redir_reg) & ~IOAPIC_REDIR_MASK));
}

void ioapic_set_vector(uint8_t line, uint8_t vector)
{
    uint32_t redir_reg = (line * 2) + IOAPIC_REDTBL;
    ioapic_write(main_ioapic.address, redir_reg, (ioapic_read(main_ioapic.address, redir_reg) & ~0xFF) | vector);
}

void ioapic_set_mode(uint8_t line, uint8_t polarity, uint8_t trigger_mode, uint8_t dest, uint8_t delivery_mode)
{
    uint32_t redir_reg = (line * 2) + IOAPIC_REDTBL;
    uint32_t vector = ioapic_read(main_ioapic.address, redir_reg) & IOAPIC_REDIR_VEC;
    uint32_t new_mode = (polarity << IOAPIC_REDIR_TRIG_SHF) | (trigger_mode << IOAPIC_REDIR_POL_SHF) | (delivery_mode << IOAPIC_REDIR_DELMODE_SHF) | vector;

    // Mask & write (automatically unmasked)
    ioapic_set_mask(line, true);
    ioapic_write(main_ioapic.address, redir_reg + 1, dest << IOAPIC_REDIR_DEST_SHF);
    ioapic_write(main_ioapic.address, redir_reg, new_mode);
}

void ioapic_init(uint64_t phybase, uint32_t irq_base)
{
    klog_early_logln(INFO, "Initializing IOAPIC @ %p (+%d)", phybase, irq_base);

    memset(&main_ioapic, 0, sizeof(struct ioapic_dev));

    mmu_map(ioapic_map, phybase, MMU_ACCESS_RW | MMU_CACHE_UC);
    main_ioapic.address = (void*)ioapic_map;
    main_ioapic.irq_base = irq_base;
    main_ioapic.redirect_len = ((ioapic_read(ioapic_map, 1) >> 16) & IOAPIC_REDIR_VEC) + 1;

    // Setup Redirection Entries
    for(uint8_t i = 0; i < 24; i++)
    {
        // Stop if outside of the redirection entries
        if(i > main_ioapic.redirect_len)
            break;
        
        ioapic_set_vector(i, i + IRQ_BASE);

        if(i < 15)
            ioapic_set_mode(i, IOAPIC_POLARITY_HIGH, IOAPIC_TRIGGER_EDGE, 0, APIC_DELMODE_FIXED);    // ISA Bus Conformant
        else
            ioapic_set_mode(i, IOAPIC_POLARITY_LOW, IOAPIC_TRIGGER_EDGE, 0, APIC_DELMODE_FIXED);    // The rest
        ioapic_set_mask(i, true);

        isr_add_handler(i + IRQ_BASE, apic_isr_handler, NULL);
    }
}

void ioapic_route_line(uint32_t global_source, uint32_t bus_source, uint8_t polarity, uint8_t trigger_mode)
{
    klog_early_logln(INFO, "Adding mapping to IOAPICs (%d -> IRQ%d, Level: %d, Low: %d)", global_source, bus_source, trigger_mode, polarity);

    // Setup IO APIC Line to match requirements
    uint32_t isa_reg = (bus_source * 2) + IOAPIC_REDTBL;
    uint32_t vector = ioapic_read(main_ioapic.address, isa_reg) & IOAPIC_REDIR_VEC;
    ioapic_set_mode((uint8_t)global_source, polarity, trigger_mode, 0, APIC_DELMODE_FIXED);

    if(bus_source != global_source)
    {
        struct irq_mapping *mapping = kmalloc(sizeof(struct irq_mapping));

        mapping->global_irq = global_source;
        mapping->isa_irq = (uint8_t)bus_source;
        mapping->next = NULL;

        if(mapping_head != NULL)
        {
            mapping_tail->next = mapping;
            mapping_tail = mapping;
        }
        else
        {
            mapping_head = mapping;
            mapping_tail = mapping;
        }

        // ioapic_set_vector((uint8_t)global_source, (uint8_t)vector);

        // Mask old line & clear vector
        ioapic_set_mask((uint8_t)bus_source, true);
        ioapic_set_vector((uint8_t)bus_source, 0);
    }

    // Mask new line
    ioapic_set_mask((uint8_t)global_source, true);
}

uint32_t ioapic_map_irq(uint8_t isa_irq)
{
    // Walk through mappings
    struct irq_mapping* mapping = mapping_head;

    while(mapping != NULL)
    {
        if(mapping->isa_irq == isa_irq)
            return mapping->global_irq;

        mapping = mapping->next;
    }

    // If falling through, probably just a regular mapping
    return isa_irq;
}

// ic_dev interface
void ioapic_dummy_enable(uint8_t irq_base) {}
void ioapic_dummy_disable(uint8_t irq_base) {}

void ioapic_irq_mask(uint8_t irq)
{
    ioapic_set_mask(irq, true);
}

void ioapic_irq_unmask(uint8_t irq)
{
    ioapic_set_mask(irq, false);
}

bool ioapic_dummy_spurious(uint8_t irq)
{
    return false;
}

void apic_eoi(uint8_t irq)
{
    apic_write(APIC_EOIR, 0);
}

struct irq_handler* ioapic_handle_irq(uint8_t irq, uint32_t int_flags, irq_function_t handler)
{
    // Return null on out of range
    if(irq >= NR_IOAPIC_IRQS)
        return NULL;
    
    struct irq_handler* irq_handler = kmalloc(sizeof(struct irq_handler));
    if(irq_handler == NULL)
        return NULL;

    if(!klog_is_init())
        klog_early_logln(INFO, "Handling INT%d using %p", irq, handler);
    else
        klog_logln(1, INFO, "Handling INT%d using %p", irq, handler);
    irq_handler->next = NULL;
    irq_handler->interrupt = irq;
    irq_handler->function = handler;
    irq_handler->ic = ioapic_get_dev();
    irq_handler->flags = int_flags;

    cpu_flags_t flags = hal_disable_interrupts();

    isr_add_handler(irq + IRQ_BASE, apic_isr_handler, NULL);

    if(main_ioapic.handler_list[irq] == NULL)
    {
        // Beginning of a new list, set the head
        main_ioapic.handler_list[irq] = irq_handler;
    }
    else
    {
        // Append to end of list
        struct irq_handler* tail = main_ioapic.handler_list[irq];

        while(tail->next != NULL)
            tail = tail->next;

        tail->next = irq_handler;
    }

    ioapic_set_mask(irq, false);

    hal_enable_interrupts(flags);
    return irq_handler;
}

void ioapic_free_irq(struct irq_handler* handler)
{
    if(handler == NULL || handler->interrupt > NR_IOAPIC_IRQS)
        return;

    // As we are modifying the interrupt list, encapsulate inside of a disable-enable interrupt pair
    cpu_flags_t flags = hal_disable_interrupts();

    if(main_ioapic.handler_list[handler->interrupt] == handler)
    {
        // Remove from head of list
        main_ioapic.handler_list[handler->interrupt] = NULL;
    }
    else
    {
        // Remove from inside handler list (iterate through as we have no backlinks)
        struct irq_handler* prev = main_ioapic.handler_list[handler->interrupt];

        while(prev->next != NULL && prev->next != handler)
            prev = prev->next;
        
        // Remove node
        prev->next = handler->next;
    }

    // Release it
    kfree(handler);

    hal_enable_interrupts(flags);
}

struct ic_dev ioapic_dev = 
{
    .enable = ioapic_dummy_enable,
    .disable = ioapic_dummy_disable,
    .mask = ioapic_irq_mask,
    .unmask = ioapic_irq_unmask,
    .is_spurious = ioapic_dummy_spurious,
    .eoi = apic_eoi,
    .handle_irq = ioapic_handle_irq,
    .free_irq = ioapic_free_irq,
};

struct ic_dev* ioapic_get_dev()
{
    return &ioapic_dev;
}
