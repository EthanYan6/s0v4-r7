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

#define PRO_LEFT_W (LCD_WIDTH / 3)
#define PRO_TOP_ROW_H 16
#define PRO_INFO_Y 25
#define PRO_AUDIO_BARS_Y 34
#define PRO_SPECTRUM_TOP_Y  (PRO_AUDIO_BARS_Y + 2)
#define PRO_AUDIO_BARS_H    (LCD_HEIGHT - PRO_AUDIO_BARS_Y)
#define PRO_SPECTRUM_MAX_H  (LCD_HEIGHT - PRO_SPECTRUM_TOP_Y)
#define PRO_SPECTRUM_H_LEFT  (PRO_SPECTRUM_MAX_H + 4)
#define PRO_SPECTRUM_H_RIGHT (LCD_HEIGHT - PRO_SPECTRUM_TOP_Y)
#define PRO_AUDIO_BAR_CNT 20
#define PRO_VOICE_BARS 36
#define PRO_VOICE_MAX_H  (PRO_TOP_ROW_H - 2)
#define PRO_FREQ_Y 10
#define PRO_CH_Y 17
#define PRO_SPECTRUM_BAR_GAP 1
#define PRO_TX_OFF_CLEAR_MS 6000

static uint8_t voiceHistory[PRO_VOICE_BARS];
static uint32_t lastTxOffTime;
static bool wasTx;
static bool inTxOffCooldown;

static void pushVoiceSample(void) {
  if (gTxState == TX_ON) {
    wasTx = true;
    inTxOffCooldown = false;
    lastTxOffTime = 0;
    uint8_t i;
    for (i = 0; i < PRO_VOICE_BARS - 1; i++)
      voiceHistory[i] = voiceHistory[i + 1];
    {
      unsigned int l = MIN((unsigned)BK4819_GetAfTxRx() * 200u, 65535u);
      l = MIN(SQRT16(l), 124u);
      voiceHistory[PRO_VOICE_BARS - 1] =
          (uint8_t)ConvertDomain((int)l, 0, 124, 0, (int)PRO_VOICE_MAX_H);
    }
    return;
  }
  if (wasTx) {
    lastTxOffTime = Now();
    inTxOffCooldown = true;
    wasTx = false;
  }
  if (inTxOffCooldown && (uint32_t)(Now() - lastTxOffTime) >= PRO_TX_OFF_CLEAR_MS) {
    memset(voiceHistory, 0, sizeof(voiceHistory));
    inTxOffCooldown = false;
    lastTxOffTime = 0;
    return;
  }
  /* 松开 PTT 后 6s 内：继续左移并右侧补 0，波形持续往左走 */
  if (inTxOffCooldown) {
    uint8_t i;
    for (i = 0; i < PRO_VOICE_BARS - 1; i++)
      voiceHistory[i] = voiceHistory[i + 1];
    voiceHistory[PRO_VOICE_BARS - 1] = 0;
  }
}

