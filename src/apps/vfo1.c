#include "vfo1.h"
#include "../apps/textinput.h"
#include "../driver/bk4819.h"
#include "../misc.h"
#include "../helper/measurements.h"
#include "../driver/uart.h"
#include "../external/FreeRTOS/include/FreeRTOS.h"
#include "../external/FreeRTOS/include/portable.h"
#include "../external/FreeRTOS/include/timers.h"
#include "../external/FreeRTOS/portable/GCC/ARM_CM0/portmacro.h"
#include "../helper/bands.h"
#include "../helper/channels.h"
#include "../helper/lootlist.h"
#include "../helper/measurements.h"
#include "../helper/numnav.h"
#include "../radio.h"
#include "../scheduler.h"
#include "../ui/components.h"
#include "../ui/graphics.h"
#include "../ui/spectrum.h"
#include "../ui/statusline.h"
#include "apps.h"
#include "chcfg.h"
#include "chlist.h"
#include "finput.h"

bool gVfo1ProMode = false;

static uint8_t menuIndex = 0;
static bool registerActive = false;

static char String[16];

static TimerHandle_t eepromWriteTimer = NULL;
static StaticTimer_t vfoSaveTimerBuffer;

static const RegisterSpec registerSpecs[] = {
    {"Gain", BK4819_REG_13, 0, 0xFFFF, 1},
    {"IF", 0x3D, 0, 0xFFFF, 100},
    {"DEV", 0x40, 0, 0xFFF, 10},
    RS_RF_FILT_BW,
    {"CMP", 0x31, 3, 1, 1},
    {"MIC", 0x7D, 0, 0xF, 1},

    {"AGCL", 0x49, 0, 0b1111111, 1},
    {"AGCH", 0x49, 7, 0b1111111, 1},
    {"AFC", 0x73, 0, 0xFF, 1},
};

static void UpdateRegMenuValue(RegisterSpec s, bool add) {
  uint16_t v, maxValue;

  if (s.num == BK4819_REG_13) {
    v = radio->gainIndex;
    maxValue = ARRAY_SIZE(gainTable) - 1;
  } else if (s.num == 0x73) {
    v = BK4819_GetAFC();
    maxValue = 8;
  } else {
    v = BK4819_GetRegValue(s);
    maxValue = s.mask;
  }

  if (add && v <= maxValue - s.inc) {
    v += s.inc;
  } else if (!add && v >= 0 + s.inc) {
    v -= s.inc;
  }

  if (s.num == BK4819_REG_13) {
    RADIO_SetGain(v);
    RADIO_SaveCurrentVFO();
  } else if (s.num == 0x73) {
    BK4819_SetAFC(v);
  } else {
    if (s.num == BK4819_REG_40) {
      gSettings.deviation = v / 10;
      SETTINGS_Save();
    }
    BK4819_SetRegValue(s, v);
  }
}

static void startABScan() {
  uint32_t F1 = gVFO[0].rxF;
  uint32_t F2 = gVFO[1].rxF;

  if (F1 > F2) {
    SWAP(F1, F2);
  }

  gCurrentBand = defaultBand;
  gCurrentBand.meta.type = TYPE_BAND_DETACHED;
  gCurrentBand.rxF = F1;
  gCurrentBand.txF = F2;
  gCurrentBand.step = radio->step;

  APPS_run(APP_SCANER);
}

static void setChannel(uint16_t v) { RADIO_TuneToCH(v); }

static void tuneTo(uint32_t f) {
  RADIO_TuneToSave(GetTuneF(f));
  radio->fixedBoundsMode = false;
  RADIO_SaveCurrentVFO();
}

static char message[16] = {'\0'};
static void sendDtmf() {
  RADIO_ToggleTX(true);
  if (gTxState == TX_ON) {
    BK4819_EnterDTMF_TX(true);
    BK4819_PlayDTMFString(message, true, 100, 100, 100, 100);
    RADIO_ToggleTX(false);
  }
}

