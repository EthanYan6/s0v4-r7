#include "pro.h"
#include "../driver/bk4819.h"
#include "../external/FreeRTOS/include/FreeRTOS.h"
#include "../external/FreeRTOS/include/portable.h"
#include "../helper/channels.h"
#include "../helper/measurements.h"
#include "../misc.h"
#include "../radio.h"
#include "../scheduler.h"
#include "../ui/graphics.h"
#include "../ui/statusline.h"
#include "apps.h"
#include "chcfg.h"
#include "finput.h"
#include <string.h>

#define VHF_UHF_BOUND_HZ 26000000

#define PRO_TOP_ROW_H 16
#define PRO_LEFT_CY 11
#define PRO_INFO_Y 25
#define PRO_LINE_Y 32
#define PRO_FREQ_LABEL_Y 37
#define PRO_FREQ_SPAN_HZ 400000u
#define PRO_BARS_TOP_Y 38
#define PRO_VOICE_BARS 36
#define PRO_VOICE_MAX_H  (PRO_TOP_ROW_H - 2)
#define PRO_FREQ_Y 10
#define PRO_CH_Y 17
#define PRO_RIGHT_X 41
#define PRO_TX_OFF_CLEAR_MS 6000
#define PRO_BAR_W 3
#define PRO_BAR_ROW_H 2
#define PRO_BAR_ROWS ((LCD_HEIGHT - PRO_BARS_TOP_Y) / PRO_BAR_ROW_H)
#define PRO_DEBRIS_MAX 6
#define PRO_DEBRIS_W 3
#define PRO_BAR_CLEAR_MS (2 * PRO_TX_OFF_CLEAR_MS)

static uint8_t voiceHistory[PRO_VOICE_BARS];
static uint32_t lastTxOffTime;
static bool wasTx;
static bool inTxOffCooldown;

static uint8_t proBarLevel[PRO_BAR_ROWS];
static uint8_t proBarFall[PRO_BAR_ROWS];
static uint32_t lastBarSignalTime;
static uint8_t proDebrisX[PRO_DEBRIS_MAX], proDebrisY[PRO_DEBRIS_MAX];
static uint8_t proDebrisH[PRO_DEBRIS_MAX];
static uint8_t proDebrisActive[PRO_DEBRIS_MAX];
static uint16_t proDebrisSeed;
static uint8_t proDebrisPhase;

static void shiftVoice(void) {
  uint8_t i;
  for (i = 0; i < PRO_VOICE_BARS - 1; i++)
    voiceHistory[i] = voiceHistory[i + 1];
}

static void pushVoiceSample(void) {
  if (gTxState == TX_ON) {
    wasTx = true;
    inTxOffCooldown = false;
    lastTxOffTime = 0;
    shiftVoice();
    { unsigned int l = MIN((unsigned)BK4819_GetAfTxRx() * 200u, 65535u); l = MIN(SQRT16(l), 124u); voiceHistory[PRO_VOICE_BARS - 1] = (uint8_t)ConvertDomain((int)l, 0, 124, 0, (int)PRO_VOICE_MAX_H); }
    return;
  }
  if (wasTx) { lastTxOffTime = Now(); inTxOffCooldown = true; wasTx = false; }
  if (inTxOffCooldown) {
    uint32_t t = Now();
    if ((uint32_t)(t - lastTxOffTime) >= PRO_TX_OFF_CLEAR_MS) {
      memset(voiceHistory, 0, sizeof(voiceHistory));
      inTxOffCooldown = false;
      lastTxOffTime = 0;
      return;
    }
    shiftVoice();
    voiceHistory[PRO_VOICE_BARS - 1] = 0;
  }
}

static void renderLeftArea(void) {
  uint8_t x, cy = PRO_LEFT_CY;
  uint32_t t = Now();
  bool showBars = (gTxState == TX_ON) || (inTxOffCooldown && (uint32_t)(t - lastTxOffTime) < PRO_TX_OFF_CLEAR_MS);
  DrawHLine(0, cy, PRO_VOICE_BARS, C_FILL);
  if (!showBars) return;
  for (x = 0; x < PRO_VOICE_BARS; x++) {
    uint8_t h = (uint8_t)((int)voiceHistory[x] * (10 + (int)(x & 5u)) / 10);
    if (h > PRO_VOICE_MAX_H) h = PRO_VOICE_MAX_H;
    if (h) {
      int y0 = (int)cy - (int)(h / 2);
      if (y0 < 0) y0 = 0;
      FillRect((int16_t)x, (int16_t)y0, 1, (int16_t)h, C_FILL);
    }
  }
}

