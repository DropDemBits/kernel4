#include <common/ata/ata.h>
#include <common/mm/liballoc.h>
#include <common/io/pci.h>
#include <common/util/kfuncs.h>

#include <arch/io.h>

void ata_init_controller(uint16_t bus, uint16_t device, uint16_t function);

#define ATA_INVALID_ID 0xFFFF

#define EINVAL -1
#define EBUSY -2
#define EABSENT -3

#define ATA_DATA 0
#define ATA_ERROR 1
#define ATA_FEATURE ATA_ERROR
#define ATA_SECTOR_COUNT 2
#define ATA_LBALO 3
#define ATA_LBAMID 4
#define ATA_LBAHI 5
#define ATA_DRV_SEL 6
#define ATA_COMMAND 7
#define ATA_STATUS 7

#define ATA_ALT_STATUS 0
#define ATA_DEV_CTRL 0
#define ATA_DRV_ADDR 1

#define STATUS_BSY  0x80 // Busy
#define STATUS_DRDY 0x40 // Device Ready
#define STATUS_DF   0x20 // Device Fault
#define STATUS_SE   0x20 // Stream Error
#define STATUS_DWE  0x10 // Deferred Write Error
#define STATUS_DRQ  0x08 // Drive Data Request
#define STATUS_ERR  0x01 // Error
#define STATUS_CHK  0x01 // Check, ATA8+

#define FIXUP_BAR(x, val) \
    if(!(x)) { printf("[PATA] Fixing up IDE quirk (%#lx -> %#lx)\n", x, val); (x) = (val); }

struct pci_dev_handler pata_driver = 
{
    .vendor_id = PCI_MATCH_ANY,
    .device_id = PCI_MATCH_ANY,
    .class_code = 0x01,
    .subclass_code = 0x01,
    .prog_if = PCI_ANY_PROG_IF,
    .found = ata_init_controller,
};

/*
 * Device ID Layout:
 * Bit 0: Master/Slave Select
 * Bit 1: Channel Select
 * Bit 2-15: Controller Select
 */
static uint16_t next_id = 0;
static uint16_t current_id = ~0;
static struct pata_dev* current_device;
static struct ata_dev** device_list = NULL;

static void ata_add_device(struct ata_dev* device)
{
    if(device_list == NULL)
        device_list = kmalloc(sizeof(char*) * 8);
    else if(next_id >= ATA_INVALID_ID)
        kpanic("Too many PATA Devices");
    else if(next_id % 8 == 0)
        device_list = krealloc(device_list, sizeof(char*) * next_id + sizeof(char*) * 8);

    device->id = next_id;
    device_list[next_id++] = device;
}

// Returns true when the device id is valid
static bool switch_device(uint16_t id)
{
    if(id >= ATA_INVALID_ID)
        return false;
    else if(device_list[id] == KNULL)
        return false;
    else if(!device_list[id]->is_active)
        return false;
    else if(current_id == id)
        return true;

    current_id = id;
    current_device = device_list[id];
    uint16_t command_base = current_device->command_base;
    uint16_t alt_base = current_device->control_base;

    outb(command_base + ATA_DRV_SEL, 0xA0 | (current_device->is_secondary) << 4);

    // Delay
    inb(alt_base + ATA_ALT_STATUS);
    inb(alt_base + ATA_ALT_STATUS);
    inb(alt_base + ATA_ALT_STATUS);
    inb(alt_base + ATA_ALT_STATUS);
    inb(alt_base + ATA_ALT_STATUS);

    return true;
}

// Handles the sending part of a command
// Returns 0 if the command was handled, non-zero otherwise
int ata_send_command(uint16_t id, uint8_t command, uint16_t features, uint64_t lba, uint8_t sector_count, bool is48bit)
{
    if(!switch_device(id))
        return EINVAL;
    else if(current_device->dev.processing_command)
        return EBUSY;

    // TODO: Check DRDY
    current_device->dev.processing_command = true;
    
    uint16_t command_base = current_device->command_base;
    uint16_t alt_base = current_device->control_base;

    outb(command_base + ATA_SECTOR_COUNT, sector_count);
    outb(command_base + ATA_LBALO, (lba >> 0) & 0xFF);
    outb(command_base + ATA_LBAMID, (lba >> 8) & 0xFF);
    outb(command_base + ATA_LBAHI, (lba >> 16) & 0xFF);

    outb(command_base + ATA_COMMAND, command);
    if(inb(alt_base + ATA_ALT_STATUS) == 0)
        return EABSENT;

    return 0;
}

void ata_end_command(uint16_t id)
{
    if(!switch_device(id))
        return EINVAL;
    
    current_device->dev.processing_command = false;
}

static void setup_features()
{
    struct ata_dev* dev = (struct ata_dev*)current_device;

    if(dev->device_type & TYPE_ATAPI)
    {
        // ATAPI Specific stuff

        // Set LBA48 if device supports 16 byte packets
        dev->has_lba48 = current_device->device_info[0] & 0x3;
        dev->sector_size = 2048;
        dev->num_sectors = 0;
    }
    else
    {
        // Non-packet Specific stuff
        dev->has_lba48 = (current_device->device_info[83] >> 1) & 1;

        uint32_t sector_size = 512;
        if((current_device->device_info[106] & 0xC000) == 0x4000)
        {
            // Read sector size
            if((current_device->device_info[106] & 0xF000) == 0x6000)
                sector_size = current_device->device_info[117] | current_device->device_info[118] << 16;
        }
        dev->has_lba48 = (current_device->device_info[83] >> 1) & 1;
        dev->sector_size = sector_size;

        if(dev->has_lba48)
        {
            dev->num_sectors = current_device->device_info[100] <<  0 |
                               current_device->device_info[101] << 16 |
                               current_device->device_info[102] << 32 |
                               current_device->device_info[103] << 48;
        }
        else
        {
            dev->num_sectors = current_device->device_info[60] << 0 | current_device->device_info[61] << 16;
        }
    }
}

