#ifndef MEMVIEW_H
#define MEMVIEW_H

#include "../driver/keyboard.h"

void MEMVIEW_Init();
void MEMVIEW_Render();
bool MEMVIEW_key(KEY_Code_t k, Key_State_t st);

#endif /* end of include guard: MEMVIEW_H */
