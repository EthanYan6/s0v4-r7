#include "vfo2.h"
#include "../dcs.h"
#include "../helper/bands.h"
#include "../helper/lootlist.h"
#include "../misc.h"
#include "../scheduler.h"
#include "../settings.h"
#include "../ui/components.h"
#include "../ui/graphics.h"
#include "../ui/spectrum.h"
#include "../ui/statusline.h"
#include "vfo1.h"

static void renderTxRxState(uint8_t i, uint8_t bl, bool isActive) {
  if (isActive && gTxState <= TX_ON) {
    FillRect(0, bl - 14, 28, 7, C_FILL);
    if (gTxState == TX_ON) {
      PrintMedium(0, bl, "TX");
      UI_TxBar(31);
    }
  }

  if (gIsListening && isActive) {
    PrintMedium(0, bl, "RX");
    UI_RSSIBar(31);
  }
}

static void renderFrequencyAndName(uint8_t bl, const VFO *vfo, uint32_t f) {

  const uint16_t fp1 = f / MHZ;
  const uint16_t fp2 = f / 100 % 1000;
  const uint8_t fp3 = f % 100;

  if (vfo->channel >= 0) {
    if (gSettings.chDisplayMode == CH_DISPLAY_MODE_F) {
      PrintBigDigitsEx(LCD_WIDTH - 19, bl, POS_R, C_FILL, "%4u.%03u", fp1, fp2);
    } else if (gSettings.chDisplayMode == CH_DISPLAY_MODE_N) {
      PrintMediumBoldEx(LCD_XCENTER, bl - 4, POS_C, C_FILL, vfo->name);
    } else {
      PrintMediumBoldEx(LCD_XCENTER, bl - 8, POS_C, C_FILL, vfo->name);
      PrintMediumEx(LCD_XCENTER, bl, POS_C, C_FILL, "%4u.%03u", fp1, fp2);
    }
    PrintSmallEx(14, bl - 9, POS_C, C_INVERT, "MR %03u", vfo->channel);
  } else {
    PrintBigDigitsEx(LCD_WIDTH - 19, bl, POS_R, C_FILL, "%4u.%03u", fp1, fp2);
    PrintMediumBoldEx(LCD_WIDTH, bl, POS_R, C_FILL, "%02u", fp3);
    PrintSmallEx(14, bl - 9, POS_C, C_INVERT, vfo->name);
  }
  PrintSmallEx(LCD_WIDTH - 1, bl - 9, POS_R, C_FILL,
               modulationTypeOptions[vfo->modulation]);
}

static void renderAdditionalInfo(uint8_t bl, const VFO *vfo,
                                 const Measurement *msm) {
  char str[64];

  if (vfo->radio == RADIO_BK4819) {
    if (msm->ct != 0xFF) {
      PrintSmallEx(0, bl + 6, POS_L, C_FILL, "C%u.%u",
                   CTCSS_Options[msm->ct] / 10, CTCSS_Options[msm->ct] % 10);
    } else if (msm->cd != 0xFF) {
      PrintSmallEx(0, bl + 6, POS_L, C_FILL, "D%03oN", DCS_Options[msm->cd]);
    }
  }

  sprintf(str, "%s", shortRadioNames[vfo->radio]);
  sprintf(str, "%s %+d", str, -gainTable[vfo->gainIndex].gainDb + 33);
  sprintf(str, "%s %s", str, RADIO_GetBWName(vfo));
  sprintf(str, "%s %s%u", str, sqTypeNames[vfo->squelch.type],
          vfo->squelch.value);
  if (vfo->allowTx) {
    sprintf(str, "%s %c", str, TX_POWER_NAMES[vfo->power][0]);
  }
  if (vfo->code.tx.type) {
    sprintf(str, "%s %s", str, TX_CODE_TYPES[vfo->code.tx.type]);
  }
  if (RADIO_GetTXFEx(vfo) != vfo->rxF) {
    sprintf(str, "%s %c", str,
            vfo->offsetDir ? TX_OFFSET_NAMES[vfo->offsetDir][0] : '*');
  }

  PrintSmallEx(LCD_XCENTER, bl + 6, POS_C, C_FILL, str);

  const uint32_t step = StepFrequencyTable[vfo->step];
  PrintSmallEx(LCD_WIDTH, bl + 6, POS_R, C_FILL, "%d.%02d", step / 100,
               step % 100);
}

static void render2VFOPart(uint8_t i) {
  const uint8_t BASE = 21;
  const uint8_t bl = BASE + 34 * i;

  const VFO *vfo = &gVFO[i];
  const Measurement *msm = &gLoot[i];
  const bool isActive = gSettings.activeVFO == i;

  uint32_t f =
      gTxState == TX_ON && isActive ? RADIO_GetTXF() : GetScreenF(vfo->rxF);

  renderTxRxState(i, bl, isActive);

  if (gTxState && gTxState != TX_ON && isActive) {
    PrintMediumBoldEx(LCD_XCENTER, bl - 8, POS_C, C_FILL, "%s",
                      TX_STATE_NAMES[gTxState]);
    PrintSmallEx(LCD_XCENTER, bl - 8 + 6, POS_C, C_FILL, "%u", RADIO_GetTXF());
  } else {
    renderFrequencyAndName(bl, vfo, f);
  }

  renderAdditionalInfo(bl, vfo, msm);
}

void VFO2_init(void) { VFO1_init(); }

bool VFO2_key(KEY_Code_t key, Key_State_t state) {
  uint8_t g = gCurrentBand.gainIndex;

  if (VFO1_keyEx(key, state, false)) {
    return true;
  }

  // long held
  if (state == KEY_LONG_PRESSED) {
    switch (key) {
    case KEY_2:
      LOOT_Standby();
      RADIO_NextVFO();
      return true;
    default:
      break;
    }
  }

  return false;
}

void VFO2_update(void) { VFO1_update(); }

void VFO2_render(void) {
  STATUSLINE_renderCurrentBand();
  SPECTRUM_Y = 6 + 35 * (1 - gSettings.activeVFO);
  SPECTRUM_H = 22;

  if (gMonitorMode) {
    render2VFOPart(gSettings.activeVFO);
    SP_RenderGraph();
    PrintSmallEx(LCD_WIDTH, SPECTRUM_Y + 6, POS_R, C_FILL, "BK SNR %u",
                 BK4819_GetSNR());
  } else {
    render2VFOPart(0);
    render2VFOPart(1);
  }
}
