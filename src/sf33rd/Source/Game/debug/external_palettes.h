#ifndef EXTERNAL_PALETTES_H
#define EXTERNAL_PALETTES_H

#include "types.h"

#include <stdbool.h>

#define ACT_PALETTE_SIZE 256

u16 rgbToRamPalette(u8 r, u8 g, u8 b);
u16* ConvActToPal(const char* palName, char* character);
void overridePaletteTest(const u16* palette);
u16 rgbToRamPalette(u8 r, u8 g, u8 b);
void ApplyExternalPalette(s16 id);

#endif
