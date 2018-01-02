#include <types.h>
#include <liballoc.h>

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