void VFO1_init(void) {
  if (!gVfo1ProMode) {
    gVfo1ProMode = gSettings.iAmPro;
  }
  RADIO_LoadCurrentVFO();
}

void VFO1_update(void) {
  RADIO_CheckAndListen();
  gRedrawScreen = true;
  vTaskDelay(pdMS_TO_TICKS(60));
}

bool VFOPRO_key(KEY_Code_t key, Key_State_t state) {
  if (key == KEY_PTT) {
    RADIO_ToggleTX(state == KEY_PRESSED);
    return true;
  }
  if (state == KEY_LONG_PRESSED) {
    switch (key) {
    case KEY_4:
      if (RADIO_GetRadio() != RADIO_BK4819) {
        gShowAllRSSI = !gShowAllRSSI;
      }
      return true;
    case KEY_5:
      registerActive = !registerActive;
      return true;
    default:
      break;
    }
  }

  bool isSsb = RADIO_IsSSB();

  if (state == KEY_RELEASED) {
    switch (key) {
    case KEY_1:
    case KEY_7:
      RADIO_UpdateStep(key == KEY_1);
      return true;
    case KEY_3:
    case KEY_9:
      RADIO_UpdateSquelchLevel(key == KEY_3);
      return true;
    case KEY_4:
      return true;
    case KEY_SIDE1:
      if (!gVfo1ProMode) {
        gMonitorMode = !gMonitorMode;
        return true;
      }
    case KEY_SIDE2:
      if (RADIO_GetRadio() == RADIO_SI4732 && isSsb) {
        RADIO_TuneToSave(radio->rxF + (key == KEY_SIDE1 ? 1 : -1));
        return true;
      }
      break;
    case KEY_EXIT:
      if (registerActive) {
        registerActive = false;
        return true;
      }
      break;
    default:
      break;
    }
  }

  if (state == KEY_RELEASED) {
    switch (key) {
    case KEY_0:
      RADIO_ToggleModulation();
      return true;
    case KEY_6:
      RADIO_ToggleListeningBW();
      return true;
    case KEY_2:
    case KEY_8:
      if (registerActive) {
        UpdateRegMenuValue(registerSpecs[menuIndex], key == KEY_2);
      } else {
        menuIndex =
            IncDecU(menuIndex, 0, ARRAY_SIZE(registerSpecs), key != KEY_2);
      }
      return true;
    case KEY_5:
      gFInputCallback = tuneTo;
      APPS_run(APP_FINPUT);
      return true;
    default:
      break;
    }
  }

  return false;
}

