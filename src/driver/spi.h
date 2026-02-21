#ifndef DRIVER_SPI_H
#define DRIVER_SPI_H

#include "../inc/dp32g030/spi.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint8_t MSTR;
  uint8_t SPR;
  uint8_t CPHA;
  uint8_t CPOL;
  uint8_t LSB;
  uint8_t TF_CLR;
  uint8_t RF_CLR;
  uint8_t TXFIFO_HFULL;
  uint8_t TXFIFO_EMPTY;
  uint8_t RXFIFO_HFULL;
  uint8_t RXFIFO_FULL;
  uint8_t RXFIFO_OVF;
} SPI_Config_t;

void SPI0_Init(void);
void SPI_WaitForUndocumentedTxFifoStatusBit(void);

void SPI_Disable(volatile uint32_t *pCR);
void SPI_Configure(volatile SPI_Port_t *pPort, SPI_Config_t *pConfig);
void SPI_ToggleMasterMode(volatile uint32_t *pCr, bool bIsMaster);
void SPI_Enable(volatile uint32_t *pCR);

#endif
