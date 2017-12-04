#include <multiboot.h>
#include <multiboot2.h>

#ifndef __MB2PARSE_H__
#define __MB2PARSE_H__ 1

/* Declerations */
extern uint32_t mb_mem_lower;
extern uint32_t mb_mem_upper;

extern uint32_t mb_mods_count;
extern uint32_t mb_mods_addr;

void multiboot_parse();
void multiboot_reclaim();

#endif /* __MB2PARSE_H__ */
