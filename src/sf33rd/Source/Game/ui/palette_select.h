#ifndef PALETTE_SELECT_H
#define PALETTE_SELECT_H
#include <stdbool.h>
#include <types.h>


#define PALNAMEMAX = 100

void RenderPaletteSelect(u8 id);
void ClearPaletteSelect();
void PaletteSelectUpdate(u8 id);
void TogglePaletteSelect(u8 id);
void ScrollPaletteList(u8 id, s8 step);
void PaletteSelectVisibility(u8 id, bool vis);
#endif
