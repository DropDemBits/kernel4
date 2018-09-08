#include <stdio.h>

#include <common/io/pci.h>

struct pci_dev_handler* driver_head = KNULL;

void test_function(uint8_t bus, uint8_t device, uint8_t function)
{
    uint16_t vendor_id =    pci_read(bus, device, function, 0x000, 2);
    uint16_t device_id =    pci_read(bus, device, function, 0x002, 2);
    uint8_t header_type =   pci_read(bus, device, function, 0x00E, 1);

    uint8_t class_code =    pci_read(bus, device, function, 0x00B, 1);
    uint8_t subclass_code = pci_read(bus, device, function, 0x00A, 1);
    uint8_t prog_if =       pci_read(bus, device, function, 0x009, 1);

    printf("[PCI ] %x:%x.%x ", bus, device, function);

         if(vendor_id == 0x1234 && device_id == 0x1111) printf("Bochs VBE-Compatible VGA Controller");
    else if(vendor_id == 0x8086 && device_id == 0x100e) printf("Intel 82540EM Gigabit Ethernet Controller");
    else if(vendor_id == 0x8086 && device_id == 0x1237) printf("Intel i440FX \"Natoma\" Northbridge PMC");
    else if(vendor_id == 0x8086 && device_id == 0x7000) printf("Intel PIIX3 Southbridge - ISA Controller");
    else if(vendor_id == 0x8086 && device_id == 0x7010) printf("Intel PIIX3 Southbridge - IDE Controller");
    else if(vendor_id == 0x8086 && device_id == 0x7020) printf("Intel PIIX3 Southbridge - USB Controller");
    else if(vendor_id == 0x8086 && device_id == 0x7113) printf("Intel PIIX4 Southbridge - ACPI Controller");

    else if(vendor_id == 0x8086 && device_id == 0x29c0) printf("Intel 82G33/G31/P35/P31 DRAM Controller");
    else if(vendor_id == 0x8086 && device_id == 0x2918) printf("Intel ICH9 LPC Controller");
    else if(vendor_id == 0x8086 && device_id == 0x2920) printf("Intel ICH9R/DO/DH SATA Controller [IDE Mode]");
    else if(vendor_id == 0x8086 && device_id == 0x2922) printf("Intel ICH9R/DO/DH SATA Controller [AHCI Mode]");
    else if(vendor_id == 0x8086 && device_id == 0x2930) printf("Intel ICH9 SMBus Controller");
    else if(vendor_id == 0x8086 && device_id == 0x2934) printf("Intel ICH9 USB UHCI Controller #1");
    else if(vendor_id == 0x8086 && device_id == 0x2935) printf("Intel ICH9 USB UHCI Controller #2");
    else if(vendor_id == 0x8086 && device_id == 0x2936) printf("Intel ICH9 USB UHCI Controller #3");
    else if(vendor_id == 0x8086 && device_id == 0x293a) printf("Intel ICH9 USB2 EHCI Controller #1");

    printf(" (%x:%x)\n", vendor_id, device_id);

    struct pci_dev_handler *node = driver_head;
    
    while(node != KNULL)
    {
        if(node->vendor_id == vendor_id && node->device_id == device_id)
        {
            node->found(bus, device, function);
            break;
        }
        else if(node->vendor_id == PCI_MATCH_ANY && node->device_id == PCI_MATCH_ANY)
        {
            if(node->class_code == class_code && node->subclass_code == subclass_code && (node->prog_if == prog_if || node->prog_if == PCI_ANY_PROG_IF))
            {
                node->found(bus, device, function);
                break;
            }
        }
        node = node->next;
    }
}

void pci_test_device(uint8_t bus, uint8_t device)
{
    uint16_t vendor_id = pci_read(bus, device, 0, 0x000, 2);
    if(vendor_id == 0xFFFF)
        return; // Non-existant device

    test_function(bus, device, 0);

    uint8_t header_type = pci_read(bus, device, 0, 0x00E, 1);
    if(header_type & 0x80)
    {
        // Enumerate functions
        for(int function = 1; function < 8; function++)
        {
            if(pci_read(bus, device, function, 0x000, 2) != 0xFFFF)
                test_function(bus, device, function);
        }
    }
}

void pci_check_bus(uint8_t bus)
{
    for(int i = 0; i < 32; i++)
    {
        pci_test_device(bus, i);
        tty_reshow();
    }
}

void pci_init()
{
    // Enumerate the pci bus
    tty_prints("[PCI ] Enumerating PCI Bus\n");
    pci_check_bus(0);
}

void pci_handle_dev(struct pci_dev_handler *handle)
{
    if(driver_head == KNULL)
    {
        driver_head = handle;
    }
    else
    {
        struct pci_dev_handler *node = driver_head;
        
        while(node->next != KNULL)
        {
            node = node->next;
        }

        node->next = handle;
    }

    handle->next = KNULL;
}
