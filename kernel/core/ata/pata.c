#include <common/hal.h>
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
#define EINVAL_ARG -4

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

#define INTR_CD     0x01
#define INTR_IO     0x02
#define INTR_REL    0x04

#define IDENTIFY 0xEC
#define IDENTIFY_PACKET 0xA1
#define PACKET 0xA0

#define FIXUP_BAR(x, val) \
    if(!(x & 0xFE)) { printf("[PATA] Fixing up IDE quirk (%#lx -> %#lx)\n", x, val); (x) = (val); }

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
static bool volatile irq_fired = false;

static void pata_irq_handler()
{
    if(current_device != KNULL && current_device->dev.processing_command)
    {
        // Acknowledge interrupt
        inb(current_device->command_base + ATA_STATUS);
        // TODO: Unblock thread currently using ata subsystem
        irq_fired = true;
        ic_eoi(current_device->dev.interrupt);
    }
    else
    {
        printf("[ATA ] WARN: IRQ fired outside of command\n");
        // The PICs only need an eoi, not a specific IRQ eoi
        ic_eoi(15);
    }
}

// TODO: Merge with main ATA driver file
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

static struct ata_dev* get_device(uint16_t id)
{
    if(id >= ATA_INVALID_ID || device_list == NULL)
        return KNULL;
    return device_list[id];
}

// Returns true when the device id is valid
static bool switch_device(uint16_t id, uint8_t lba_bits)
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

    outb(command_base + ATA_DRV_SEL, 0xE0 | (current_device->is_secondary) << 4 | (lba_bits & 0x0F));

    // Delay
    inb(alt_base + ATA_ALT_STATUS);
    inb(alt_base + ATA_ALT_STATUS);
    inb(alt_base + ATA_ALT_STATUS);
    inb(alt_base + ATA_ALT_STATUS);

    return true;
}

// Handles the sending part of a command
// Returns 0 if the command was handled, 1 if the command was aborted / errored, negative otherwise
int ata_send_command(uint16_t id, uint8_t command, uint16_t features, uint64_t lba, uint16_t sector_count, bool is48bit)
{
    if(!switch_device(id, (lba >> 24) & 0x0F))
        return EINVAL;
    else if(current_device->dev.processing_command)
        return EBUSY;

    current_device->dev.processing_command = true;
    irq_fired = false;
    
    uint16_t command_base = current_device->command_base;
    uint16_t control_base = current_device->control_base;
    uint8_t last_status = 0;

    while(inb(control_base + ATA_ALT_STATUS) & (STATUS_DRDY) != STATUS_DRDY)
        busy_wait();

    if(current_device->dev.has_lba48 && is48bit)
    {
        // Write appropriate high bytes
        outb(command_base + ATA_SECTOR_COUNT, (sector_count >> 8) & 0xFF);
        outb(command_base + ATA_LBALO,  (lba >> 24) & 0xFF);
        outb(command_base + ATA_LBAMID, (lba >> 32) & 0xFF);
        outb(command_base + ATA_LBAHI,  (lba >> 40) & 0xFF);

        if(features != 0)
            outb(command_base + ATA_FEATURE, (features >> 8) & 0xFF);
    }

    outb(command_base + ATA_SECTOR_COUNT, (sector_count >> 0) & 0xFF);
    outb(command_base + ATA_LBALO,  (lba >> 0) & 0xFF);
    outb(command_base + ATA_LBAMID, (lba >> 8) & 0xFF);
    outb(command_base + ATA_LBAHI,  (lba >> 16) & 0xFF);

    if(features != 0)
        outb(command_base + ATA_FEATURE, (features >> 0) & 0xFF);

    outb(command_base + ATA_COMMAND, command);

    // Time delay
    sched_sleep_ms(1);
    
    if(inb(control_base + ATA_ALT_STATUS) == 0)
        return EABSENT;

    // Command completed successfully
    if((inb(control_base + ATA_ALT_STATUS) & (STATUS_BSY | STATUS_DRQ)) == 0)
        return 0;
    
    // Command completed with an error
    if((inb(control_base + ATA_ALT_STATUS) & STATUS_ERR) == 1)
        return 1;

    // NOTE: PACKET doesn't raise interrupts, so we can't wait for them and have to use a busy loop
    if(command == PACKET)
        goto packet_wait;

    // Wait until busy bit clears
    // TODO: Block current thread & awake on interrupt
    while(!irq_fired)
        busy_wait();

    goto normal_exit;

    packet_wait:
    last_status = inb(control_base + ATA_ALT_STATUS);
    while((last_status & STATUS_BSY) && (last_status & (STATUS_ERR | STATUS_DRDY)) == 0)
    {
        busy_wait();
        last_status = inb(control_base + ATA_ALT_STATUS);
    }

    normal_exit:
    return 0;
}

