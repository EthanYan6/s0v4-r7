#ifndef DRIVER_CRC_H
#define DRIVER_CRC_H

#include <stdint.h>

void CRC_Init(void);
uint16_t CRC_Calculate(const void *pBuffer, uint16_t Size);

#endif
