#include <types.h>

#ifndef __MM_H__
#define __MM_H__ 1

size_t get_page_size();
void mmu_init();
void mmu_map_address(linear_addr_t* address, physical_addr_t* mapping);

void mm_init();
void mm_add_free_region(physical_addr_t base, size_t length);

#endif /* __MM_H__ */
