#include <common/types.h>
#include <common/hal.h>

#ifndef __PCI_H__
#define __PCI_H__ 1

#define PCI_DEV_HANDLED 0
#define PCI_DEV_NOT_HANDLED 1

// Used to match to the class code & prog_if code instead of vendor id & device id. Vendor & Device id must be set to this value
#define PCI_MATCH_ANY 0xFFFF
#define PCI_ANY_PROG_IF 0xFF

#define PCI_VENDOR          0x000
#define PCI_DEVICE          0x002
#define PCI_PROG_IF         0x009
#define PCI_SUBCLASS_CODE   0x00A
#define PCI_CLASS_CODE      0x00B
#define PCI_HEADER          0x00E
#define PCI_BAR0            0x010
#define PCI_BAR1            0x014
#define PCI_BAR2            0x018
#define PCI_BAR3            0x01C
#define PCI_BAR4            0x020
#define PCI_BAR5            0x024

#define PCI_CAP_PTR         0x034
#define PCI_INTR_LINE       0x03C
#define PCI_IRQ_PIN         0x03D

// IRQ Flags
#define IRQ_LEGACY          0x00000001  // Mutually exclusive with IRQ_INTX
#define IRQ_INTX            0x00000002  // Mutually exclusive with IRQ_LEGACY
#define IRQ_MSI             0x00000004
#define IRQ_MSIX            0x00000008
#define IRQ_ALL             (IRQ_LEGACY | IRQ_INTX | IRQ_MSI | IRQ_MSIX)

// PCI Config Space Access (Including ECAM)
// If the offset is outside of the accessable space, the read functions return 0xFF / 0xFFFF / 0xFFFFFFFF respectively
extern uint32_t pci_read_raw(uint16_t bus, uint8_t device, uint8_t function, uint16_t reg, uint8_t len);
extern void pci_write_raw(uint16_t bus, uint8_t device, uint8_t function, uint16_t reg, uint8_t len, uint32_t data);
extern uint32_t pci_get_ecam_addr(uint16_t bus_base, uint16_t bus, uint8_t device, uint8_t function);

struct pci_dev;

typedef int pci_handle_ret_t;
typedef pci_handle_ret_t(*pci_device_found_t)(struct pci_dev* dev);

struct pci_dev_handler
{
    uint16_t vendor_id;
    uint16_t device_id;

    uint8_t class_code;
    uint8_t subclass_code;
    uint8_t prog_if;

    pci_device_found_t found;

    struct pci_dev_handler* next;
};

struct pci_dev
{
    uint16_t bus;
    uint8_t device : 5;
    uint8_t function : 3;
    
    uint8_t irq_pin;         // Interrupt Pin (corresponds to IRQ0-15)
    uint8_t intr_line;       // Interupt Line (corresponds to INTA-D)

    uint8_t num_irqs;
    struct irq_handler* irq_handler;
    struct irq_handler** msi_handlers;
};

void pci_init();
struct pci_dev* pci_get_dev(uint8_t bus, uint8_t device, uint8_t function);
void pci_put_dev(struct pci_dev* dev);
void pci_handle_dev(struct pci_dev_handler *handle);

// pci_*_raw wrappers
inline uint32_t pci_read(struct pci_dev* dev, uint16_t reg, uint8_t len)
{
    if(dev == KNULL || dev == NULL)
        return 0xFFFFFFFF;
    return pci_read_raw(dev->bus, dev->device, dev->function, reg, len);
}

inline void pci_write(struct pci_dev* dev, uint16_t reg, uint8_t len, uint32_t data)
{
    if(dev == KNULL || dev == NULL)
        return;

    pci_write_raw(dev->bus, dev->device, dev->function, reg, len, data);
}

int pci_alloc_irq(struct pci_dev* dev, uint8_t num_irqs, uint32_t flags);
void pci_handle_irq(struct pci_dev* dev, uint8_t irq_handle, irq_function_t handler);
void pci_unhandle_irq(struct pci_dev* dev, uint8_t irq_handle);
void pci_free_irq(struct pci_dev* dev);

#endif /* __PCI_H__ */