static void init_atapi_device()
{
    // End the identify command
    ata_end_command(current_id);
    
    // Send the identify packet command
    ata_send_command(current_id, 0xA1, 0x0000, 0, 0, false);

    uint16_t* buffer = kmalloc(256*2); // 256 words

    for(int i = 0; i < 256; i++)
    {
        buffer[i] = inw(current_device->command_base + ATA_DATA);
    }

    // End the identify packet command
    ata_end_command(current_id);

    current_device->device_info = buffer;
    current_device->dev.device_type = TYPE_ATAPI;
    setup_features();
}

void init_ide_controller(uint32_t command_base, uint32_t control_base, bool secondary)
{
    // Setup Device Control (nIEN)
    outb(control_base + ATA_DEV_CTRL, 0x02);

    struct pata_dev* controller = kmalloc(sizeof(struct pata_dev));
    controller->dev.dev_lock = mutex_create();
    controller->dev.processing_command = false;
    controller->dev.is_active = true;
    controller->command_base = command_base;
    controller->control_base = control_base;
    controller->is_secondary = secondary;
    controller->device_info = KNULL;

    ata_add_device(controller);

    int err = ata_send_command(controller->dev.id, 0xEC, 0x0000, 0, 0, false);
    if(err == EABSENT)
    {
        // This is untested, but should work
        controller->dev.is_active = false;
        controller->dev.processing_command = false;
        current_id = ATA_INVALID_ID;
        current_device = KNULL;
        return;
    }

    uint8_t id_low = inb(command_base + ATA_LBAMID);
    uint8_t id_hi = inb(command_base + ATA_LBAHI);
    uint8_t last_status;

    // Wait a bit after sending the command (also check device sig bytes)
    // Break if it is not busy, or there is data / an error
    while((last_status = inb(command_base + ATA_STATUS) & STATUS_BSY) && !(last_status & (STATUS_DRQ | STATUS_ERR)))
    {
        id_low = inb(command_base + ATA_LBAMID);
        id_hi = inb(command_base + ATA_LBAHI);

        if(id_hi != 0 && id_low != 0)
            break;
        
        sched_sleep_ns(500);
    }

         if(id_low == 0x14 && id_hi == 0xEB) init_atapi_device();
    else if(id_low == 0x3C && id_hi == 0xC3) printf("[SATA] Setting up SATA device\n");
    else if(id_low == 0x69 && id_hi == 0x96) printf("[SATA] Setting up SATA? device\n");

    // The dedicated init methods take care of the job
    if(id_low != 0x00 && id_hi != 0x00)
        return;

    // Setup the identify buffer
    uint16_t* buffer = kmalloc(256*2); // 256 words

    for(int i = 0; i < 256; i++)
    {
        buffer[i] = inw(command_base + ATA_DATA);
    }

    // End the identify command
    ata_end_command(current_id);

    current_device->device_info = buffer;
    current_device->dev.device_type = TYPE_ATA;

    setup_features();
}

void ata_init_controller(uint16_t bus, uint16_t device, uint16_t function)
{
    bool init_primary = true;
    bool init_secondary = true;

    printf("[PATA] Initializing IDE controller at %x:%x.%x\n", bus, device, function);
    uint8_t operating_mode = pci_read(bus, device, function, PCI_PROG_IF, 1);

    uint32_t command0_base = pci_read(bus, device, function, PCI_BAR0, 4) & ~0x3;
    uint32_t command1_base = pci_read(bus, device, function, PCI_BAR2, 4) & ~0x3;
    uint32_t control0_base = pci_read(bus, device, function, PCI_BAR1, 4) & ~0x3;
    uint32_t control1_base = pci_read(bus, device, function, PCI_BAR3, 4) & ~0x3;

    if((operating_mode & 0b0001) != (operating_mode & 0b0100) && (operating_mode & 0b1010) == 0)
    {
        printf("[PATA] Error: Operating mode mismatch (%d != %d)\n", (operating_mode & 0b0001), (operating_mode & 0b0100));
        return;
    }

    FIXUP_BAR(command0_base, 0x1F0);
    FIXUP_BAR(control0_base, 0x3F6);
    FIXUP_BAR(command1_base, 0x170);
    FIXUP_BAR(control1_base, 0x376);

    printf("[PATA] Primary channel at %#lx-%#lx, %#lx\n", command0_base, command0_base+7, control0_base);
    printf("[PATA] Secondary channel at %#lx-%#lx, %#lx\n", command1_base, command1_base+7, control1_base);

    if(inb(control0_base + ATA_ALT_STATUS) == 0xFF)
        init_primary = false;
    if(inb(control0_base + ATA_ALT_STATUS) == 0xFF)
        init_secondary = false;
    
    if(!init_primary && !init_secondary)
    {
        printf("[PATA] Error: Non-existant IDE Controller\n");
        return;
    }

    if(init_primary)
    {
        printf("[PATA] Setting up primary channel\n");
        init_ide_controller(command0_base, control0_base, false);
        init_ide_controller(command0_base, control0_base, true);
    }

    if(init_secondary)
    {
        printf("[PATA] Setting up secondary channel\n");
        init_ide_controller(command1_base, control1_base, false);
        init_ide_controller(command1_base, control1_base, true);
    }
    
    tty_reshow();
}

void pata_init()
{
    pci_handle_dev(&pata_driver);
}
