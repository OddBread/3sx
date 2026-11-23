#ifndef EXTERNAL_PALETTES_H
#define EXTERNAL_PALETTES_H

#include "types.h"

#include <stdbool.h>

#define ACT_PALETTE_SIZE 256

u16* ConvActToPal(const char* palName, const char* character);

void ApplyExternalPalette(s16 id);

void ApplyExternalFaceIcon(s16 id);
void ApplyExternalFaceIcons(void);

void RestoreFaceIconPalette(s16 id);
void RestoreFaceIconPalettes(void);

#endif
