#include "fc.h"
#include "../dcs.h"
#include "../driver/system.h"
#include "../radio.h"
#include "../scheduler.h"
#include "../settings.h"
#include "../ui/components.h"
#include "../ui/graphics.h"
#include "../ui/statusline.h"
#include "apps.h"
#include "vfo1.h"

static const char *FILTER_NAMES[] = {
    [FILTER_OFF] = "ALL",
    [FILTER_VHF] = "VHF",
    [FILTER_UHF] = "UHF",
};

static bool bandAutoSwitch = false;
static FreqScanTime T = F_SC_T_0_2s;
static uint32_t scanF = 0;
static Filter filter = FILTER_UHF;
static uint32_t bound;
static uint32_t fcTimeMs;

static const uint32_t STEP = 100;

static uint32_t lastSwitch = 0;

static bool scanning = false;

static void stopScan() {
  BK4819_StopScan();
  BK4819_RX_TurnOn();
}

static void gotF(uint32_t f) {
  stopScan();
  scanning = false;
  RADIO_TuneTo(RoundToStep(f, STEP));
  RADIO_ToggleRX(true);
  SYS_DelayMs(200);
  gRedrawScreen = true;
}

static void switchBand() {
  scanF = 0;
  BK4819_SelectFilterEx(filter);
  FC_init();
}

static void startScan() {
  T = gSettings.fcTime;
  fcTimeMs = 200 << T;
  BK4819_StopScan();
  BK4819_EnableFrequencyScanEx(T);
  scanning = true;
}

void FC_init() {
  RADIO_LoadCurrentVFO();
  gMonitorMode = false;

  // RADIO_ToggleRX(false);
  startScan();
  bound = SETTINGS_GetFilterBound();
}

void FC_deinit() { stopScan(); }

void FC_update() {
  gRedrawScreen = true;
  if (gIsListening) {
    vTaskDelay(pdMS_TO_TICKS(60));
    RADIO_CheckAndListen();
    return;
  }

  if (bandAutoSwitch &&
      (Now() - lastSwitch >= (uint32_t)(fcTimeMs * 2) + 200)) {
    switch (filter) {
    case FILTER_UHF:
      filter = FILTER_VHF;
      break;
    case FILTER_VHF:
    default:
      filter = FILTER_UHF;
      break;
    }
    switchBand();
    lastSwitch = Now();
    return;
  }

  uint32_t f = 0;
  if (scanning && !BK4819_GetFrequencyScanResult(&f)) {
    return;
  }

  startScan();

  if (!f) {
    return;
  }

  if (f >= 8800000 && f < 10800000) {
    return;
  }

  if ((filter == FILTER_VHF && f >= bound) ||
      (filter == FILTER_UHF && f < bound)) {
    return;
  }

  Loot *loot = LOOT_Get(f);
  if (loot && (loot->blacklist || loot->whitelist)) {
    return;
  }

  if (DeltaF(f, scanF) < STEP) {
    gotF(f);
  }
  scanF = f;
  vTaskDelay(pdMS_TO_TICKS(200));
}

bool FC_key(KEY_Code_t key, Key_State_t state) {
  if (state == KEY_RELEASED) {
    switch (key) {
    case KEY_UP:
    case KEY_DOWN:
      gSettings.fcTime = IncDecI(gSettings.fcTime, 0, 3 + 1, key == KEY_UP);
      SETTINGS_DelayedSave();
      FC_init();
      break;
    case KEY_3:
    case KEY_9:
      radio->squelch.value = IncDecU(radio->squelch.value, 0, 11, key == KEY_3);
      RADIO_Setup();
      break;
    case KEY_STAR:
      APPS_run(APP_LOOT_LIST);
      return true;
    case KEY_F:
      filter = IncDecU(filter, 0, 3, true);
      switchBand();
      return true;
    case KEY_0:
      bandAutoSwitch = !bandAutoSwitch;
      return true;
    case KEY_PTT:
      gVfo1ProMode = true;
      APPS_run(APP_VFO1);
      return true;
    default:
      break;
    }
  }
  return false;
}

void FC_render() {
  PrintMediumEx(0, 16, POS_L, C_FILL, "%s %ums SQ %u %s", FILTER_NAMES[filter],
                fcTimeMs, radio->squelch.value, bandAutoSwitch ? "[A]" : "");
  UI_BigFrequency(40, scanF);

  if (gLastActiveLoot) {
    PrintMediumEx(LCD_WIDTH, 40 + 8, POS_R, C_FILL, "%u.%05u",
                  gLastActiveLoot->f / MHZ, gLastActiveLoot->f % MHZ);
    if (gLastActiveLoot->ct != 0xFF) {
      PrintSmallEx(LCD_WIDTH, 40 + 8 + 6, POS_R, C_FILL, "CT:%u.%uHz",
                   CTCSS_Options[gLastActiveLoot->ct] / 10,
                   CTCSS_Options[gLastActiveLoot->ct] % 10);
    } else if (gLastActiveLoot->cd != 0xFF) {
      PrintSmallEx(LCD_WIDTH, 40 + 8 + 6, POS_R, C_FILL, "DCS:D%03oN",
                   DCS_Options[gLastActiveLoot->cd]);
    }
  }
}
