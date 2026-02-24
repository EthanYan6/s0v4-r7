#ifndef STATUSLINE_H
#define STATUSLINE_H

#include <stdint.h>

void STATUSLINE_update();
void STATUSLINE_render();
void STATUSLINE_renderVfo1Row(uint8_t y);
/** 与上相同，但项间间隔为 gapPx；若 outXAfterBw 非 NULL 则写入最后一左项（bw）后的 x */
void STATUSLINE_renderVfo1RowEx(uint8_t y, uint8_t gapPx, uint8_t *outXAfterBw);
void STATUSLINE_SetText(const char *pattern, ...);
void STATUSLINE_SetTickerText(const char *pattern, ...);
void STATUSLINE_renderCurrentBand();

#endif /* end of include guard: STATUSLINE_H */
