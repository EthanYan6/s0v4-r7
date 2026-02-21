#include "../inc/dp32g030/crc.h"
#include "../driver/crc.h"

void CRC_Init(void) {
  CRC_CR = 0 | CRC_CR_CRC_EN_BITS_DISABLE | CRC_CR_INPUT_REV_BITS_NORMAL |
           CRC_CR_INPUT_INV_BITS_NORMAL | CRC_CR_OUTPUT_REV_BITS_NORMAL |
           CRC_CR_OUTPUT_INV_BITS_NORMAL | CRC_CR_DATA_WIDTH_BITS_8 |
           CRC_CR_CRC_SEL_BITS_CRC_16_CCITT;
  CRC_IV = 0;
}

uint16_t CRC_Calculate(const void *pBuffer, uint16_t Size) {
  const uint8_t *pData = (const uint8_t *)pBuffer;
  uint16_t i, Crc;

  CRC_CR = (CRC_CR & ~CRC_CR_CRC_EN_MASK) | CRC_CR_CRC_EN_BITS_ENABLE;

  for (i = 0; i < Size; i++) {
    CRC_DATAIN = pData[i];
  }
  Crc = (uint16_t)CRC_DATAOUT;

  CRC_CR = (CRC_CR & ~CRC_CR_CRC_EN_MASK) | CRC_CR_CRC_EN_BITS_DISABLE;

  return Crc;
}
