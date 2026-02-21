#include "appslist.h"
#include "../driver/uart.h"
#include "../helper/measurements.h"
#include "../helper/numnav.h"
#include "../misc.h"
#include "../ui/graphics.h"
#include "../ui/menu.h"
#include "../ui/statusline.h"
#include "apps.h"
#include "chlist.h"

static const uint8_t MENU_SIZE = ARRAY_SIZE(appsAvailableToRun);

static uint8_t menuIndex = 0;

static void getMenuItem(uint16_t i, uint16_t index, bool isCurrent) {
  const uint8_t y = MENU_Y + i * MENU_ITEM_H;

  if (isCurrent) {
    FillRect(0, y, LCD_WIDTH - 4, MENU_ITEM_H, C_FILL);
  }
  PrintMediumEx(8, y + 8, POS_L, C_INVERT, "%s",
                apps[appsAvailableToRun[index]].name);
}

void APPSLIST_render(void) {
  if (gIsNumNavInput) {
    STATUSLINE_SetText("Select: %s", gNumNavInput);
  } else {
    STATUSLINE_SetText(apps[APP_APPS_LIST].name);
  }

  UI_ShowMenuEx(getMenuItem, MENU_SIZE, menuIndex, 5);
}

static void setMenuIndexAndRun(uint16_t v) {
  menuIndex = v - 1;
  APPS_exit();
  APPS_runManual(appsAvailableToRun[menuIndex]);
}

void APPSLIST_init(void) {}

bool APPSLIST_key(KEY_Code_t key, Key_State_t state) {
  if (state == KEY_RELEASED) {
    if (!gIsNumNavInput && key <= KEY_9) {
      NUMNAV_Init(menuIndex + 1, 1, MENU_SIZE);
      gNumNavCallback = setMenuIndexAndRun;
    }
    if (gIsNumNavInput) {
      menuIndex = NUMNAV_Input(key) - 1;
      return true;
    }
  }

  AppType_t app = appsAvailableToRun[menuIndex];

  if (state == KEY_RELEASED || state == KEY_LONG_PRESSED_CONT) {
    switch (key) {
    case KEY_UP:
    case KEY_DOWN:
      menuIndex = IncDecU(menuIndex, 0, MENU_SIZE, key != KEY_UP);
      return true;
    case KEY_MENU:
      APPS_exit();
      if (app == APP_LOOT_LIST || app == APP_CH_LIST) {
        if (app == APP_CH_LIST) {
          gChListFilter = TYPE_FILTER_CH;
          gChSaveMode = false;
        }
        APPS_run(app);
      } else {
        APPS_runManual(app);
      }
      return true;
    case KEY_EXIT:
      APPS_exit();
      return true;
    default:
      break;
    }
  }
  return false;
}
