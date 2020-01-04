#include <arch/cpufuncs.h>

// Declarations to make the compiler happy
extern inline void membar_read();
extern inline void membar_write();

extern inline void intr_wait();
extern inline void hal_enable_interrupts_raw();
extern inline void hal_disable_interrupts_raw();
extern inline void busy_wait();

// Atomic Memory Functions
extern inline uint32_t lock_cmpxchg(uint32_t* data, uint32_t expected, uint32_t set);
extern inline void lock_xchg(uint32_t* data, uint32_t set);
extern inline uint32_t lock_read(uint32_t* data);

extern inline void hal_enable_interrupts(uint64_t flags);
extern inline cpu_flags_t hal_disable_interrupts();
