/**
 * Copyright (C) 2018 DropDemBits
 * 
 * This file is part of Kernel4.
 * 
 * Kernel4 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Kernel4 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Kernel4.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#include <string.h>
#include <stdio.h>

#include <common/types.h>
#include <common/acpi.h>
#include <common/multiboot.h>
#include <common/multiboot2.h>
#include <common/mm/mm.h>
#include <common/tty/fb.h>
#include <common/util/klog.h>

typedef multiboot2_memory_map_t mb2_mmap_t;
typedef multiboot_memory_map_t mb_mmap_t;

struct multiboot_tags_header
{
    uint32_t size;
    uint32_t reserved;
};

struct acpi_rsdp
{
    char Sig[8];
    uint8_t Checksum;
    char OEMID[6];
    uint8_t Version;
    uint32_t RsdtAddress;
};

struct acpi_xsdp
{
    struct acpi_rsdp RSDP;

    uint32_t Length;
    uint64_t XsdtAddress;
    uint8_t ExtendedChecksum;
    uint8_t Rsvd[3];
};

void* multiboot_ptr = (void*)0;
size_t multiboot_magic = 0;
// 4KiB aligned pointer
size_t multiboot_base = 0;
size_t multiboot_size = 0;

/* Multiboot info */
// Basic memory information
uint32_t mb_mem_lower;
uint32_t mb_mem_upper;

// Will always be 32bit ints
uint32_t initrd_start = 0xDEADBEEF;
uint32_t initrd_size = 0;

// RSDP Physical Location
uint64_t mb_rsdp_addr = 0;

/* Forward Declerations */
void parse_mb1();
void parse_mb2();

const char* region_names[] = {
    "Avaliable",
    "Reserved",
    "ACPI Reclaimable",
    "ACPI NVS",
    "Bad RAM",
};

static uint64_t find_rsdp(uint32_t search_base, uint32_t search_limit)
{
#ifndef __X86__
    void* address = (void*)((uintptr_t)search_base & ~0xF);

    while(address < search_limit)
    {
        uint32_t* sig = (uint32_t*)address;
        if(ACPI_VALIDATE_RSDP_SIG(sig))
        {
            // Check csum
            struct acpi_xsdp* xsdp = (struct acpi_xsdp*)address;
            uint8_t* xsdp_bytes = (uint8_t*)address;
            uint8_t csum = 0;

            for(int i = 0; i < sizeof(struct acpi_rsdp); i++)
                csum += xsdp_bytes[i];

            if(xsdp->RSDP.Version >= 2)
            {
                for(int i = sizeof(struct acpi_rsdp); i < sizeof(struct acpi_xsdp); i++)
                    csum += xsdp_bytes[i];
            }

            if(!csum)
                return (uint64_t)address;
        }
        
        // Address will always be 16 byte aligned
        address += 16;
    }
#endif
    return 0;
}

void multiboot_parse()
{
    // Check which multiboot we're working with

    switch (multiboot_magic) {
        case MULTIBOOT2_BOOTLOADER_MAGIC:
            klog_early_logln(DEBUG, "Parsing Multiboot 2 structure");
            parse_mb2();
            break;
        case MULTIBOOT_BOOTLOADER_MAGIC:
            klog_early_logln(DEBUG, "Parsing Multiboot 1 structure");
            parse_mb1();
            break;
        default:
            // Not loaded by multiboot loader
            break;
    }

}

/**
 * Parsing the Multiboot info structure is easy, just copy values over.
 */
