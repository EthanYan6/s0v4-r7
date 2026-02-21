#ifndef DRIVER_KEYBOARD_H
#define DRIVER_KEYBOARD_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  KEY_0 = 0,
  KEY_1 = 1,
  KEY_2 = 2,
  KEY_3 = 3,
  KEY_4 = 4,
  KEY_5 = 5,
  KEY_6 = 6,
  KEY_7 = 7,
  KEY_8 = 8,
  KEY_9 = 9,
  KEY_MENU = 10,
  KEY_UP = 11,
  KEY_DOWN = 12,
  KEY_EXIT = 13,
  KEY_STAR = 14,
  KEY_F = 15,
  KEY_PTT = 21,
  KEY_SIDE2 = 22,
  KEY_SIDE1 = 23,
  KEY_INVALID = 255,
} KEY_Code_t;

typedef enum {
  KEY_RELEASED,
  KEY_PRESSED,
  KEY_LONG_PRESSED,
  KEY_LONG_PRESSED_CONT
} Key_State_t;

void KEYBOARD_Poll(void);
void KEYBOARD_CheckKeys();
void KEYBOARD_Init();

#endif
