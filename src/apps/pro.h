#ifndef PRO_APP_H
#define PRO_APP_H

#include "../driver/keyboard.h"

void PRO_init(void);
void PRO_update(void);
void PRO_render(void);
bool PRO_key(KEY_Code_t key, Key_State_t state);

#endif
