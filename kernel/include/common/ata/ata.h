#ifndef __ATA_H__
#define __ATA_H__ 1

#include <common/types.h>
#include <common/util/locks.h>

#define TYPE_ATA 0
#define TYPE_ATAPI 1
#define TYPE_SATA 2
#define TYPE_SATAPI 3

#define TRANSFER_READ 1
#define TRANSFER_WRITE 0

struct ata_dev_info
{
    uint16_t general_config;
    uint16_t obsolete_0;
    uint16_t specific_config; // 3
    uint16_t obsolete_1[7]; // 4-9

    char serial_number[20];

    uint16_t obsolete_2[3];

    char firmware_revision[8];
    char model_name[40];

    uint16_t max_sector_transfer; // 47

    uint16_t trusted_computing_features; // 48
    uint16_t capabilities[2]; // 49-50

    uint16_t obsolete_3[2]; // 51-52

    uint16_t word_53;

    uint16_t reserved_0[256-50];
};

struct ata_dev
{
    struct pci_dev_t* dev;  // PCI Device backing

    uint8_t device_type;
    bool is_active;
    uint16_t id;

    mutex_t* dev_lock;
    bool processing_command;
    uint8_t interrupt;      // The interrupt of this device 

    // Common info
    uint64_t num_sectors;   // Number of sectors, in the sector size. Zero indicates a dynamic capacity
    uint32_t sector_size;   // The size of a sector in bytes

    // Common features
    bool has_lba48;
    
};

struct pata_dev
{
    struct ata_dev dev;

    uint16_t command_base;
    uint16_t control_base;
    uint8_t is_secondary;

    // ATA Device info (From either IDENTIFY or IDENTIFY_ATAPI command)
    uint16_t* device_info;
};

void ata_init();

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
 * @retval EINVAL if the device isn't ATAPI or doesn't support the command length
 *         EABSENT if the device doesn't exist
 *         0 if the command was sent successfully
 */
int atapi_send_command(uint16_t id, uint16_t* command, void* transfer_buffer, uint16_t transfer_size, int transfer_dir, bool is_dma, bool is_16b);

int pata_do_transfer(uint16_t id, uint64_t lba, void* transfer_buffer, uint32_t sector_count, int transfer_dir, bool is_dma, bool is_48bit);

#endif /* __ATA_H__ */
