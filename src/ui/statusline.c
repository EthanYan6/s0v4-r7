#include "statusline.h"
#include "../apps/apps.h"
#include "../driver/eeprom.h"
#include "../driver/si473x.h"
#include "../driver/st7565.h"
#include "../external/FreeRTOS/include/FreeRTOS.h"
#include "../external/FreeRTOS/include/projdefs.h"
#include "../external/FreeRTOS/include/timers.h"
#include "../helper/bands.h"
#include "../helper/battery.h"
#include "../helper/channels.h"
#include "../helper/measurements.h"
#include "../helper/numnav.h"
#include "../radio.h"
#include "../scheduler.h"
#include "components.h"
#include "graphics.h"
#include <string.h>

static uint8_t previousBatteryLevel = 255;
static bool showBattery = true;

static uint32_t lastEepromWrite = 0;
static uint32_t lastTickerUpdate = 0;

static char statuslineText[32] = {0};
static char statuslineTicker[32] = {0};
static StaticTimer_t eepromRWTimerBuffer;
static TimerHandle_t eepromRWTimer;

void STATUSLINE_SetText(const char *pattern, ...) {
  char statuslineTextNew[32] = {0};
  va_list args;
  va_start(args, pattern);
  vsnprintf(statuslineTextNew, 31, pattern, args);
  va_end(args);
  if (strcmp(statuslineText, statuslineTextNew)) {
    strncpy(statuslineText, statuslineTextNew, 31);
    gRedrawScreen = true;
  }
}

void STATUSLINE_SetTickerText(const char *pattern, ...) {
  char statuslineTextNew[32] = {0};
  va_list args;
  va_start(args, pattern);
  vsnprintf(statuslineTextNew, 31, pattern, args);
  va_end(args);
  if (strcmp(statuslineTicker, statuslineTextNew)) {
    strncpy(statuslineTicker, statuslineTextNew, 31);
    gRedrawScreen = true;
  }
  lastTickerUpdate = Now();
}

void STATUSLINE_update(void) {
  BATTERY_UpdateBatteryInfo();
  uint8_t level = gBatteryPercent / 10;
  if (gBatteryPercent < BAT_WARN_PERCENT) {
    showBattery = !showBattery;
    gRedrawScreen = true;
  } else {
    showBattery = true;
  }
  if (previousBatteryLevel != level) {
    previousBatteryLevel = level;
    gRedrawScreen = true;
  }

  if ((bool)lastEepromWrite != gEepromWrite) {
    lastEepromWrite = gEepromWrite ? Now() : 0;
    gRedrawScreen = true;
  }
  if (lastEepromWrite && Now() - lastEepromWrite > 500) {
    lastEepromWrite = gEepromWrite = false;
    gRedrawScreen = true;
  }

  if (Now() - lastTickerUpdate > 5000) {
    statuslineTicker[0] = '\0';
  }
}

/* 功率短名：很低/低/中/高 -> UL / L / M / H */
static const char *const powerShortNames[] = {"UL", "L", "M", "H"};
/* 亚音短名：无 / CT / DCS / -DCS */
static const char *const codeShortNames[] = {"-", "CT", "DC", "-D"};

/* 顶部状态栏统一垂直范围：y=0～5，所有图标与文字在此高度内对齐 */
#define STATUS_ICON_TOP    0
#define STATUS_ICON_BOTTOM 5

/* 在 (x,y) 绘制宽窄带（复用 radio 的 bwNames），仅 BK4819 有效 */
static void drawBwAt(uint8_t x, uint8_t y) {
  if (RADIO_GetRadio() == RADIO_BK4819 && radio->bw <= BK4819_FILTER_BW_26k)
    PrintSmall(x, y, "%s", bwNames[radio->bw]);
}

