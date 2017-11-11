#include <multiboot.h>
#include <multiboot2.h>
#include <stdint.h>

/* Declerations */
extern uint32_t mb_mem_lower;
extern uint32_t mb_mem_upper;

extern uint32_t mb_mods_count;
extern uint32_t mb_mods_addr;

extern uint64_t mb_framebuffer_addr;
extern uint32_t mb_framebuffer_width;
extern uint32_t mb_framebuffer_height;
extern uint8_t mb_framebuffer_type;
extern uint8_t mb_framebuffer_bpp;

void multiboot_parse();
