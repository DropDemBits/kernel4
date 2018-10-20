#include <common/hal.h>
#include <common/ata/ata.h>
#include <common/mm/liballoc.h>
#include <common/io/pci.h>
#include <common/util/kfuncs.h>

#include <arch/io.h>

pci_handle_ret_t ata_init_controller(struct pci_dev* dev);

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

#define IDENTIFY            0xEC
#define IDENTIFY_PACKET     0xA1
#define PACKET              0xA0

#define READ_SECTORS        0x20
#define READ_SECTORS_EXT    0x24
#define READ_DMA_EXT        0x25
#define READ_MULTIPLE_EXT   0x29

#define WRITE_SECTORS       0x30
#define WRITE_SECTORS_EXT   0x34
#define WRITE_DMA_EXT       0x35
#define WRITE_MULTIPLE_EXT  0x39

#define READ_MULTIPLE       0xC4
#define WRITE_MULTIPLE      0xC5

#define READ_DMA            0xC8
#define WRITE_DMA           0xCA

#define FLUSH_CACHE         0xE7
#define FLUSH_CACHE_EXT     0xEA

#define FIXUP_BAR(x, val) \
    if(!(x & 0xFE)) { klog_logln(ata_subsys, DEBUG, "PATA: Fixing up IDE quirk (%#lx -> %#lx)", x, val); (x) = (val); }

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
static uint16_t ata_subsys = 0;

