#include <types.h>

void mm_init();
void mm_map_address(linear_addr_t* address, physical_addr_t* mapping);

//void mm_add_free_region(physical_addr_t base, size_t length);
