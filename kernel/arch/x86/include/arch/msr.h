#include <common/types.h>

#ifndef __MSR_H__
#define __MSR_H__ 1

#define MSR_IA32_APIC_BASE      0x1B
#define MSR_IA32_MISC_ENABLE    0x1A0
#define MSR_IA32_PAT            0x277

static inline uint64_t msr_read(uint32_t msr_index)
{
    uint32_t eax, edx;
    asm volatile("rdmsr":"=a"(eax),"=d"(edx):"c"(msr_index));
    uint64_t value = (eax) | ((uint64_t)edx << 32);
    return value;
}

static inline void msr_write(uint32_t msr_index, uint64_t value)
{
    uint32_t eax = (uint32_t)(value & 0xFFFFFFFF), edx = (uint32_t)(value >> 32);
    asm volatile("wrmsr"::"a"(eax),"d"(edx),"c"(msr_index));
}

#endif /* __MSR_H__ */