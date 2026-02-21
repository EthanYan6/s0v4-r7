#ifndef DRIVER_UART_H
#define DRIVER_UART_H

#include "../helper/channels.h"
#include <stdbool.h>
#include <stdint.h>

void UART_Init(void);
void UART_Send(const void *pBuffer, uint32_t Size);
void UART_printf(const char *str, ...);

bool UART_IsCommandAvailable(void);
void UART_HandleCommand(void);
void Log(const char *pattern, ...);
void LogUart(const char *const str);
void PrintCh(uint16_t chNum, CH *ch);

#endif
