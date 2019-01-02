#include <common/usb/usb.h>
#include <common/io/pci.h>
#include <common/util/klog.h>

#define PROG_IF_OHCI 0x00
#define PROG_IF_UHCI 0x10
#define PROG_IF_EHCI 0x20
#define PROG_IF_XHCI 0x30

static void uhci_disable_emu(struct pci_dev* dev)
{
    // UHCI's emulation disabling is simple, clear all the emulation bits in LEGSUP
    pci_write(dev, 0xC0, 2, 0xBF00); // Disable trap handling
}

static void ohci_disable_emu(struct pci_dev* dev)
{
    // TODO: do this
}

static pci_handle_ret_t hci_disable_emu(struct pci_dev* dev)
{
    klog_logln(DEBUG, "Disabling USB emulation for %02x:%02x.%01x", dev->bus, dev->device, dev->function);

    switch(dev->prog_if)
    {
        case PROG_IF_UHCI:
            uhci_disable_emu(dev);
            break;
        case PROG_IF_OHCI:
            ohci_disable_emu(dev);
            break;
        // We don't care about ehci & xhci controller handoff
        case PROG_IF_EHCI:
        case PROG_IF_XHCI:
        default:
            break;
    }
    return PCI_DEV_NOT_HANDLED;
}

struct pci_dev_handler usb_hci_handler = 
{
    .vendor_id = PCI_MATCH_ANY,
    .device_id = PCI_MATCH_ANY,
    .class_code = 0x0C,
    .subclass_code = 0x03,
    .prog_if = PCI_ANY_PROG_IF,
    .found = hci_disable_emu,
};

void usb_init()
{
    // Add (x)HCI PCI handlers
    pci_handle_dev(&usb_hci_handler);
}
