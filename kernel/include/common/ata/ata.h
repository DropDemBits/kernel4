#include <common/types.h>
#include <common/util/locks.h>

#ifndef __ATA_H__
#define __ATA_H__ 1

#define TYPE_ATA 0
#define TYPE_ATAPI 1
#define TYPE_SATA 2
#define TYPE_SATAPI 3

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
    uint8_t device_type;
    bool is_active;
    uint16_t id;

    mutex_t* dev_lock;
    bool processing_command;

    // Common info
    uint64_t num_sectors; // Number of sectors, in the sector size. Zero indicates a dynamic capacity
    uint32_t sector_size; // The size of a sector in bytes

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

#endif /* __ATA_H__ */