/* 1 Moto R7 页面：左侧天线+5格信号（一体）+ 直频 + 功率 + 亚音 + 芯片 + 宽窄带，右侧仅电池，无主文案 */
static void STATUSLINE_renderVfo1(void) {
  const uint8_t BASE_Y = STATUS_ICON_BOTTOM;  /* 文字基线，与图标底边对齐 */
  const uint8_t ANTENNA_W = 3;
  const uint8_t BARS_LEFT = 3;
  const uint8_t BAR_W = 2;
  const uint8_t BAR_GAP = 1;
  const uint8_t BAR_BASE_Y = STATUS_ICON_BOTTOM;
  const uint8_t BAR_HEIGHTS[] = {1, 2, 3, 4, 5};
  const uint8_t ICON_GAP = 2;
  const uint8_t SIGNAL_BARS_END = BARS_LEFT + 5 * (BAR_W + BAR_GAP);
  const uint8_t DIRECT_FREQ_W = 6;
  const uint8_t DIRECT_FREQ_LEFT = SIGNAL_BARS_END + ICON_GAP;
  const uint8_t POWER_LEFT = DIRECT_FREQ_LEFT + DIRECT_FREQ_W + ICON_GAP;
  const uint8_t POWER_W = 10;
  const uint8_t CODE_LEFT = POWER_LEFT + POWER_W + ICON_GAP;
  const uint8_t CODE_W = 8;
  const uint8_t CHIP_LEFT = CODE_LEFT + CODE_W + ICON_GAP;
  const uint8_t CHIP_W = 10;
  const uint8_t BW_LEFT = CHIP_LEFT + CHIP_W + ICON_GAP;

  /* 1. 天线 + 5 格信号（一体），均在 STATUS_ICON_TOP～BOTTOM 内 */
  DrawVLine(1, STATUS_ICON_TOP + 1, (uint8_t)(STATUS_ICON_BOTTOM - STATUS_ICON_TOP), C_FILL);
  DrawHLine(0, STATUS_ICON_TOP, ANTENNA_W, C_FILL);
  uint16_t rssi = RADIO_GetRSSI();
  int n = ConvertDomain((int)rssi, (int)RSSI_MIN, (int)RSSI_MAX, 0, 5);
  if (n < 0) n = 0;
  if (n > 5) n = 5;
  for (uint8_t i = 0; i < 5; i++) {
    uint8_t h = BAR_HEIGHTS[i];
    uint8_t x = BARS_LEFT + i * (BAR_W + BAR_GAP);
    uint8_t topY = BAR_BASE_Y - h;
    if (i < (uint8_t)n) {
      FillRect(x, topY, BAR_W, h, C_FILL);
    }
  }

  /* 2. 直频图标 |->| ，两竖线下方短 1px（只画到 y=4） */
  if (RADIO_GetTXF() == radio->rxF) {
    uint8_t x0 = DIRECT_FREQ_LEFT;
    const uint8_t directFreqH = STATUS_ICON_BOTTOM - STATUS_ICON_TOP;  /* 5，比图标带短 1px */
    DrawVLine(x0, STATUS_ICON_TOP, directFreqH, C_FILL);
    DrawVLine(x0 + 5, STATUS_ICON_TOP, directFreqH, C_FILL);
    DrawHLine(x0 + 1, 2, 2, C_FILL);
    PutPixel(x0 + 3, 1, C_FILL);
    PutPixel(x0 + 4, 2, C_FILL);
    PutPixel(x0 + 3, 3, C_FILL);
  }

  /* 功率、亚音、芯片文字整体上移 1px */
  const uint8_t TEXT_Y = BASE_Y - 1;
  if (radio->power <= TX_POW_HIGH) {
    PrintSmall(POWER_LEFT, TEXT_Y, "%s", powerShortNames[radio->power]);
  }
  if (radio->code.tx.type < 4u) {
    PrintSmall(CODE_LEFT, TEXT_Y, "%s", codeShortNames[radio->code.tx.type]);
  }
  if (RADIO_GetRadio() <= RADIO_SI4732) {
    PrintSmall(CHIP_LEFT, TEXT_Y, "%s", shortRadioNames[RADIO_GetRadio()]);
  }
  drawBwAt(BW_LEFT, TEXT_Y);

  /* 右侧仅电池 */
  if (showBattery) {
    if (gSettings.batteryStyle) {
      PrintSmallEx(LCD_WIDTH - 1, BASE_Y, POS_R, C_INVERT, "%u%%",
                   gBatteryPercent);
    } else {
      UI_Battery(previousBatteryLevel);
    }
  }
  if (gSettings.batteryStyle == BAT_VOLTAGE) {
    PrintSmallEx(LCD_WIDTH - 1 - 16, BASE_Y, POS_R, C_FILL, "%u.%02uV",
                 gBatteryVoltage / 100, gBatteryVoltage % 100);
  }
}

