#include <common/types.h>

#ifndef __UART_H__
#define __UART_H__ 1

void uart_init();
void uart_writec(char c);
void uart_writestr(const char* str, size_t length);
uint8_t uart_read();

#endif
