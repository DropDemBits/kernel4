#include <common/ata/ata.h>
#include <common/io/pci.h>

void ata_init_controller(uint16_t bus, uint16_t device, uint16_t function);

struct pci_dev_handler ata_driver = 
{
    .vendor_id = PCI_MATCH_ANY,
    .device_id = PCI_MATCH_ANY,
    .class_code = 0x01,
    .subclass_code = 0x01,
    .prog_if = PCI_ANY_PROG_IF,
    .found = ata_init_controller,
};

void ata_init_controller(uint16_t bus, uint16_t device, uint16_t function)
{
    printf("[ATA ] Initializing IDE controller at %x:%x.%x\n", bus, device, function);
}

void ata_init()
{
    tty_prints("[ATA ] Setting up driver\n");
    pci_handle_dev(&ata_driver);
}