int ata_end_command(uint16_t id)
{
    if(!switch_device(id, 0))
        return EINVAL;
    
    current_device->dev.processing_command = false;
    return 0;
}

int pata_do_transfer(uint16_t id, uint16_t* transfer_buffer, uint16_t sector_count, uint16_t transfer_size, int transfer_dir, bool is_dma)
{
    return 0;
}

/**
 * @brief  Sends an ATAPI command to the device
 * @note   If the command does not require any data to be transfered, the transfer buffer, direction, and size are ignored.
 *         If the command uses DMA, the size is ignored.
 *         If is_16b is true and the device doesn't support the command or the device isn't an ATAPI device, EINVAL is returned.
 * @param  id: The id of the target ATAPI device
 * @param  command: The command buffer composed of the appropriate amount of bytes
 * @param  transfer_buffer: The buffer containing the data to transfer
 * @param  transfer_size: The size of how large one transfer block can be
 * @param  transfer_dir: The transfer direction of the command (0 = write, 1 = read)
 * @param  is_dma: Whether the command will use DMA
 * @param  is_16b: Whether the command is 16 bytes long (only if the device supports it)
 * @retval EINVAL if the device doesn't exist
 *         EINVAL_ARG if the device isn't ATAPI or doesn't support the command length
 *         EABSENT if the device doesn't exist
 *         0 if the command was sent successfully or the buffer was null
 */
int atapi_send_command(uint16_t id, uint16_t* command, uint16_t* transfer_buffer, uint16_t transfer_size, int transfer_dir, bool is_dma, bool is_16b)
{
    // Initial checks
    if(transfer_buffer == NULL || transfer_buffer == KNULL)
        return 0;
    if(get_device(id) == KNULL)
        return EABSENT;
    if(get_device(id)->device_type & TYPE_ATAPI != TYPE_ATAPI)
        return EINVAL_ARG;
    if(!get_device(id)->has_lba48 && is_16b)
        return EINVAL_ARG;

    if(!switch_device(id, 0))
        return EINVAL;
    else if(current_device->dev.processing_command)
        return EBUSY;

    // Send Packet Command
    int error_code = 0;
    uint16_t command_base = current_device->command_base;
    uint16_t control_base = current_device->control_base;

    error_code = ata_send_command(id, PACKET, /*(transfer_dir & 1) << 2 |*/ is_dma, transfer_size << 8, 0, false);
    irq_fired = false;

    if(error_code < 0)
    {
        ata_end_command(id);
        return error_code;
    }
    
    // Send the command over
    int command_len = (is_16b) ? 8 : 6;
    for(int i = 0; i < command_len; i++)
        outw(command_base, command[i]);

    // Wait for an IRQ indicating the device is ready to begin the transfer
    while(!irq_fired)
        busy_wait();

    // Check current status
    if((inb(control_base + ATA_ALT_STATUS) & (STATUS_BSY | STATUS_DRQ)) == 0)
    {
        // Command processed successfully, nothing else to be done
        ata_end_command(id);
        return 0;
    }

    // Begin transfer
    uint16_t byte_count = 0;
    uint16_t word_count = 0;
    uint32_t buffer_index = 0;

    next_transfer:
    irq_fired = false;

    byte_count = inb(command_base + ATA_LBAHI) << 8 | inb(command_base + ATA_LBAMID);
    word_count = byte_count >> 1;
    printf("[PATA] Begining Data Transfer (%d bytes)\n", byte_count);

    if(transfer_dir == 1)
    {
        while(word_count--)
            transfer_buffer[buffer_index++] = inw(command_base + ATA_DATA);
    }
    else
    {
        while(word_count--)
            outw(command_base + ATA_DATA, transfer_buffer[buffer_index++]);
    }

    while(!irq_fired)
        busy_wait();

    if(inb(control_base + ATA_ALT_STATUS) & STATUS_DRQ)
    {
        // Transfer not complete, wait for IRQ & go
        goto next_transfer;
    }

    ata_end_command(id);
    return 0;
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
            // Separate sector count into two dwords
            dev->num_sectors =  current_device->device_info[102] <<  0 |
                                current_device->device_info[103] << 16;
            dev->num_sectors <<= 32;
            dev->num_sectors |= current_device->device_info[100] <<  0 |
                                current_device->device_info[101] << 16;
        }
        else
        {
            dev->num_sectors = current_device->device_info[60] << 0ULL | current_device->device_info[61] << 16ULL;
        }
    }
}