static void renderLeftArea(void) {
  uint8_t x, cy = PRO_TOP_ROW_H / 2;
  bool showBars = (gTxState == TX_ON) ||
      (inTxOffCooldown && (uint32_t)(Now() - lastTxOffTime) < PRO_TX_OFF_CLEAR_MS);
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

static void renderFreqAndChannel(void) {
  const VFO *vfo = radio;
  uint32_t f =
      gTxState == TX_ON ? RADIO_GetTXF() : GetScreenF(vfo->rxF);
  const uint16_t fp1 = f / MHZ;
  const uint16_t fp2 = f / 100 % 1000;
  const uint8_t fp3 = f % 100;

  PrintBigDigitsEx(LCD_WIDTH - 25, PRO_FREQ_Y, POS_R, C_FILL, "%4u.%03u", fp1, fp2);
  PrintBigDigitsEx(LCD_WIDTH - 3, PRO_FREQ_Y, POS_R, C_FILL, "%02u", fp3);
  if (vfo->channel >= 0) {
    PrintSmallEx(LCD_WIDTH - 1, PRO_CH_Y + 1, POS_R, C_FILL, "MR %03u %s",
                 (unsigned)(vfo->channel + 1), vfo->name);
  } else {
    PrintSmallEx(LCD_WIDTH - 1, PRO_CH_Y + 1, POS_R, C_FILL, "VFO");
  }
}

static void proTuneTo(uint32_t f) {
  RADIO_TuneToSave(GetTuneF(f));
  radio->fixedBoundsMode = false;
  RADIO_SaveCurrentVFO();
}

#define RSSI_STRONG 150
#define PRO_SPECTRUM_BLOCK_H  2
#define PRO_SPECTRUM_BLOCK_GAP 1
#define PRO_SPECTRUM_H_MAX  (PRO_SPECTRUM_H_RIGHT - 12)

static void renderAudioBars(void) {
  uint8_t barW = (LCD_WIDTH - (PRO_AUDIO_BAR_CNT - 1) * PRO_SPECTRUM_BAR_GAP) / PRO_AUDIO_BAR_CNT;
  uint8_t step = barW + PRO_SPECTRUM_BAR_GAP;
  uint16_t rssi = RADIO_GetRSSI();
  int rs;
  uint8_t i, h, y_off, segH;
  int maxHi;
  uint8_t prof;

  if (rssi <= RSSI_MIN)
    rs = 0;
  else if (rssi >= RSSI_STRONG)
    rs = (int)PRO_SPECTRUM_H_LEFT;
  else
    rs = (int)((rssi - RSSI_MIN) * (unsigned)PRO_SPECTRUM_H_LEFT / (RSSI_STRONG - RSSI_MIN));

  PrintSmallEx(LCD_WIDTH - 1, PRO_SPECTRUM_TOP_Y, POS_R, C_FILL, "S%u %ddBm",
               DBm2S(Rssi2DBm(rssi), radio->rxF < VHF_UHF_BOUND_HZ), Rssi2DBm(rssi));
  PrintSmallEx(LCD_WIDTH - 1, PRO_SPECTRUM_TOP_Y + 9, POS_R, C_FILL, "%s",
               modulationTypeOptions[radio->modulation]);
  if (rs <= 0) return;
  for (i = 0; i < PRO_AUDIO_BAR_CNT; i++) {
    if (i <= 6)
      prof = (uint8_t)i;
    else if (i <= 16)
      prof = (uint8_t)((18 - i) / 2);
    else if (i == 17)
      prof = 1;
    else
      prof = 2;
    maxHi = (prof == 0) ? 1 : (int)PRO_SPECTRUM_H_MAX * (int)prof / 6;
    if (maxHi < 1) maxHi = 1;
    h = 1 + (int)((maxHi - 1) * (unsigned)rs / (int)PRO_SPECTRUM_H_LEFT);
    if (h > maxHi) h = (uint8_t)maxHi;
    if (h < 1) h = 1;
    for (y_off = 0; y_off < h; y_off += PRO_SPECTRUM_BLOCK_H + PRO_SPECTRUM_BLOCK_GAP) {
      segH = (h - y_off) >= PRO_SPECTRUM_BLOCK_H ? PRO_SPECTRUM_BLOCK_H : (uint8_t)(h - y_off);
      if (segH)
        FillRect((int16_t)(i * step), (int16_t)(LCD_HEIGHT - h + y_off), barW, segH, C_FILL);
    }
  }
}

void PRO_init(void) {
  memset(voiceHistory, 0, sizeof(voiceHistory));
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
  FillRect(0, 0, LCD_WIDTH, LCD_HEIGHT, C_CLEAR);

  pushVoiceSample();
  renderFreqAndChannel();
  renderLeftArea();
  STATUSLINE_renderVfo1Row(PRO_INFO_Y);

  renderAudioBars();
}

#pragma GCC diagnostic ignored "-Wunused-parameter"
bool PRO_key(KEY_Code_t key, Key_State_t state) {
  if (key == KEY_EXIT && state == KEY_RELEASED) {
    APPS_exit();
    return true;
  }

  if (key == KEY_PTT) {
    RADIO_ToggleTX(state == KEY_PRESSED);
    return true;
  }
  if (key == KEY_MENU) {
    if (state == KEY_LONG_PRESSED)
      APPS_run(APP_SETTINGS);
    else if (state == KEY_RELEASED)
      APPS_run(APP_APPS_LIST);
    return true;
  }
  if ((state == KEY_RELEASED || state == KEY_LONG_PRESSED_CONT) && (key == KEY_UP || key == KEY_DOWN)) {
    if (RADIO_IsChMode())
      CHANNELS_Next(key == KEY_UP);
    else
      RADIO_NextF(key == KEY_UP);
    RADIO_SaveCurrentVFODelayed();
    return true;
  }
  if (key == KEY_3 && state == KEY_LONG_PRESSED) {
    RADIO_ToggleVfoMR();
    return true;
  }
  if (key == KEY_6 && state == KEY_LONG_PRESSED) {
    RADIO_ToggleTxPower();
    return true;
  }
  if (!RADIO_IsChMode() && state == KEY_RELEASED && key >= KEY_0 && key <= KEY_9) {
    gFInputCallback = proTuneTo;
    APPS_run(APP_FINPUT);
    APPS_key(key, state);
    return true;
  }
  if (key == KEY_F && state == KEY_RELEASED) {
    gChEd = *radio;
    if (RADIO_IsChMode()) {
      gChEd.meta.type = TYPE_CH;
    }
    APPS_run(APP_CH_CFG);
    return true;
  }

  return false;
}