static void renderFreqAndChannel(uint32_t f) {
  PrintBigDigitsEx(LCD_WIDTH - 25, PRO_FREQ_Y, POS_R, C_FILL, "%4u.%03u", (uint16_t)(f / MHZ), (uint16_t)(f / 100 % 1000));
  PrintBigDigitsEx(LCD_WIDTH - 3, PRO_FREQ_Y, POS_R, C_FILL, "%02u", (uint8_t)(f % 100));
  PrintSmallEx(PRO_RIGHT_X, PRO_CH_Y + 1, POS_L, C_FILL, "%s", modulationTypeOptions[radio->modulation]);
  if (radio->channel >= 0)
    PrintSmallEx(LCD_WIDTH - 1, PRO_CH_Y + 1, POS_R, C_FILL, "MR %03u %s", (unsigned)(radio->channel + 1), radio->name);
  else
    PrintSmallEx(LCD_WIDTH - 1, PRO_CH_Y + 1, POS_R, C_FILL, "VFO");
}

static void proTuneTo(uint32_t f) {
  RADIO_TuneToSave(GetTuneF(f));
  radio->fixedBoundsMode = false;
  RADIO_SaveCurrentVFO();
}

static void renderFreqLineAndLabels(void) {
  DrawHLine(0, PRO_LINE_Y, LCD_WIDTH, C_FILL);
  DrawVLine(64, PRO_LINE_Y + 1, 4, C_FILL);
  PrintSmallEx(0, PRO_FREQ_LABEL_Y, POS_L, C_FILL, "-%u.%u", 0, 4);
  PrintSmallEx(LCD_WIDTH - 1, PRO_FREQ_LABEL_Y, POS_R, C_FILL, "+%u.%u", 0, 4);
}

static void updateDebris(void) {
  uint8_t i;
  proDebrisPhase++;
  for (i = 0; i < PRO_DEBRIS_MAX; i++) {
    if (!proDebrisActive[i]) continue;
    if ((proDebrisPhase & 1u) == 0) proDebrisY[i]++;
    if (proDebrisY[i] >= LCD_HEIGHT) proDebrisActive[i] = 0;
  }
  if ((Now() & 31u) >= 28u) {
    for (i = 0; i < PRO_DEBRIS_MAX; i++) {
      if (proDebrisActive[i]) continue;
      proDebrisSeed = (uint16_t)(proDebrisSeed * 31u + (uint16_t)Now() + 1u);
      proDebrisX[i] = (uint8_t)(proDebrisSeed % LCD_WIDTH);
      if ((uint8_t)(proDebrisX[i] - 61) <= 4u)
        proDebrisX[i] = (uint8_t)((proDebrisX[i] + 10) % LCD_WIDTH);
      proDebrisY[i] = PRO_BARS_TOP_Y;
      proDebrisH[i] = (uint8_t)(1u + (proDebrisSeed & 1u));
      proDebrisActive[i] = 1;
      break;
    }
  }
}

static void updateBars(void) {
  uint16_t rssi = RADIO_GetRSSI();
  uint8_t i;
  if (gTxState != TX_ON && ((unsigned)(rssi - 2) >= 98u)) {
    lastBarSignalTime = Now();
    for (i = PRO_BAR_ROWS - 1; i > 0; i--) {
      proBarLevel[i] = proBarLevel[i - 1];
      proBarFall[i] = proBarFall[i - 1];
    }
    proBarLevel[0] = 1;
    proBarFall[0] = 0;
  } else {
    if ((uint32_t)(Now() - lastBarSignalTime) >= PRO_BAR_CLEAR_MS) {
      memset(proBarLevel, 0, sizeof(proBarLevel));
      memset(proBarFall, 0, sizeof(proBarFall));
    } else {
      for (i = 0; i < PRO_BAR_ROWS; i++)
        if (proBarLevel[i] != 0 && proBarFall[i] != 255)
          proBarFall[i]++;
    }
  }
  updateDebris();
}