bool VFO1_keyEx(KEY_Code_t key, Key_State_t state, bool isProMode) {
  if ((!gVfo1ProMode || gCurrentApp == APP_VFO2) && state == KEY_RELEASED &&
      RADIO_IsChMode()) {
    if (!gIsNumNavInput && key <= KEY_9) {
      NUMNAV_Init(radio->channel, 0, CHANNELS_GetCountMax() - 1);
      gNumNavCallback = setChannel;
    }
    if (gIsNumNavInput) {
      NUMNAV_Input(key);
      return true;
    }
  }

  if (isProMode && VFOPRO_key(key, state)) {
    return true;
  }

  if (key == KEY_PTT && !gIsNumNavInput) {
    RADIO_ToggleTX(state == KEY_PRESSED);
    return true;
  }

  if (state == KEY_RELEASED || state == KEY_LONG_PRESSED_CONT) {
    bool isSsb = RADIO_IsSSB();
    switch (key) {
    case KEY_UP:
    case KEY_DOWN:
      if (RADIO_IsChMode()) {
        CHANNELS_Next(key == KEY_UP);
      } else {
        RADIO_NextF(key == KEY_UP);
      }
      RADIO_SaveCurrentVFODelayed();
      return true;
    case KEY_SIDE1:
    case KEY_SIDE2:
      if (RADIO_GetRadio() == RADIO_SI4732 && isSsb) {
        RADIO_TuneToSave(radio->rxF + (key == KEY_SIDE1 ? 5 : -5));
        return true;
      }
      break;
    default:
      break;
    }
  }

  bool longHeld = state == KEY_LONG_PRESSED;
  bool simpleKeypress = state == KEY_RELEASED;

  if (longHeld) {
    switch (key) {
    case KEY_EXIT:
      startABScan();
      return true;
    case KEY_3:
      RADIO_ToggleVfoMR();
      VFO1_init();
      return true;
    case KEY_5:
      return true;
    case KEY_6:
      RADIO_ToggleTxPower();
      return true;
    case KEY_0:
      RADIO_ToggleModulation();
      return true;
    case KEY_STAR:
      APPS_run(APP_SCANER);
      return true;
    case KEY_SIDE1:
      return true;
    case KEY_SIDE2:
      return true;
    default:
      break;
    }
  }

  if (simpleKeypress) {
    switch (key) {
    case KEY_0:
    case KEY_1:
    case KEY_2:
    case KEY_3:
    case KEY_4:
    case KEY_5:
    case KEY_6:
    case KEY_7:
    case KEY_8:
    case KEY_9:
      gFInputCallback = tuneTo;
      APPS_run(APP_FINPUT);
      APPS_key(key, state);
      return true;
    case KEY_F:
      gChEd = *radio;
      if (RADIO_IsChMode()) {
        gChEd.meta.type = TYPE_CH;
      }
      APPS_run(APP_CH_CFG);
      return true;
    case KEY_STAR:
      APPS_run(APP_LOOT_LIST);
      return true;
    case KEY_SIDE1:
      gMonitorMode = !gMonitorMode;
      return true;
    case KEY_SIDE2:
      break;
    case KEY_EXIT:
      if (!APPS_exit()) {
        LOOT_Standby();
        RADIO_NextVFO();
      }
      return true;
    default:
      break;
    }
  }
  return false;
}

bool VFO1_key(KEY_Code_t key, Key_State_t state) {
  return VFO1_keyEx(key, state, gVfo1ProMode);
}

static void DrawRegs(void) {
  RegisterSpec rs = registerSpecs[menuIndex];

  if (rs.num == BK4819_REG_13) {
    snprintf(String, sizeof(String),
             (radio->gainIndex == AUTO_GAIN_INDEX) ? "auto" : "%+ddB",
             -gainTable[radio->gainIndex].gainDb + 33);
  } else if (rs.num == 0x73) {
    uint8_t afc = BK4819_GetAFC();
    snprintf(String, sizeof(String), afc ? "%u" : "off", afc);
  } else {
    snprintf(String, sizeof(String), "%u", BK4819_GetRegValue(rs));
  }

  PrintMedium(2, LCD_HEIGHT - 4, "%u. %s: %s", menuIndex, rs.name, String);

  if (registerActive) {
    FillRect(0, LCD_HEIGHT - 4 - 7, LCD_WIDTH, 9, C_INVERT);
  }
}