void parse_mb1()
{
    multiboot_info_t* mb1 = (multiboot_info_t*)multiboot_ptr;
    uint32_t flags = mb1->flags;
    multiboot_base = ((size_t)mb1 + 0xFFF) & ~0xFFF;
    multiboot_size = 0x1000;

    if(flags & MULTIBOOT_INFO_MEMORY)
    {
        mb_mem_lower = mb1->mem_lower;
        mb_mem_upper = mb1->mem_upper;
    }

    if(flags & MULTIBOOT_INFO_MODS)
    {
        // Module information
        struct multiboot_mod_list *module = (struct multiboot_mod_list*)((uintptr_t)mb1->mods_addr);

        if(strcmp((const char*)((uintptr_t)module->cmdline),"initrd.tar") == 0)
        {
            initrd_start = module->mod_start;
            initrd_size = module->mod_end - initrd_start;
            klog_early_logln(DEBUG, "Found initrd.tar");
        }
    }

    if(flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO)
    {
        fb_info.base_addr = mb1->framebuffer_addr;
        fb_info.width = mb1->framebuffer_width;
        fb_info.height = mb1->framebuffer_height;
        fb_info.type = mb1->framebuffer_type;
        fb_info.pitch = mb1->framebuffer_pitch;
        fb_info.bits_pp = mb1->framebuffer_bpp;
        fb_info.bytes_pp = mb1->framebuffer_bpp >> 3;

        if(fb_info.type == TYPE_INDEXED)
        {
            fb_info.palette_size = mb1->framebuffer_palette_num_colors;
            fb_info.palette_addr = mb1->framebuffer_palette_addr;
        }
        else if (fb_info.type == TYPE_RGB)
        {
            fb_info.red_position = mb1->framebuffer_red_field_position;
            fb_info.red_mask_size = mb1->framebuffer_red_mask_size;
            fb_info.green_position = mb1->framebuffer_green_field_position;
            fb_info.green_mask_size = mb1->framebuffer_green_mask_size;
            fb_info.blue_position = mb1->framebuffer_blue_field_position;
            fb_info.blue_mask_size = mb1->framebuffer_blue_mask_size;
        }
    }

    if(flags & MULTIBOOT_INFO_BOOT_LOADER_NAME)
        klog_early_logln(INFO, "Loaded by bootloader \"%s\"", (const char*)((uintptr_t)mb1->boot_loader_name));

    // This needs to be last as to not overwrite the rest of multiboot things
    if(flags & MULTIBOOT_INFO_MEM_MAP)
    {
        klog_early_logln(INFO, "Memory Regions:");
        mb_mmap_t* mmap = (mb_mmap_t*)((uintptr_t)mb1->mmap_addr);

        while((size_t)mmap < mb1->mmap_addr + mb1->mmap_length) {
            mb2_mmap_t* actual = (mb2_mmap_t*)((uintptr_t)mmap + sizeof(mmap->size));

            klog_early_logln(INFO, "base: 0x%08llx, length: 0x%08llx, type(%lx): %s", actual->addr, actual->len, actual->type, region_names[(actual->type-1) % 5]);

            if(actual->type != 0)
                mm_add_area(actual->addr, actual->len, actual->type);

            mmap = (mb_mmap_t*) ((uintptr_t)mmap + mmap->size + sizeof(mmap->size));
        }
    }

#ifdef __X86__
    // Only find the RSDP on x86 machines
    klog_early_logln(INFO, "Searching for RSDP...");
    uint32_t address = 0;

    // Begin with searching in the EBDA
    uint32_t ebda_seg = *((uint16_t*)0x40E) << 4;
    klog_early_logln(INFO, "- in EBDA (0x%05p)", ebda_seg);
    address = find_rsdp(ebda_seg, ebda_seg + 1024);

    if(address == 0)
    {
        // Search in the BIOS ROM region (0xE0000 - 0xFFFFF)
        klog_early_logln(INFO, "- in BIOS ROM");
        address = find_rsdp(0xE0000, 0xFFFFF);
    }

    if(address != 0)
    {
        klog_early_logln(INFO, "Found RSDP @ 0x%05p", address);
        mb_rsdp_addr = (uint64_t)address;
    }
    else
    {
        klog_early_logln(INFO, "Unable to find RSDP (Pre-ACPI / (U)EFI machine?)");
    }

#endif
}

/**
 * Parsing the Multiboot2 info structure requires looping over all the tags
 */
