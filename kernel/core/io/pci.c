#include <stdio.h>
#include <string.h>

#include <common/io/pci.h>
#include <common/util/kfuncs.h>

struct pci_dev_handler* driver_head = KNULL;
static uint16_t pci_subsys = 0;

void test_function(uint8_t bus, uint8_t device, uint8_t function)
{
    uint16_t vendor_id =    pci_read_raw(bus, device, function, PCI_VENDOR, 2);
    uint16_t device_id =    pci_read_raw(bus, device, function, PCI_DEVICE, 2);
    // uint8_t header_type =   pci_read_raw(bus, device, function, PCI_HEADER, 1);

    char* device_name = "[unknown]";

    // TODO: Just replace below with an array of pointers to pci device name structs
         if(vendor_id == 0x1022 && device_id == 0x2000) device_name = "AMD PCnet32 FAST III (Am79C973)";
    else if(vendor_id == 0x106b && device_id == 0x003f) device_name = "Apple USB Controller";
    else if(vendor_id == 0x10ec && device_id == 0x8139) device_name = "Realtek RTL-8139 PCI Fast Ethernet Adapter";
    else if(vendor_id == 0x1234 && device_id == 0x1111) device_name = "Bochs VBE-Compatible VGA Controller";
    else if(vendor_id == 0x1af4 && device_id == 0x1002) device_name = "Virtio memory balloon";
    else if(vendor_id == 0x1af4 && device_id == 0x1003) device_name = "Virtio console";
    else if(vendor_id == 0x1b36 && device_id == 0x0100) device_name = "QXL Paravirtual Graphic Card";
    else if(vendor_id == 0x8086 && device_id == 0x100e) device_name = "Intel 82540EM Gigabit Ethernet Controller";
    else if(vendor_id == 0x8086 && device_id == 0x1237) device_name = "Intel i440FX \"Natoma\" Northbridge PMC";
    else if(vendor_id == 0x8086 && device_id == 0x2415) device_name = "Intel AC'97 Audio Controller";
    else if(vendor_id == 0x8086 && device_id == 0x269e) device_name = "Intel Enterprise Southbridge IDE Controller";
    else if(vendor_id == 0x8086 && device_id == 0x27b9) device_name = "Intel ICH7-M LPC Controller";
    else if(vendor_id == 0x8086 && device_id == 0x2918) device_name = "Intel ICH9 LPC Controller";
    else if(vendor_id == 0x8086 && device_id == 0x2920) device_name = "Intel ICH9R/DO/DH SATA Controller [IDE Mode]";
    else if(vendor_id == 0x8086 && device_id == 0x2922) device_name = "Intel ICH9R/DO/DH SATA Controller [AHCI Mode]";
    else if(vendor_id == 0x8086 && device_id == 0x2930) device_name = "Intel ICH9 SMBus Controller";
    else if(vendor_id == 0x8086 && device_id == 0x2934) device_name = "Intel ICH9 USB UHCI Controller #1";
    else if(vendor_id == 0x8086 && device_id == 0x2935) device_name = "Intel ICH9 USB UHCI Controller #2";
    else if(vendor_id == 0x8086 && device_id == 0x2936) device_name = "Intel ICH9 USB UHCI Controller #3";
    else if(vendor_id == 0x8086 && device_id == 0x2937) device_name = "Intel ICH9 USB UHCI Controller #4";
    else if(vendor_id == 0x8086 && device_id == 0x2938) device_name = "Intel ICH9 USB UHCI Controller #5";
    else if(vendor_id == 0x8086 && device_id == 0x2939) device_name = "Intel ICH9 USB UHCI Controller #6";
    else if(vendor_id == 0x8086 && device_id == 0x293a) device_name = "Intel ICH9 USB2 EHCI Controller #1";
    else if(vendor_id == 0x8086 && device_id == 0x293b) device_name = "Intel ICH9 USB2 EHCI Controller #2";
    else if(vendor_id == 0x8086 && device_id == 0x29c0) device_name = "Intel 82G33/G31/P35/P31 DRAM Controller";
    else if(vendor_id == 0x8086 && device_id == 0x7000) device_name = "Intel PIIX3 Southbridge - ISA Controller";
    else if(vendor_id == 0x8086 && device_id == 0x7010) device_name = "Intel PIIX3 Southbridge - IDE Controller";
    else if(vendor_id == 0x8086 && device_id == 0x7020) device_name = "Intel PIIX3 Southbridge - USB Controller";
    else if(vendor_id == 0x8086 && device_id == 0x7111) device_name = "Intel PIIX4 Southbridge - IDE Controller";
    else if(vendor_id == 0x8086 && device_id == 0x7113) device_name = "Intel PIIX4 Southbridge - ACPI Controller";
    else if(vendor_id == 0x80ee && device_id == 0xbeef) device_name = "VirtualBox Graphics Adapter";
    else if(vendor_id == 0x80ee && device_id == 0xcafe) device_name = "VirtualBox Guest Service";

    klog_logln(pci_subsys, DEBUG, "%x:%x.%x: %s (%x:%x)", bus, device, function, device_name, vendor_id, device_id);

    struct pci_dev_handler *node = driver_head;
    struct pci_dev* dev = pci_get_dev(bus, device, function);
    
    while(node != KNULL)
    {
        if(node->vendor_id == vendor_id && node->device_id == device_id)
        {
            if(node->found(dev) == PCI_DEV_HANDLED)
                break;
        }
        else if(node->vendor_id == PCI_MATCH_ANY && node->device_id == PCI_MATCH_ANY)
        {
            if(node->class_code == dev->class_code && node->subclass_code == dev->subclass_code && (node->prog_if == dev->prog_if || node->prog_if == PCI_ANY_PROG_IF))
            {
                if(node->found(dev) == PCI_DEV_HANDLED)
                    break;
            }
        }
        node = node->next;
    }

    if(node == KNULL)
        pci_free_irq(dev);
}

