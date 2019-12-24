#include <common/io/pci.h>

#include <arch/io.h>

uint64_t ecam_base;

// PCI Config Space Acess (ECAM Only)
// bus_base is the starting host bridge decode address
inline uint32_t pci_get_ecam_addr(uint16_t bus_base, uint16_t bus, uint8_t device, uint8_t function)
{
    if(!ecam_base)
        return 0xFFFFFFFF;
    
    return ecam_base + ((bus - bus_base) << 20 | device << 15 | function << 12);
}

// PCI Config Space Access
// If the offset is outside of the accessable space, the read functions return 0xFF / 0xFFFF / 0xFFFFFFFF respectively
uint32_t pci_read_raw(uint16_t bus, uint8_t device, uint8_t function, uint16_t reg, uint8_t len)
{
    uint32_t address = 0x80000000 | (reg & 0xF00) << 16 | bus << 16 | (device & 0x1F) << 11 | (function & 0x7) << 8 | (reg & 0xFC);
    outl(0xCF8, address);

    switch(len)
    {
        case 1:
            return inb(0xCFC + (reg & 0x3));
        case 2:
            return inw(0xCFC + (reg & 0x2));
        case 4:
            return inl(0xCFC);
    }

    return 0xFFFFFFFF;
}

void pci_write_raw(uint16_t bus, uint8_t device, uint8_t function, uint16_t reg, uint8_t len, uint32_t data)
{
    uint32_t address = 0x80000000 | (reg & 0xF00) << 16 | bus << 16 | (device & 0x1F) << 11 | (function & 0x7) << 8 | (reg & 0xFC);
    outl(0xCF8, address);

    switch(len)
    {
        case 1:
            outb(data & 0xFF, 0xCFC + (reg & 0x3));
            return;
        case 2:
            outw(data & 0xFFFF, 0xCFC + (reg & 0x2));
            return;
        case 4:
            outl(data, 0xCFC);
            return;
    }
}
