/**
 * @file PaletteBuddy.c
 * contains helper code related to custom character palettes
 */

#include "/sf33rd/Source/Game/debug/PaletteBuddy.h"
#include "sf33rd/Source/Game/engine/workuser.h"

// TODO: define brightening effect on statements that use MOD_LUM and TINT in
// palmod
u16 palPor[POR_PALETTE_SIZE];

// converts sprite palettes to portrait palettes using the lookup key
u16* convPalSprPor(u16* palSpr, s16 id) {
    if (id <= 1 && palSpr != NULL) {
        u16 character = My_char[id];
        // none of this should be changed by the palette
        palPor[0] = 0;
        // hud colours
        palPor[1] = 32784;
        palPor[2] = 62362;
        // only check for the hardcode for characters that use it
        if (character != 1 && character != 11) {
            for (int i = 3; i < POR_PALETTE_SIZE - 1; i++) {
                // override with pure black and white when specified
                if (hudPortraitLookup[character][i - 3] == -1) {
                    palPor[i] = 32768;
                } else if (hudPortraitLookup[character][i - 3] == -2) {
                    palPor[i] = 65535;
                } else {
                    palPor[i] = palSpr[hudPortraitLookup[character][i - 3]];
                }
            };
        } else {
            // don't check for overrides with characters known to not have any
            for (int i = 3; i < POR_PALETTE_SIZE - 1; i++) {
                palPor[i] = palSpr[hudPortraitLookup[character][i - 3]];
            };
        }

        // unused on everyone
        palPor[15] = 0;
        return palPor;
    } else {
        return NULL;
    }
}
