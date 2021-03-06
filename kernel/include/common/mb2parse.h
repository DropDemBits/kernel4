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

#include <common/multiboot.h>
#include <common/multiboot2.h>

#ifndef __MB2PARSE_H__
#define __MB2PARSE_H__ 1

/* Declerations */
extern uint32_t mb_mem_lower;
extern uint32_t mb_mem_upper;

extern uint32_t mb_mods_count;
extern uint32_t mb_mods_addr;

extern uint64_t mb_rsdp_addr;

void multiboot_parse();
void multiboot_reclaim();

#endif /* __MB2PARSE_H__ */
