/**
 * @file PaletteBuddy.c
 * contains helper code related to custom character palettes
 */

#include "/sf33rd/Source/Game/debug/PaletteBuddy.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/screen/sel_pl.h"
#include <SDL3/SDL.h>

u16 palPor[POR_PALETTE_SIZE];

// Swaps controlled player ID when sequential character select is enabled (i.e. training mode), and the first selection
// has been made.
// Utilized to let you select both character's palettes in singleplayer training mode.
s16 TrainingHandoffSlot(u8 pad_id) {
    // sequential select not on, no swap needed
    if (!Sel_PL_Is_Sequential()) {
        return pad_id;
    }
    // champion in training (ID making both decisions)
    const s16 lead = Sel_PL_Lead_ID();
    if (pad_id != lead) {
        // kill inputs from non-lead pad id
        return -1;
    }

    return (S_No[3] >= 2) ? (s16)(lead ^ 1) : lead;
}

// Converts sprite palettes to portrait palettes using the lookup key.
// TODO: define brightening effect on statements that use MOD_LUM and TINT in
// palmod.
u16* convPalSprPor(u16* palSpr, s16 id) {
    if (id <= 1 && palSpr != NULL) {
        u16 character = My_char[id];
        // the first 3 shouldn't be changed by the palette
        palPor[0] = 0;
        // hud colours
        palPor[1] = 32784;
        palPor[2] = 62362;
        // only check for the hardcoded colors on characters that use it (Ken/Alex)
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