void STATUSLINE_renderVfo1Row(uint8_t y) {
  if (RADIO_GetRadio() <= RADIO_SI4732) {
    PrintSmall(0, y, "%s", shortRadioNames[RADIO_GetRadio()]);
  }
  if (radio->power <= TX_POW_HIGH) {
    PrintSmall(10, y, "%s", powerShortNames[radio->power]);
  }
  if (radio->code.tx.type < 4u) {
    PrintSmall(22, y, "%s", codeShortNames[radio->code.tx.type]);
  }
  drawBwAt(34, y);
  if (showBattery) {
    PrintSmallEx(LCD_WIDTH - 1, y, POS_R, C_INVERT, "%u%%", gBatteryPercent);
  }
  if (gSettings.batteryStyle == BAT_VOLTAGE) {
    PrintSmallEx(LCD_WIDTH - 1 - 16, y, POS_R, C_FILL, "%u.%02uV",
                 gBatteryVoltage / 100, gBatteryVoltage % 100);
  }
}

void STATUSLINE_render(void) {
  if (gCurrentApp == APP_PRO) {
    return;
  }
  UI_ClearStatus();

  const uint8_t BASE_Y = 4;

  DrawHLine(0, 6, LCD_WIDTH, C_FILL);

  /* 1 Moto R7 页面：仅电池 + 左侧天线与 5 格信号 */
  if (gCurrentApp == APP_VFO1) {
    STATUSLINE_renderVfo1();
    return;
  }

  if (showBattery) {
    if (gSettings.batteryStyle) {
      PrintSmallEx(LCD_WIDTH - 1, BASE_Y, POS_R, C_INVERT, "%u%%",
                   gBatteryPercent);
    } else {
      UI_Battery(previousBatteryLevel);
    }
  }

  if (gSettings.batteryStyle == BAT_VOLTAGE) {
    PrintSmallEx(LCD_WIDTH - 1 - 16, BASE_Y, POS_R, C_FILL, "%u.%02uV",
                 gBatteryVoltage / 100, gBatteryVoltage % 100);
  }

  char icons[8] = {'\0'};
  uint8_t idx = 0;

  if (gEepromWrite) {
    icons[idx++] = SYM_EEPROM_W;
  }

  /* if (SVC_Running(SVC_BEACON)) {
    icons[idx++] = SYM_BEACON;
  } */

  if (gSettings.dw != DW_OFF) {
    icons[idx++] = SYM_DW;
  }

  /* if (SVC_Running(SVC_FC)) {
    icons[idx++] = SYM_FC;
  }

  if (SVC_Running(SVC_SCAN)) {
    icons[idx++] = SYM_SCAN;
  } */

  if (LOOT_Size() == LOOT_SIZE_MAX) {
    icons[idx++] = SYM_LOOT_FULL;
  }

  if (gMonitorMode) {
    icons[idx++] = SYM_MONITOR;
  }

  if (RADIO_GetRadio() == RADIO_BK1080 || isSi4732On) {
    icons[idx++] = SYM_BROADCAST;
  }

  if (gSettings.upconverter) {
    icons[idx++] = SYM_CONVERTER;
  }

  if (gSettings.keylock) {
    icons[idx++] = SYM_LOCK;
  }

  if ((gCurrentApp == APP_CH_LIST || gCurrentApp == APP_VFO1 ||
       gCurrentApp == APP_VFO2 || gCurrentApp == APP_LOOT_LIST)) {
    UI_Scanlists(LCD_XCENTER - 13, 0, gSettings.currentScanlist);
  }

  PrintSymbolsEx(LCD_WIDTH - 1 -
                     (gSettings.batteryStyle == BAT_VOLTAGE ? 38 : 18),
                 BASE_Y, POS_R, C_FILL, "%s", icons);

  PrintSmall(0, BASE_Y,
             statuslineTicker[0] == '\0' ? statuslineText : statuslineTicker);
}

void STATUSLINE_renderCurrentBand() {
  if (gIsNumNavInput) {
    STATUSLINE_SetText("Select: %s", gNumNavInput);
  } else {
    if (gCurrentBand.name[0] == '-' && gCurrentBand.name[1] == '\0') {
      STATUSLINE_SetText("");
    } else {
      if (gCurrentBand.meta.type == TYPE_BAND_DETACHED) {
        STATUSLINE_SetText("*%s", gCurrentBand.name);
        /* } else if (SVC_Running(SVC_SCAN)) {
          STATUSLINE_SetText("=%s", gCurrentBand.name); */
      } else {
        STATUSLINE_SetText(radio->fixedBoundsMode ? "=%s:%u" : "%s:%u",
                           gCurrentBand.name,
                           CHANNELS_GetChannel(&gCurrentBand, radio->rxF) + 1);
      }
    }
  }
}
