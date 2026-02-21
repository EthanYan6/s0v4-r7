#ifndef DRIVER_SYS_H
#define DRIVER_SYS_H

#include <stdint.h>

#define CPU_CLOCK_HZ 48000000

void SYS_DelayMs(uint32_t Delay);
void SYS_ConfigureClocks(void);
void SYS_ConfigureSysCon(void);

#endif
