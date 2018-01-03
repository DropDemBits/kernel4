#include <stdarg.h>
#include <stack_state.h>

#ifndef __KFUNCS_H__
#define __KFUNCS_H__

void __attribute__((noreturn)) kpanic(const char* message_string, ...);
void __attribute__((noreturn)) kpanic_intr(struct intr_stack *stack, const char* message_string, ...);
void __attribute__((noreturn)) kvpanic(const char* message_string, va_list args);
void __attribute__((noreturn)) kvpanic_intr(struct intr_stack *stack, const char* message_string, va_list args);

#endif /* __KFUNCS_H__ */