static void renderMiddleContent(uint8_t startY, uint8_t height, uint32_t f,
                                const char *name, bool isChMode, int16_t chNum,
                                bool isTx) {
  (void)isTx;
  const uint8_t THICK_LEFT = 6;
  const uint8_t BOX_PAD = 5;
  const uint8_t PAD_TOP = 10;
  const uint8_t LINE1_H = 6;
  const uint8_t GAP = 6;
  const uint8_t LINE2_DOWN = 4;
  uint8_t boxY = startY;
  uint8_t boxH = height;
  bool hasSignal = RADIO_IsSquelchOpen();

  if (hasSignal) {
    DrawRect(0, boxY, THICK_LEFT, boxH, C_FILL);
  } else {
    FillRect(0, boxY, THICK_LEFT, boxH, C_FILL);
  }
  DrawHLine(THICK_LEFT, boxY, LCD_WIDTH - THICK_LEFT, C_FILL);
  DrawHLine(THICK_LEFT, boxY + boxH - 1, LCD_WIDTH - THICK_LEFT, C_FILL);
  DrawVLine(LCD_WIDTH - 1, boxY, boxH, C_FILL);

  uint8_t innerX = THICK_LEFT + BOX_PAD;
  uint8_t y1 = boxY + PAD_TOP + 2;
  uint8_t y2 = y1 + LINE1_H + GAP + LINE2_DOWN;

  {
    uint16_t rssi = RADIO_GetRSSI();
    int dbm = Rssi2DBm(rssi);
    PrintSmallEx(LCD_WIDTH - 1, boxY + 6, POS_R, C_INVERT, "%d dBm", dbm);
  }

  if (isChMode) {
    uint16_t num = (chNum >= 0) ? (uint16_t)(chNum + 1) : 1;
    PrintMedium(innerX, y1, "%03u ", num);
    PrintMediumEx(innerX + 24, y1, POS_L, C_INVERT, "%s", name);
  } else {
    PrintMediumEx(innerX, y1, POS_L, C_INVERT, "VFO");
  }

  PrintBigDigits(innerX, y2, "%u.%05u", f / MHZ, f % MHZ);
}

static void renderProModeInfo(uint8_t y, const VFO *radio) {
  PrintSmall(34, 12, "RNG %+3u %+3u %+3u", RADIO_GetRSSI(), BK4819_GetNoise(),
             BK4819_GetGlitch());
  PrintSmallEx(LCD_WIDTH - 1, 12, POS_R, C_FILL, "%s%u",
               sqTypeNames[radio->squelch.type], radio->squelch.value);
  PrintSmallEx(LCD_WIDTH - 1, 18, POS_R, true, RADIO_GetBWName(radio));

  const uint32_t step = StepFrequencyTable[radio->step];
  PrintSmall(0, y - 7, "SNR %u", RADIO_GetSNR());
  PrintSmallEx(0, y, POS_L, C_FILL, "STP %d.%02d", step / 100, step % 100);

  DrawRegs();
}

void VFO1_render(void) {
  STATUSLINE_renderCurrentBand();

  uint32_t f = gTxState == TX_ON ? RADIO_GetTXF() : GetScreenF(radio->rxF);
  const char *mod = modulationTypeOptions[radio->modulation];

  const uint8_t MID_START = 9;
  const uint8_t MID_END = 54;
  const uint8_t MID_H = (uint8_t)((MID_END - MID_START) * 3 / 4) + 3;

  if (!gVfo1ProMode) {
    renderMiddleContent(MID_START, MID_H, f, radio->name, RADIO_IsChMode(),
                        radio->channel, gTxState == TX_ON);
  } else {
    if (gIsListening || gVfo1ProMode) {
      UI_RSSIBar(MID_START);
    }
    renderProModeInfo(40, radio);
  }

  const uint8_t BOTTOM_BAR_Y = 54;
  const uint8_t BOTTOM_BAR_H = LCD_HEIGHT - BOTTOM_BAR_Y;
  const uint8_t HALF_W = LCD_WIDTH / 2;
  FillRect(0, BOTTOM_BAR_Y, LCD_WIDTH, BOTTOM_BAR_H, C_FILL);
  DrawHLine(0, BOTTOM_BAR_Y, LCD_WIDTH, C_CLEAR);
  DrawVLine(HALF_W, BOTTOM_BAR_Y, BOTTOM_BAR_H, C_CLEAR);
  const uint8_t BTN_CENTER_Y = BOTTOM_BAR_Y + BOTTOM_BAR_H / 2 + 1 + 2;
  PrintMediumEx(HALF_W / 2, BTN_CENTER_Y, POS_C, C_CLEAR, "Menu");
  PrintMediumEx(HALF_W + HALF_W / 2, BTN_CENTER_Y, POS_C, C_CLEAR, "%s", mod);
}
