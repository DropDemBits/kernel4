#ifndef __CPUFUNCS_H__
#define __CPUFUNCS_H__ 1

inline void intr_wait()
{
    asm("hlt");
}

inline void hal_enable_interrupts_raw()
{
    asm volatile("sti");
}

inline void hal_disable_interrupts_raw()
{
    asm volatile("cli");
}

inline void busy_wait()
{
    asm volatile("pause");
}

#if defined(__x86_64__)
inline void hal_enable_interrupts(uint64_t flags)
{
    asm volatile("push %%rax\n\tpopfq"::"a"(flags));
}

inline uint64_t hal_disable_interrupts()
{
    uint64_t flags = 0;
    asm volatile("pushfq\n\tpopq %%rax":"=a"(flags));
    asm volatile("cli");
    return flags;
}
#else
inline void hal_enable_interrupts(uint64_t flags)
{
    asm volatile("push %%eax\n\tpopf"::"a"((uint32_t)flags));
}

inline uint64_t hal_disable_interrupts()
{
    uint32_t flags = 0;
    asm volatile("pushf\n\tpopl %%eax":"=a"(flags));
    asm volatile("cli");
    return (uint64_t)flags;
}
#endif

#endif /* __CPUFUNCS_H__ */