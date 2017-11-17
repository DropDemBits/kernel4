#include <types.h>

#ifndef __MM_H__
#define __MM_H__ 1

void mm_init();
void mm_map_address(linear_addr_t* address, physical_addr_t* mapping);

//void mm_add_free_region(physical_addr_t base, size_t length);

#endif /* __MM_H__ */