/**
 * @brief  Initializes an atapi device.
 * @note   
 * @retval None
 */
static void init_atapi_device()
{
    // End the identify command
    ata_end_command(current_id);
    
    // Send the identify packet command
    ata_send_command(current_id, IDENTIFY_PACKET, 0x0000, 0, 0, false);

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

/**
 * @brief  Initializes an IDE channel.
 * @note   
 * @param  command_base: The base of the command block of registers.
 * @param  control_base: The base of the control block of registers.
 * @param  irq_line: The IRQ line of the IDE channel
 * @param  secondary: If the current channel is the secondary one.
 * @retval None
 */
struct pata_dev* init_ide_controller(uint32_t command_base, uint32_t control_base, uint16_t irq_line, bool secondary)
{
    // Reset device
    outb(control_base + ATA_DEV_CTRL, 0x04);
    outb(control_base + ATA_DEV_CTRL, 0x00);

    struct pata_dev* controller = kmalloc(sizeof(struct pata_dev));
    controller->dev.dev_lock = mutex_create();
    controller->dev.processing_command = false;
    controller->dev.is_active = true;
    controller->dev.interrupt = irq_line;
    controller->command_base = command_base;
    controller->control_base = control_base;
    controller->is_secondary = secondary;
    controller->device_info = KNULL;

    ata_add_device(controller);

    int err = ata_send_command(controller->dev.id, IDENTIFY, 0x0000, 0, 0, false);
    if(err == EABSENT)
    {
        controller->dev.is_active = false;
        controller->dev.processing_command = false;
        current_id = ATA_INVALID_ID;
        current_device = KNULL;
        return controller;
    }

    uint8_t id_low = inb(command_base + ATA_LBAMID);
    uint8_t id_hi = inb(command_base + ATA_LBAHI);
    uint8_t id_sig = inb(command_base + ATA_LBALO);
    uint8_t last_status;

         if(id_low == 0x14 && id_hi == 0xEB) init_atapi_device();
    else if(id_low == 0x3C && id_hi == 0xC3) printf("[SATA] Setting up SATA device\n");
    else if(id_low == 0x69 && id_hi == 0x96) printf("[SATA] Setting up SATA? device\n");

    printf("[PATA] PATA Device Present (%d, %d)\n", controller->dev.id, controller->dev.device_type);

    // The dedicated init methods take care of the job
    if(id_low != 0x00 && id_hi != 0x00)
        return controller;

    // Continue setting up ATA device

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
    return controller;
}

/**
 * @brief  Initializes an ide controller found on the PCI bus.
 * @note   
 * @param  bus: The bus of the controller.
 * @param  device: The device of the controller.
 * @param  function: The function of the controller.
 * @retval None
 */
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
    printf("[PATA] OpMode: %x\n", operating_mode);

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
        // Setup interrupt handler if channel is in compatibility mode
        if((operating_mode & 0b0001) == 0)
            irq_add_handler(14, pata_irq_handler);
        
        init_ide_controller(command0_base, control0_base, 14, false);
        init_ide_controller(command0_base, control0_base, 14, true);
    }

    if(init_secondary)
    {
        printf("[PATA] Setting up secondary channel\n");
        // Setup interrupt handler if channel is in compatibility mode
        if((operating_mode & 0b0100) == 0)
            irq_add_handler(15, pata_irq_handler);

        init_ide_controller(command1_base, control1_base, 15, false);
        init_ide_controller(command1_base, control1_base, 15, true);
    }

    current_id = ATA_INVALID_ID;
    current_device = KNULL;
}

/**
 * @brief  Initializes the PATA side of the ATA driver
 * @note   
 * @retval None
 */
void pata_init()
{
    pci_handle_dev(&pata_driver);
}
