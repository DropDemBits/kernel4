#ifndef __CPUFUNCS_H__
#define __CPUFUNCS_H__ 1

// Typedefs
#if defined(__x86_64__)
typedef uint64_t cpu_flags_t;
#else
typedef uint32_t cpu_flags_t;
#endif

// Memory barriers (SMP-only)
#ifdef _ENABLE_SMP_
static inline void membar_read()
{

}

static inline void membar_write()
{
    
}
#else
// Empty placeholders
#define membar_read
#define membar_write
#endif /* _ENABLE_SMP */

// Caching functions

// Misc
static inline void intr_wait()
{
    asm("hlt");
}

static inline void hal_enable_interrupts_raw()
{
    asm volatile("sti");
}

static inline void hal_disable_interrupts_raw()
{
    asm volatile("cli");
}

static inline void busy_wait()
{
    asm volatile("pause");
}

// Atomic Memory Transactions
static inline uint32_t lock_cmpxchg(uint32_t* data, uint32_t expected, uint32_t set)
{
    uint32_t current_value = 0;
    asm volatile("lock cmpxchg %1, (%2)":"=a"(current_value):"d"(set),"r"((uintptr_t)data),"a"(expected):"cc","memory");
    return current_value;
}

static inline void lock_xchg(uint32_t* data, uint32_t set)
{
    asm volatile("lock xchg %%eax, (%1)"::"a"(set),"r"(data));
}

static inline uint32_t lock_read(uint32_t* data)
{
    return *data;
}

#if defined(__x86_64__)
static inline void hal_enable_interrupts(uint64_t flags)
{
    asm volatile("push %%rax\n\tpopfq"::"a"(flags));
}

static inline cpu_flags_t hal_disable_interrupts()
{
    cpu_flags_t flags = 0;
    asm volatile("pushfq\n\tpopq %%rax":"=a"(flags));
    asm volatile("cli");
    return flags;
}
#else
static inline void hal_enable_interrupts(uint64_t flags)
{
    asm volatile("push %%eax\n\tpopf"::"a"((uint32_t)flags));
}

static inline cpu_flags_t hal_disable_interrupts()
{
    cpu_flags_t flags = 0;
    asm volatile("pushf\n\tpopl %%eax":"=a"(flags));
    asm volatile("cli");
    return flags;
}
#endif

#endif /* __CPUFUNCS_H__ */