#ifndef __CPUFUNCS_H__
#define __CPUFUNCS_H__ 1

// Memory barriers (SMP-only)
#ifdef _ENABLE_SMP_
inline void membar_read()
{

}

inline void membar_write()
{
    
}
#else
// Empty placeholders
#define membar_read
#define membar_write
#endif /* _ENABLE_SMP */

// Caching functions

// Misc
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

// Atomic Memory Transactions
inline uint32_t lock_cmpxchg(uint32_t* data, uint32_t expected, uint32_t set)
{
    uint32_t current_value = 0;
    asm volatile("lock cmpxchg %0, (%1)":"=a"(current_value):"r"(set),"m"(data),"a"(expected):"eax","cc","memory");
    return current_value;
}

inline void lock_xchg(uint32_t* data, uint32_t set)
{
    asm volatile("lock xchg %%eax, (%1)"::"a"(set),"r"(data));
}

inline uint32_t lock_read(uint32_t* data)
{
    return *data;
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