#include <x86_64/types.h>

#ifndef __IO_H__
#define __IO_H__ 1

#ifdef __NO_OPTIMIZE__
#    define FPREFIX static
#else
#    define FPREFIX inline
#endif

FPREFIX void outb(uint16_t port, uint8_t value)
{
    asm volatile ( "outb %0, %1" : : "a"(value), "Nd"(port) );
}

FPREFIX void outw(uint16_t port, uint16_t value)
{
    asm volatile ( "outw %0, %1" : : "a"(value), "Nd"(port) );
}

FPREFIX void outl(uint16_t port, uint32_t value)
{
    asm volatile ( "outl %0, %1" : : "a"(value), "Nd"(port) );
}

FPREFIX uint8_t inb(uint16_t port)
{
    uint8_t ret;
    asm volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

FPREFIX uint16_t inw(uint16_t port)
{
    uint16_t ret;
    asm volatile ( "inw %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

FPREFIX uint32_t inl(uint16_t port)
{
    uint32_t ret;
    asm volatile ( "inl %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

FPREFIX void io_wait()
{
    asm volatile ( "outb %%al, $0x80" : : "a"(0) );
}

#undef FPREFIX
#endif
