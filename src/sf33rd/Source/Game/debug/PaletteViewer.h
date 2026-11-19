#ifndef PALETTEVIEWER_H
#define PALETTEVIEWER_H

#define PALETTE_VIEWER_MTS 13 // Slot for matrix
#include "types.h"
struct ViewerPos {
    int x;
    int y;
};
bool ViewerPaletteFromAct(const char* file, const char* character, s16 mts_slot, u8 id);
void PaletteViewer_Draw(u8 id);
void PaletteViewer_Release(void);
void PaletteViewer_ReleaseSlot(u8 id);
#endif
