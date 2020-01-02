/**
 * Copyright (C) 2018 DropDemBits
 * 
 * This file is part of Kernel4.
 * 
 * Kernel4 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Kernel4 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Kernel4.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#ifndef __KFUNCS_H__
#define __KFUNCS_H__ 1

#include <stdarg.h>
#include <common/util/klog.h>

struct intr_stack;

void __attribute__((noreturn)) kpanic(const char* message_string, ...);
void __attribute__((noreturn)) kpanic_intr(struct intr_stack *stack, const char* message_string, ...);
void __attribute__((noreturn)) kvpanic(const char* message_string, va_list args);
void __attribute__((noreturn)) kvpanic_intr(struct intr_stack *stack, const char* message_string, va_list args);

#endif /* __KFUNCS_H__ */
