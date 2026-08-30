/**
 * @file palette_select.c
 * character select custom palette selection UI
 */

#include "SDL3/SDL_filesystem.h"
#include "SDL3/SDL_stdinc.h"
#include "\sf33rd\Source\Game\debug\PaletteBuddy.h"
#include "\sf33rd\Source\Game\ui\glyph_renderer.h"
#include "\sf33rd\Source\Game\ui\palette_select.h"
#include "sf33rd/Source/Game/debug/debug_config.h"
#include "sf33rd/Source/Game/debug/external_palettes.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/rendering/mtrans.h"
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>

// the names of all the palettes each player's character has
char** palettes[2];
int palLen[2];

u8 paletteSelectEnabled[2] = { 0, 0 };
u8 paletteSelectVisible[2] = { 0, 0 };
// the ui positions of the palette selectors for each player
GlyphPosition paletteSelectPosition[2] = { (GlyphPosition) { 40, 20 }, (GlyphPosition) { 270, 100 } };
// stores the index of the custom palette currently being looked at
int paletteIndex[2] = { 0, 0 };
// store the formatted palette displays for each player
char* formattedDisplay[2] = { NULL, NULL };

// move a player's palette selection a defined number of steps foreward or back (usually just 1 or -1)
void ScrollPaletteList(u8 id, s8 step) {
    if (palLen[id] == 0) {
        return;
    }

    // step == 0 means the scroll is being called to update the displayed palette
    if (step == 0) {
        SDL_asprintf(&formattedDisplay[id], "< %d: %s >", paletteIndex[id], palettes[id][paletteIndex[id]]);
        return;
    }

    paletteIndex[id] += step;
    if (paletteIndex[id] >= palLen[id]) {
        paletteIndex[id] = 0;
    } else if (paletteIndex[id] < 0) {
        paletteIndex[id] = (palLen[id] - 1);
    }

    SDL_strlcpy(
        debug_config.palette_override[id], palettes[id][paletteIndex[id]], sizeof(debug_config.palette_override[id])
    );

    SDL_asprintf(&formattedDisplay[id], "< %d: %s >", paletteIndex[id], palettes[id][paletteIndex[id]]);
}

// get a list of all of the palettes for a player's currently selected character
void GetCharacterPalettes(u8 id, u8 character) {
    // empty the player's palette list
    SDL_free(palettes[id]);
    palettes[id] = NULL;
    palLen[id] = 0;

    int paletteCount = 0;
    const char* charName = character_names[character];
    const char* base_path = SDL_GetBasePath();
    char* full_path = NULL;
    SDL_asprintf(&full_path, "%sassets\\%s\\", base_path, charName);

    char** tempPals = SDL_GlobDirectory(full_path, "*.act", 0, &paletteCount);
    if (paletteCount < 1) {
        SDL_asprintf(&formattedDisplay[id], "< NO PALETTES INSTALLED >");
        return;
    }
    palLen[id] = paletteCount;
    palettes[id] = tempPals;
    SDL_strlcpy(
        debug_config.palette_override[id], palettes[id][paletteIndex[id]], sizeof(debug_config.palette_override[id])
    );
    if (formattedDisplay[id] == NULL) {
        ScrollPaletteList(id, 0);
    }
}

// toggles the palette select
void TogglePaletteSelect(u8 id) {
    paletteSelectEnabled[id] ^= 1;
    paletteSelectVisible[id] = paletteSelectEnabled[id];
    if (paletteSelectEnabled[id] == 1) {
        GetCharacterPalettes(id, ID_of_Face[Cursor_Y[id]][Cursor_X[id]]);
        paletteIndex[id] = 0;
    } else {
        // reset the stored indexes upon closing the menu
        formattedDisplay[id] = NULL;
        paletteIndex[id] = 0;
        SDL_strlcpy(debug_config.palette_override[id], "", sizeof(debug_config.palette_override[id]));
    }
}

void PaletteSelectVisibility(u8 id, bool vis) {
    if (id > 1) {
        return;
    }
    paletteSelectVisible[id] = vis;
}

void ClearPaletteSelect() {
    ScrollPaletteList(0, (paletteIndex[0] * -1));
    ScrollPaletteList(1, (paletteIndex[1] * -1));
    for (int i = 0; i < 2; i++) {
        formattedDisplay[i] = NULL;
        paletteSelectEnabled[i] = 0;
    }
}

// renders the palette selection every frame, assuming it's enabled
void RenderPaletteSelect(u8 id) {
    if (paletteSelectVisible[id] && paletteSelectEnabled[id]) {
        GlyphRenderer_DrawString(formattedDisplay[id], paletteSelectPosition[id], GLYPH_COLOR_DEFAULT, PrioBase[2]);
    }
}
