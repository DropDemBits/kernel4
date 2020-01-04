#include <arch/io.h>

// Declarations to make the compiler happy
extern inline void io_wait();
extern inline void outb(uint16_t port, uint8_t value);
extern inline void outw(uint16_t port, uint16_t value);
extern inline void outl(uint16_t port, uint32_t value);
extern inline uint8_t inb(uint16_t port);
extern inline uint16_t inw(uint16_t port);
extern inline uint32_t inl(uint16_t port);