static void renderBars(void) {
  uint8_t i;
  int16_t y, x = 63;
  for (i = 0; i < PRO_BAR_ROWS; i++) {
    if (proBarLevel[i] == 0) continue;
    y = (int16_t)(PRO_BARS_TOP_Y + i * PRO_BAR_ROW_H + proBarFall[i]);
    if (y < 64) FillRect(x, y, PRO_BAR_W, PRO_BAR_ROW_H, C_FILL);
  }
  for (i = 0; i < PRO_DEBRIS_MAX; i++)
    if (proDebrisActive[i])
      FillRect((int16_t)proDebrisX[i], (int16_t)proDebrisY[i], PRO_DEBRIS_W, (int16_t)proDebrisH[i], C_FILL);
}

void PRO_init(void) {
  memset(voiceHistory, 0, sizeof(voiceHistory));
  memset(proBarLevel, 0, sizeof(proBarLevel));
  memset(proBarFall, 0, sizeof(proBarFall));
  lastBarSignalTime = 0;
  memset(proDebrisActive, 0, sizeof(proDebrisActive));
  proDebrisSeed = 0;
  proDebrisPhase = 0;
  lastTxOffTime = 0;
  wasTx = false;
  inTxOffCooldown = false;
}

void PRO_update(void) {
  RADIO_CheckAndListen();
  gRedrawScreen = true;
  vTaskDelay(pdMS_TO_TICKS(60));
}

void PRO_render(void) {
  uint32_t f = gTxState == TX_ON ? RADIO_GetTXF() : GetScreenF(radio->rxF);
  uint8_t xAfterBw;
  uint16_t rssi;
  FillRect(0, 0, LCD_WIDTH, LCD_HEIGHT, C_CLEAR);
  pushVoiceSample();
  renderFreqAndChannel(f);
  renderLeftArea();
  STATUSLINE_renderVfo1RowEx(PRO_INFO_Y, 1, &xAfterBw);
  rssi = RADIO_GetRSSI();
  { int dbm = Rssi2DBm(rssi); PrintSmallEx((uint8_t)(xAfterBw + 8), PRO_INFO_Y, POS_L, C_FILL, "S%u %ddBm", DBm2S(dbm, radio->rxF < VHF_UHF_BOUND_HZ), dbm); }
  renderFreqLineAndLabels();
  updateBars();
  renderBars();
}

#pragma GCC diagnostic ignored "-Wunused-parameter"
bool PRO_key(KEY_Code_t key, Key_State_t state) {
  if (key == KEY_EXIT && state == KEY_RELEASED) { APPS_exit(); return true; }
  if (key == KEY_PTT) { RADIO_ToggleTX(state == KEY_PRESSED); return true; }
  if (key == KEY_MENU) {
    if (state == KEY_LONG_PRESSED)
      APPS_run(APP_SETTINGS);
    else if (state == KEY_RELEASED)
      APPS_run(APP_APPS_LIST);
    return true;
  }
  if ((state == KEY_RELEASED || state == KEY_LONG_PRESSED_CONT) && (key == KEY_UP || key == KEY_DOWN)) {
    if (RADIO_IsChMode()) CHANNELS_Next(key == KEY_UP);
    else RADIO_NextF(key == KEY_UP);
    RADIO_SaveCurrentVFODelayed();
    return true;
  }
  if (key == KEY_3 && state == KEY_LONG_PRESSED) { RADIO_ToggleVfoMR(); return true; }
  if (key == KEY_6 && state == KEY_LONG_PRESSED) { RADIO_ToggleTxPower(); return true; }
  if (!RADIO_IsChMode() && state == KEY_RELEASED && key >= KEY_0 && key <= KEY_9) {
    gFInputCallback = proTuneTo;
    APPS_run(APP_FINPUT);
    APPS_key(key, state);
    return true;
  }
  if (key == KEY_F && state == KEY_RELEASED) {
    gChEd = *radio;
    if (RADIO_IsChMode()) gChEd.meta.type = TYPE_CH;
    APPS_run(APP_CH_CFG);
    return true;
  }
  return false;
}
