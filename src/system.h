#ifndef SYS_H
#define SYS_H
#include "driver/keyboard.h"

extern uint32_t gAppUpdateInterval;

void SYS_Main(void *params);
void SYS_MsgKey(KEY_Code_t key, Key_State_t state);
void SYS_MsgNotify(const char *message, uint32_t ms);

#endif /* end of include guard: SYS_H */
