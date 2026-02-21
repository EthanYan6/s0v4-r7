#include "generator.h"
#include "../driver/bk4819.h"
#include "../misc.h"
#include "../radio.h"
#include "../ui/graphics.h"
#include "apps.h"
#include "finput.h"

static uint16_t tone1Freq = 1000;
static uint8_t power = 26;
static bool paEnabled = false;
static uint8_t bkPower = 0;

static void setTone1Freq(uint32_t f) { tone1Freq = f / 100; }

static void calcPower() {
  paEnabled = power >= 26;

  if (power < 26) {
    bkPower = power * 255 / 26;
  } else {
    bkPower = 1 + (power - 26) * 254 / (255 - 26);
  }
}

static void updatePower(int8_t v) {
  if (v > 0 && power < 255) {
    power++;
  }
  if (v < 0 && power > 0) {
    power--;
  }
  calcPower();
}

void GENERATOR_init() { calcPower(); }
void GENERATOR_update() {}
bool GENERATOR_key(KEY_Code_t key, Key_State_t state) {
  const uint16_t M[] = {tone1Freq, 0, 0, 0};
  if (key == KEY_PTT) {
    RADIO_ToggleTXEX(state == KEY_PRESSED, RADIO_GetTXF(), power, bkPower);
    if (state == KEY_PRESSED && gTxState == TX_ON) {
      BK4819_PlaySequence(M);
    }

    return true;
  }
  // Simple keypress
  if (state == KEY_RELEASED) {
    switch (key) {
    case KEY_5:
      gFInputCallback = RADIO_TuneTo;
      APPS_run(APP_FINPUT);
      return true;
    case KEY_SIDE1:
      gFInputCallback = setTone1Freq;
      APPS_run(APP_FINPUT);
      return true;
    case KEY_EXIT:
      APPS_exit();
      return true;
    default:
      break;
    }
  }
  // up-down keys
  if (state == KEY_RELEASED || state == KEY_LONG_PRESSED_CONT) {
    switch (key) {
    case KEY_UP:
    case KEY_DOWN:
      RADIO_NextF(key == KEY_UP);
      return true;
    case KEY_2:
    case KEY_8:
      updatePower(key == KEY_2 ? 1 : -1);
      return true;
    default:
      break;
    }
  }
  return false;
}

void GENERATOR_render() {
  uint32_t txf = RADIO_GetTXF();
  PrintMediumEx(LCD_XCENTER, 15, POS_C, C_FILL, "%u.%05u", txf / MHZ,
                txf % MHZ);
  PrintMediumEx(LCD_XCENTER, 15 + 12, POS_C, C_FILL, "F: %uHz", tone1Freq);
  PrintMediumEx(LCD_XCENTER, 15 + 28, POS_C, C_FILL, "Pow: %u%s", power,
                (bkPower >= 0x91 && paEnabled) ? "!!!" : "");
}
