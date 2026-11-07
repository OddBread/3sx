#ifndef EXTERNAL_PALETTES_H
#define EXTERNAL_PALETTES_H

#include "types.h"

#include <stdbool.h>

#define ACT_PALETTE_SIZE 256

u16 rgbToRamPalette(u8 r, u8 g, u8 b);
bool getColorFromPalette(const char* palName, const char* character, const u8 index, u8 out[3]);
u16* ConvActToPal(const char* palName, const char* character);
void overridePaletteTest(const u16* palette);
u16 rgbToRamPalette(u8 r, u8 g, u8 b);
void ApplyExternalPalette(s16 id);
void RestoreFaceIconPalette(s16 id);
void RestoreFaceIconPalettes(void);

#endif