void pci_test_device(uint8_t bus, uint8_t device)
{
    uint16_t vendor_id = pci_read_raw(bus, device, 0, PCI_VENDOR, 2);
    if(vendor_id == 0xFFFF)
        return; // Non-existant device

    test_function(bus, device, 0);

    uint8_t header_type = pci_read_raw(bus, device, 0, PCI_HEADER, 1);
    if(header_type & 0x80)
    {
        // Enumerate functions
        for(int function = 1; function < 8; function++)
        {
            if(pci_read_raw(bus, device, function, PCI_VENDOR, 2) != 0xFFFF)
                test_function(bus, device, function);
        }
    }
}

void pci_check_bus(uint8_t bus)
{
    for(int i = 0; i < 32; i++)
        pci_test_device(bus, i);
}

void pci_init()
{
    pci_subsys = klog_add_subsystem("PCI");
    // Enumerate the pci bus
    klog_logln(pci_subsys, INFO, "Enumerating PCI Bus");
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

struct pci_dev* pci_get_dev(uint8_t bus, uint8_t device, uint8_t function)
{
    struct pci_dev* dev = kmalloc(sizeof(struct pci_dev));

    if(dev == NULL)
        return KNULL;

    uint8_t intr_line =     pci_read_raw(bus, device, function, PCI_INTR_LINE, 1);
    uint8_t irq_pin =       pci_read_raw(bus, device, function, PCI_IRQ_PIN, 1);

    uint8_t class_code =    pci_read_raw(bus, device, function, PCI_CLASS_CODE, 1);
    uint8_t subclass_code = pci_read_raw(bus, device, function, PCI_SUBCLASS_CODE, 1);
    uint8_t prog_if =       pci_read_raw(bus, device, function, PCI_PROG_IF, 1);

    dev->bus = bus;
    dev->device = device;
    dev->function = function;

    dev->class_code = class_code;
    dev->subclass_code = subclass_code;
    dev->prog_if = prog_if;

    dev->num_irqs = 0;
    dev->intr_line = intr_line;
    dev->irq_pin = irq_pin;
    dev->irq_handler = NULL;
    dev->msi_handlers = NULL;
    dev->msix_handlers = NULL;

    return dev;
}

void pci_put_dev(struct pci_dev* dev)
{
    pci_free_irq(dev);
    kfree(dev);
}

int pci_alloc_irq(struct pci_dev* dev, uint8_t num_irqs, uint32_t flags)
{
    if(dev == KNULL)
        return -1;

    int irqs_alloc = 0;

    if(flags & IRQ_LEGACY)
    {
        dev->irq_handler = NULL;
        irqs_alloc++;
    }

    dev->num_irqs = irqs_alloc;
    return irqs_alloc;
}

void pci_handle_irq(struct pci_dev* dev, uint8_t irq_handle, irq_function_t handler)
{
    if(dev == KNULL)
        return;

    if(irq_handle == IRQ_HANDLE_LEGACY && dev->irq_handler == NULL)
        dev->irq_handler = ic_irq_handle(dev->irq_pin, INT_EOI_FAST | INT_SRC_LEGACY, handler);
}

void pci_unhandle_irq(struct pci_dev* dev, uint8_t irq_handle)
{
    if(irq_handle == IRQ_HANDLE_LEGACY && dev->irq_handler != NULL)
        ic_irq_free(dev->irq_handler);
}

void pci_free_irq(struct pci_dev* dev)
{
    pci_unhandle_irq(dev, IRQ_HANDLE_LEGACY);
}
