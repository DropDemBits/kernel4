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
#include <common/liballoc.h>

#ifndef __MM_H__
#define __MM_H__ 1

size_t get_page_size();
void mmu_init();
int mmu_map(linear_addr_t* address);
int mmu_map_direct(linear_addr_t* address, physical_addr_t* mapping);
void mmu_unmap(linear_addr_t* address);
bool mmu_is_usable(linear_addr_t* address);

void mm_init();
void mm_add_region(physical_addr_t base, size_t length, uint32_t type);
void* mm_alloc(size_t size);
void mm_free(void* addr, size_t size);

void heap_init();

#endif /* __MM_H__ */
