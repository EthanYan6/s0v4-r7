#ifndef STATUSLINE_H
#define STATUSLINE_H

#include <stdint.h>

void STATUSLINE_update();
void STATUSLINE_render();
void STATUSLINE_renderVfo1Row(uint8_t y);
void STATUSLINE_SetText(const char *pattern, ...);
void STATUSLINE_SetTickerText(const char *pattern, ...);
void STATUSLINE_renderCurrentBand();

#endif /* end of include guard: STATUSLINE_H */
