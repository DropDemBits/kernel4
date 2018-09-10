#include <common/types.h>

#ifndef __PCI_H__
#define __PCI_H__ 1

// Used to match to the class code & prog_if code instead of vendor id & device id. Vendor & Device id must be set to this value
#define PCI_MATCH_ANY 0xFFFF
#define PCI_ANY_PROG_IF 0xFF

#define PCI_PROG_IF        0x009
#define PCI_SUBCLASS_CODE  0x00A
#define PCI_CLASS_CODE     0x00B
#define PCI_BAR0           0x010
#define PCI_BAR1           0x014
#define PCI_BAR2           0x018
#define PCI_BAR3           0x01C
#define PCI_BAR4           0x020
#define PCI_BAR5           0x024

// PCI Config Space Access (Including ECAM)
// If the offset is outside of the accessable space, the read functions return 0xFF / 0xFFFF / 0xFFFFFFFF respectively
extern uint32_t pci_read(uint8_t bus, uint8_t device, uint8_t function, uint16_t reg, uint8_t len);
extern void pci_write(uint8_t bus, uint8_t device, uint8_t function, uint16_t reg, uint8_t len, uint32_t data);
extern uint32_t pci_get_ecam_addr(uint8_t bus_base, uint8_t bus, uint8_t device, uint8_t function);

typedef void(*pci_device_found_t)(uint16_t bus, uint16_t device, uint16_t function);

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

void pci_init();
void pci_handle_dev(struct pci_dev_handler *handle);

#endif /* __PCI_H__ */
