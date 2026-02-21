#include "memview.h"
#include "../driver/eeprom.h"
#include "../helper/measurements.h"
#include "../settings.h"
#include "../ui/graphics.h"
#include "apps.h"

static uint32_t page = 0;
static const uint8_t PAGE_SZ = 64;
static uint16_t pagesCount;

void MEMVIEW_Init(void) { pagesCount = SETTINGS_GetEEPROMSize() / PAGE_SZ; }

void MEMVIEW_Render(void) {
  uint8_t buf[64] = {0};
  EEPROM_ReadBuffer(page * PAGE_SZ, buf, PAGE_SZ);

  for (uint8_t i = 0; i < PAGE_SZ; ++i) {
    uint8_t col = i % 8;
    uint8_t row = i / 8;
    uint8_t rowYBL = row * 6 + 8 + 5;

    if (i % 8 == 0) {
      PrintSmall(0, rowYBL, "%u", page * PAGE_SZ + i);
    }

    PrintSmall(16 + col * 9, rowYBL, "%02x", buf[i]);
    PrintSmall(88 + col * 5, rowYBL, "%c", IsPrintable(buf[i]));
  }
}

bool MEMVIEW_key(KEY_Code_t key, Key_State_t state) {
  switch (key) {
  case KEY_EXIT:
    APPS_exit();
    return true;
  case KEY_UP:
  case KEY_DOWN:
    page = IncDecU(page, 0, pagesCount, key != KEY_UP);
    return true;
  case KEY_3:
  case KEY_9:
    page =
        AdjustU(page, 0, pagesCount, (key == KEY_3 ? -8196 : 8196) / PAGE_SZ);
    return true;
  case KEY_MENU:
    return false;
  default:
    break;
  }
  return true;
}
