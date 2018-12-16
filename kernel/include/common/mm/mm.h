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

#include <common/types.h>

#ifndef __MM_H__
#define __MM_H__ 1

// Access Types
#define MMU_ACCESS_R                0x00000001  // If not set, indicates the mapping is not active
#define MMU_ACCESS_W                0x00000002  // Set: Mapping is writeable
#define MMU_ACCESS_X                0x00000004  // Set: Mapping is executable
#define MMU_ACCESS_USER             0x00000008  // Set: Mapping is user-accessable
#define MMU_ACCESS_RO               ( MMU_ACCESS_R )
#define MMU_ACCESS_RW               ( MMU_ACCESS_R | MMU_ACCESS_W )
#define MMU_ACCESS_RX               ( MMU_ACCESS_R | MMU_ACCESS_X )
#define MMU_ACCESS_RWX              ( MMU_ACCESS_R | MMU_ACCESS_W | MMU_ACCESS_X )
#define MMU_ACCESS_MASK             0x0000000F

// Caching types
#define MMU_CACHE_UC                0x00000000
#define MMU_CACHE_UCO               0x00000010
#define MMU_CACHE_WT                0x00000020
#define MMU_CACHE_WB                0x00000030
#define MMU_CACHE_WC                0x00000040
#define MMU_CACHE_WP                0x00000050
#define MMU_CACHE_MASK              0x000000F0

#define MMU_FLAGS_DEFAULT           ( MMU_ACCESS_RW | MMU_CACHE_WB )

#define MMU_MAPPING_ERROR           -1  // General mapping error
#define MMU_MAPPING_INVAL           -2  // Invalid arguments
#define MMU_MAPPING_EXISTS          -3  // Mapping already exists
#define MMU_MAPPING_NOT_CAPABLE     -4  // Unable to map with the requested flags 

enum MemoryRegionType
{
    MEM_REGION_AVAILABLE = 1,
    MEM_REGION_RESERVED,
    MEM_REGION_RECLAIMABLE,
};

/**
 * Address space
 */
typedef struct paging_context
{
    unsigned long phybase;
} paging_context_t;

void mmu_init();

/**
 * @brief  Creates a mapping between the linear and physical address
 * @note   
 * @param  address: The linear address to map
 * @param  mapping: The physical address to map to
 * @param  flags: The flags used in the mapping. Default access flags is RW, Supervisor and caching is WB
 * @retval See MMU_MAPPING_xxx error codes
 */
int mmu_map(unsigned long address, unsigned long mapping, uint32_t flags);

/**
 * @brief  Sets up a new mapping attribute for the mapping specified
 * @note   Implicitly invalidates the page entry for cache reloading
 * @param  address: The linear address to change the mapping attributes
 * @param  flags: The new attributes to set to
 * @retval See MMU_MAPPING_xxx error codes
 */
int mmu_change_attr(unsigned long address, uint32_t flags);

/**
 * @brief  Unmaps the specified addres
 * @note   
 * @param  address: The address mapping to unlink
 * @param  erase_entry: If true, the entry should be completely erased
 * @retval True if unmapping was done successfully, false otherwise
 */
bool mmu_unmap(unsigned long address, bool erase_entry);

/**
 * @brief  Gets the mapping address from the linear address
 * @note   
 * @param  address: The linear address
 * @retval The mapped physical address, or 0 if an error occured
 */
unsigned long mmu_get_mapping(unsigned long address);

/**
 * @brief  Checks whether the specifed address can be accessed
 * @note   
 * @param  address: The address to check
 * @param  flags: The access types to check (R/Present,W,X,U)
 * @retval True if it can, false otherwise
 */
bool mmu_check_access(unsigned long address, uint32_t flags);

// MMU Context
void mmu_switch_address_space(uint64_t page_context_base);
paging_context_t* mmu_create_context();
void mmu_destroy_context(paging_context_t* addr_context);
paging_context_t* mmu_current_context();
void mmu_switch_context(paging_context_t* addr_context);
void mmu_set_context(paging_context_t* addr_context);
void mmu_set_temp_context(paging_context_t* addr_context);
void mmu_exit_temp_context();

// Physical Memory Manager
void mm_early_init();
void mm_init();

/**
 * @brief  Adds a memory area to be considered during memory map building
 * @note   
 * @param  base: 
 * @param  length: 
 * @param  type: 
 * @retval None
 */
void mm_add_area(unsigned long base, unsigned long length, uint32_t type);
void mm_add_region(unsigned long base, size_t length, uint32_t type);
unsigned long mm_alloc(size_t size);
void mm_free(unsigned long addr, size_t size);

void heap_init();

#endif /* __MM_H__ */