void parse_mb2()
{
    // Check address alignment
    if(((size_t)multiboot_ptr) & 7)
    {
        // Our address is unaligned... Do something about that.
        return;
    }

    struct multiboot_tag *tag = (struct multiboot_tag*)multiboot_ptr + 1;
    struct multiboot_tag_mmap *mmap_tag = KNULL;
    uint32_t info_size = *((uint32_t*)multiboot_ptr);
    mb2_mmap_t *mmap;

    for (;
         tag->type != MULTIBOOT_TAG_TYPE_END;
         tag = (struct multiboot_tag *) ((uint8_t *) tag + ((tag->size + 7) & ~7)))
    {
        switch (tag->type) {
            case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
                mb_mem_lower = ((struct multiboot_tag_basic_meminfo*) tag)->mem_lower;
                mb_mem_upper = ((struct multiboot_tag_basic_meminfo*) tag)->mem_upper;
                break;
            case MULTIBOOT_TAG_TYPE_MODULE:
                if(strcmp(((struct multiboot_tag_module *) tag)->cmdline,"initrd.tar") == 0)
                {
                    initrd_start = ((struct multiboot_tag_module *) tag)->mod_start;
                    initrd_size = ((struct multiboot_tag_module *) tag)->mod_end - initrd_start;
                    klog_early_logln(DEBUG, "Found initrd.tar");
                }
                break;
            case MULTIBOOT_TAG_TYPE_MMAP:
                // Walk through later
                mmap_tag = ((struct multiboot_tag_mmap *) tag);
                break;
            case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
            {
                struct multiboot_tag_framebuffer *fb = ((struct multiboot_tag_framebuffer *) tag);
                fb_info.base_addr = fb->common.framebuffer_addr;
                fb_info.width = fb->common.framebuffer_width;
                fb_info.height = fb->common.framebuffer_height;
                fb_info.type = fb->common.framebuffer_type;
                fb_info.pitch = fb->common.framebuffer_pitch;
                fb_info.bits_pp = fb->common.framebuffer_bpp;
                fb_info.bytes_pp = fb->common.framebuffer_bpp >> 3;
                if(fb_info.type == TYPE_INDEXED)
                {
                    fb_info.palette_size = fb->framebuffer_palette_num_colors;
                    // We need to copy the palette into a different area, as this will be unmapped at mmu_init
                    // fb_info.palette_addr = (unsigned long)&(fb->framebuffer_palette);
                }
                else if (fb_info.type == TYPE_RGB)
                {
                    fb_info.red_position = fb->framebuffer_red_field_position;
                    fb_info.red_mask_size = fb->framebuffer_red_mask_size;
                    fb_info.green_position = fb->framebuffer_green_field_position;
                    fb_info.green_mask_size = fb->framebuffer_green_mask_size;
                    fb_info.blue_position = fb->framebuffer_blue_field_position;
                    fb_info.blue_mask_size = fb->framebuffer_blue_mask_size;
                }
            }
                break;
            case MULTIBOOT_TAG_TYPE_ELF_SECTIONS:
                break;
            case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME:
                klog_early_logln(INFO, "Loaded by bootloader \"%s\"", ((struct multiboot_tag_string*)tag)->string);
                break;
            case MULTIBOOT_TAG_TYPE_ACPI_OLD:
                if(mb_rsdp_addr != 0)
                    break;
            case MULTIBOOT_TAG_TYPE_ACPI_NEW:
                // Copy address of the RSDP copy
                mb_rsdp_addr = (uint64_t)((size_t)(tag+1));
                klog_early_logln(INFO, "Found RSDP @ %p", mb_rsdp_addr);
                klog_early_logln(INFO, "%.8s v%d", ((struct acpi_xsdp*)((uintptr_t)mb_rsdp_addr))->RSDP.Sig, ((struct acpi_xsdp*)((uintptr_t)mb_rsdp_addr))->RSDP.Version);

                break;
            default:
                break;
        }
    }

    // Check if we have been passed a memory map
    if(mmap_tag == KNULL) return;

    // 4KiB align info size
    info_size = (info_size + 0xFFF) & ~0xFFF;

    klog_early_logln(INFO, "Memory Regions:");
    for (mmap = mmap_tag->entries;
        (uint8_t *) mmap < ((uint8_t *) mmap_tag + mmap_tag->size);
        mmap = (mb2_mmap_t *)((unsigned long) mmap + mmap_tag->entry_size))
    {
        if(mmap->type == 0)
            continue;

        //klog_early_logln(INFO, "base: 0x%08llx, length: 0x%08llx, type: %s", mmap->addr, mmap->len, region_names[mmap->type-1]);
        mm_add_area(mmap->addr, mmap->len, mmap->type);

        // Reserve multiboot info region
        multiboot_base = (unsigned long)multiboot_ptr & ~0xFFF;
        multiboot_size = info_size;
    }
}