static void pata_irq_handler()
{
    if(current_device != KNULL && current_device->dev.processing_command)
    {
        // Acknowledge interrupt
        inb(current_device->command_base + ATA_STATUS);
        // TODO: Unblock thread currently using ata subsystem
        irq_fired = true;
    }
    else
    {
        klog_logln(ata_subsys, WARN, "IRQ fired outside of command");
        // The PICs only need an eoi, not a specific IRQ eoi
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
    current_device = (struct pata_dev*)device_list[id];
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

// Returns true if it timed out
static bool ata_wait()
{
    // TODO: Block current thread & awake on interrupt
    uint64_t timeout = timer_read_counter(0) + (5000000); // Delay of 5ms

    while(!irq_fired && (inb(current_device->control_base + ATA_ALT_STATUS) & STATUS_DRQ) == 0)
    {
        busy_wait();
        sched_sleep_ms(1);
        if(timeout <= timer_read_counter(0))
        {
            klog_logln(ata_subsys, DEBUG, "ata_dev%d timeout %x", current_id, inb(current_device->control_base + ATA_ALT_STATUS));
            return true;
        }
    }

    return false;
}

// Handles the sending part of a command
// Returns 0 if the command was handled, 1 if the command was aborted / errored, negative otherwise
int ata_send_command(uint16_t id, uint8_t command, uint16_t features, uint64_t lba, uint16_t sector_count, int transfer_dir, bool is48bit)
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

    int loop_counter = 10;
    // Wait until the drive is ready & not busy
    while((inb(control_base + ATA_ALT_STATUS) & (STATUS_DRDY | STATUS_BSY)) == STATUS_BSY && loop_counter > 0)
    {
        busy_wait();
        loop_counter--;
    }

    if(loop_counter <= 0)
        return EABSENT;

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

    klog_logln(ata_subsys, DEBUG, "ata_dev%d status: %x (%d)", id, last_status, loop_counter);

    last_status = inb(control_base + ATA_ALT_STATUS);

    // Command completed successfully
    if((last_status & (STATUS_BSY | STATUS_DRQ)) == 0)
        return 0;
    
    // Command completed with an error
    if((last_status & STATUS_ERR) == STATUS_ERR)
        return 1;

    // NOTE: Writes don't raise interrupts upon data ready, so we can't wait for them and have to use a busy loop
    if(transfer_dir == TRANSFER_WRITE)
        goto write_wait;

    // Wait until busy bit clears
    bool timeout = ata_wait();

    if(!timeout)
        goto normal_exit;
    else
        return EABSENT;

    write_wait:
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

int pata_do_transfer(uint16_t id, uint64_t lba, uint16_t* transfer_buffer, uint32_t sector_count, int transfer_dir, bool is_dma, bool is_48bit)
{
    // Initial checks
    if((transfer_buffer == NULL || transfer_buffer == KNULL))
        return 0;
    if(!sector_count)
        return 0;
    if(get_device(id) == KNULL)
        return EABSENT;
    if((get_device(id)->device_type & TYPE_ATAPI) == TYPE_ATAPI)
        return EINVAL_ARG;
    if(!get_device(id)->has_lba48 && (lba > 0x10000000 || sector_count > 0x100 || is_48bit))
        return EINVAL_ARG;

    if(!switch_device(id, 0))
        return EINVAL;
    else if(current_device->dev.processing_command)
        return EBUSY;

    // Send Read/Write Command
    int error_code = 0;
    uint16_t command_base = current_device->command_base;
    uint16_t control_base = current_device->control_base;

    uint8_t command = 0;

         if(!is_dma && !is_48bit && transfer_dir == TRANSFER_READ ) command = READ_SECTORS;
    else if(!is_dma &&  is_48bit && transfer_dir == TRANSFER_READ ) command = READ_SECTORS_EXT;
    else if( is_dma && !is_48bit && transfer_dir == TRANSFER_READ ) command = READ_DMA;
    else if( is_dma &&  is_48bit && transfer_dir == TRANSFER_READ ) command = READ_DMA_EXT;
    else if(!is_dma && !is_48bit && transfer_dir == TRANSFER_WRITE) command = WRITE_SECTORS;
    else if(!is_dma &&  is_48bit && transfer_dir == TRANSFER_WRITE) command = WRITE_SECTORS_EXT;
    else if( is_dma && !is_48bit && transfer_dir == TRANSFER_WRITE) command = WRITE_DMA;
    else if( is_dma &&  is_48bit && transfer_dir == TRANSFER_WRITE) command = WRITE_DMA_EXT;
    
    error_code = ata_send_command(id, command, 0, lba, sector_count, transfer_dir, is_48bit);

    if(error_code != 0)
    {
        ata_end_command(id);
        return error_code;
    }

    uint64_t buffer_index = 0;
    uint64_t word_count = (sector_count * current_device->dev.sector_size) >> 1;

    resume_tranfer:
    irq_fired = false;

    if((inb(control_base + ATA_ALT_STATUS) & STATUS_DRQ) == 0 && (inb(control_base + ATA_ALT_STATUS) & STATUS_ERR) != 0)
    {
        // We got an error here
        ata_end_command(id);
        return 1;
    }

    if((inb(control_base + ATA_ALT_STATUS) & (STATUS_DRQ | STATUS_BSY)) == 0 || word_count == 0)
        goto normal_exit;

    // We are in data transfer mode, do it
    klog_logln(ata_subsys, DEBUG, "(ata_dev%lld) Beginning PIO Data Transfer (%lld bytes)", id, word_count << 1);
    if(transfer_dir == TRANSFER_READ)
    {
        while(word_count--)
            transfer_buffer[buffer_index++] = inw(command_base + ATA_DATA);
    }
    else
    {
        while(word_count--)
            outw(command_base + ATA_DATA, transfer_buffer[buffer_index++]);
    }

    // Nothing else needs to be done for read
    if(transfer_dir == TRANSFER_READ)
        goto normal_exit;

    // Writes, on the other hand
    ata_wait();
    goto resume_tranfer;

    normal_exit:
    ata_end_command(id);
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
    if((transfer_buffer == NULL || transfer_buffer == KNULL) && transfer_dir == TRANSFER_WRITE)
        return 0;
    if(get_device(id) == KNULL)
        return EABSENT;
    if((get_device(id)->device_type & TYPE_ATAPI) != TYPE_ATAPI)
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

    error_code = ata_send_command(id, PACKET, /*(transfer_dir & 1) << 2 |*/ is_dma, transfer_size << 8, 0, TRANSFER_WRITE, false);
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
    ata_wait();

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
    klog_logln(ata_subsys, DEBUG, "(ata_dev%d) Begining Packet PIO Data Transfer (%d bytes)", id, byte_count);

    if(transfer_dir == 1)
    {
        if(transfer_buffer == NULL || transfer_buffer == KNULL)
        {
            while(word_count--)
                inw(command_base + ATA_DATA);
        }
        else
        {
            while(word_count--)
                transfer_buffer[buffer_index++] = inw(command_base + ATA_DATA);
        }
    }
    else
    {
        while(word_count--)
            outw(command_base + ATA_DATA, transfer_buffer[buffer_index++]);
    }

    ata_wait();

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
    ata_send_command(current_id, IDENTIFY_PACKET, 0x0000, 0, 0, TRANSFER_READ, false);

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

    ata_add_device((struct ata_dev*)controller);

    int err = ata_send_command(controller->dev.id, IDENTIFY, 0x0000, 0, 0, TRANSFER_READ, false);
    if(err == EABSENT)
    {
        klog_logln(ata_subsys, DEBUG, "ata_dev%d is absent, disabling", controller->dev.id);
        controller->dev.is_active = false;
        controller->dev.processing_command = false;
        current_id = ATA_INVALID_ID;
        current_device = KNULL;
        return controller;
    }

    uint8_t id_low = inb(command_base + ATA_LBAMID);
    uint8_t id_hi = inb(command_base + ATA_LBAHI);

         if(id_low == 0x14 && id_hi == 0xEB) init_atapi_device();
    else if(id_low == 0x3C && id_hi == 0xC3) klog_logln(ata_subsys, DEBUG, "Setting up SATA device");
    else if(id_low == 0x69 && id_hi == 0x96) klog_logln(ata_subsys, DEBUG, "Setting up SATAPI device");

    char* type_names[] = {"PATA", "ATAPI", "SATA", "SATAPI"};
    klog_logln(ata_subsys, DEBUG, "ATA Device Present (%d, %s)", controller->dev.id, type_names[controller->dev.device_type]);

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
pci_handle_ret_t ata_init_controller(struct pci_dev* dev)
{
    bool init_primary = true;
    bool init_secondary = true;

    klog_logln(ata_subsys, INFO, "Initializing IDE controller at %x:%x.%x", dev->bus, dev->device, dev->function);
    
    if((pci_read(dev, PCI_PROG_IF, 1) & 0b1010) != 0)
    {
        // Try and force the IDE channels into both native
        pci_write(dev, PCI_PROG_IF, 1, 0x0F);
        if((pci_read(dev, PCI_PROG_IF, 1) & 0x05) != 0x05)
        {
            // Try and force the IDE channels into both compatibility
            pci_write(dev, PCI_PROG_IF, 1, 0x00);
        }
        klog_logln(ata_subsys, DEBUG, "NewOpMode: %x", pci_read(dev, PCI_PROG_IF, 1));
    }

    uint8_t operating_mode = pci_read(dev, PCI_PROG_IF, 1);
    uint32_t command0_base = pci_read(dev, PCI_BAR0, 4) & ~0x3;
    uint32_t command1_base = pci_read(dev, PCI_BAR2, 4) & ~0x3;
    uint32_t control0_base = pci_read(dev, PCI_BAR1, 4) & ~0x3;
    uint32_t control1_base = pci_read(dev, PCI_BAR3, 4) & ~0x3;

    if((operating_mode & 0b0001) != (operating_mode & 0b0100) && (operating_mode & 0b1010) == 0)
    {
        klog_logln(ata_subsys, ERROR, "Operating mode mismatch (%d != %d)", (operating_mode & 0b0001), (operating_mode & 0b0100));
        return PCI_DEV_NOT_HANDLED;
    }

    if((operating_mode & 0b0101) == 5)
    {
        pci_alloc_irq(dev, 1, IRQ_LEGACY | IRQ_INTX);
    }

    FIXUP_BAR(command0_base, 0x1F0);
    FIXUP_BAR(control0_base, 0x3F6);
    FIXUP_BAR(command1_base, 0x170);
    FIXUP_BAR(control1_base, 0x376);

    // Write BARs back if we can change them
    if((operating_mode & 0b0010))
    {
        klog_logln(ata_subsys, DEBUG, "Fixing up secondary channel");
        pci_write(dev, PCI_BAR0, 4, command0_base);
        pci_write(dev, PCI_BAR1, 4, control0_base);
    }

    if((operating_mode & 0b1000))
    {
        klog_logln(ata_subsys, DEBUG, "Fixing up secondary channel");
        pci_write(dev, PCI_BAR2, 4, command0_base);
        pci_write(dev, PCI_BAR3, 4, control0_base);
    }

    klog_logln(ata_subsys, DEBUG, "Primary channel at %#lx-%#lx, %#lx", command0_base, command0_base+7, control0_base);
    klog_logln(ata_subsys, DEBUG, "Secondary channel at %#lx-%#lx, %#lx", command1_base, command1_base+7, control1_base);
    klog_logln(ata_subsys, DEBUG, "OpMode: %x", operating_mode);

    if(inb(control0_base + ATA_ALT_STATUS) == 0xFF)
        init_primary = false;
    if(inb(control1_base + ATA_ALT_STATUS) == 0xFF)
        init_secondary = false;
    
    if(!init_primary && !init_secondary)
    {
        klog_logln(ata_subsys, ERROR, "No IDE channels exist");
        return PCI_DEV_NOT_HANDLED;
    }

    if(init_primary)
    {
        klog_logln(ata_subsys, INFO, "Setting up primary IDE channel");
        // Use legacy interrupt if channel is in compatibility mode
        if((operating_mode & 0b0001) == 0)
            ic_irq_handle(14, LEGACY, (irq_function_t)pata_irq_handler);
        else
            pci_handle_irq(dev, 0, (irq_function_t)pata_irq_handler);
        
        init_ide_controller(command0_base, control0_base, (operating_mode & 0b0001) ? dev->irq_pin : 14, false);
        init_ide_controller(command0_base, control0_base, (operating_mode & 0b0001) ? dev->irq_pin : 14, true);
    }

    if(init_secondary)
    {
        klog_logln(ata_subsys, INFO, "Setting up secondary IDE channel");
        // Use legacy interrupt if channel is in compatibility mode
        if((operating_mode & 0b0100) == 0)
            ic_irq_handle(15, LEGACY, (irq_function_t)pata_irq_handler);
        else
            pci_handle_irq(dev, 0, (irq_function_t)pata_irq_handler);

        init_ide_controller(command1_base, control1_base, (operating_mode & 0b0100) ? dev->irq_pin : 15, false);
        init_ide_controller(command1_base, control1_base, (operating_mode & 0b0100) ? dev->irq_pin : 15, true);
    }

    current_id = ATA_INVALID_ID;
    current_device = KNULL;

    return PCI_DEV_HANDLED;
}

/**
 * @brief  Initializes the PATA side of the ATA driver
 * @note   
 * @param       id: ID of the ATA subsystem
 * @retval None
 */
void pata_init(uint16_t id)
{
    ata_subsys = id;
    pci_handle_dev(&pata_driver);
}
