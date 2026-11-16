/**
 * @file palette_select.c
 * character select custom palette selection UI
 */

#include "/sf33rd/Source/Game/ui/palette_select.h"
#include "/sf33rd/Source/Game/debug/PaletteBuddy.h"
#include "/sf33rd/Source/Game/ui/glyph_renderer.h"
#include "SDL3/SDL_filesystem.h"
#include "SDL3/SDL_stdinc.h"
#include "core/renderer.h"
#include "sf33rd/Source/Game/debug/debug_config.h"
#include "sf33rd/Source/Game/debug/external_palettes.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/rendering/mtrans.h"
#include <SDL3/SDL.h>

#if DEBUG
#include "sf33rd/Source/Game/debug/PaletteViewer.h"
#endif
// max palette name size before truncation occurs
#define PALNAMEMAX = 16;

// the filenames of all available palettes
char** palettes[2];
// the size of the palettes list for each player
int palLen[2];
// menu state handling
u8 paletteSelectEnabled[2] = { 0, 0 };
u8 paletteSelectVisible[2] = { 0, 0 };
// the ui positions of the palette selectors for each player
GlyphPosition paletteSelectPosition[2] = { (GlyphPosition) { 40, 20 }, (GlyphPosition) { 270, 100 } };
// stores the index and display name of the custom palette currently being looked at
int paletteIndex[2] = { 0, 0 };
char paletteName[2][32];

// TODO: move all storage via debug_config to a different location

// Frees the player's palette list to avoid disagreements between palLen and palettes.
static void ReleaseCharacterPalettes(u8 id) {
    if (id > 1) {
        return;
    }

    SDL_free(palettes[id]);
    palettes[id] = NULL;
    palLen[id] = 0;
}

//  Get a list of all of the palettes for a player's currently selected character
void GetCharacterPalettes(u8 id, u8 character) {
    // empty the player's palette list
    ReleaseCharacterPalettes(id);

    int paletteCount = 0;
    const char* charName = character_names[character];
    const char* base_path = SDL_GetBasePath();
    char* full_path = NULL;
    SDL_asprintf(&full_path, "%sassets\\%s\\", base_path, charName);

    char** tempPals = SDL_GlobDirectory(full_path, "*.act", 0, &paletteCount);
    if (tempPals == NULL || paletteCount < 1) {
        return;
    }
    palLen[id] = paletteCount;
    palettes[id] = tempPals;
    SDL_strlcpy(
        debug_config.palette_override[id], palettes[id][paletteIndex[id]], sizeof(debug_config.palette_override[id])
    );
}

static void update_palette_name(u8 id) {
    if (id > 1)
        return;
    if (palLen[id] < 1 || palettes[id] == NULL || palettes[id][paletteIndex[id]] == NULL)
        return;

    SDL_strlcpy(paletteName[id], palettes[id][paletteIndex[id]], sizeof(paletteName[id]));
    const size_t len = SDL_strlen(paletteName[id]);
    if (len > 4 && SDL_strcasecmp(&paletteName[id][len - 4], ".act") == 0) {
        paletteName[id][len - 4] = '\0';
    }
}

// Update the preview colors for a specified player
// TODO: replace this with the sprite viewer code
static void UpdatePreviewColors(u8 id, u8 character) {
    if (id > 1 || id < 0 || character > 19) {
        return;
    }
#if DEBUG
    ViewerPaletteFromAct(debug_config.palette_override[id], character_names[character], PALETTE_VIEWER_MTS);
#endif
}

// Renders the palette selection every frame, assuming it's enabled
void RenderPaletteSelect(u8 id) {
    if (id > 1) {
        return;
    }
    if (!paletteSelectVisible[id]) {
        paletteSelectEnabled[id] = 0;
    }

    if (paletteSelectVisible[id] && paletteSelectEnabled[id]) {

        glyph_renderer_state.position = paletteSelectPosition[id];
        glyph_renderer_state.color = GLYPH_COLOR_DEFAULT;
        glyph_renderer_state.z = PrioBase[2];
        if (palLen[id] < 1 || palettes[id] == NULL) {
            glyph_renderer_state.color = GLYPH_COLOR_HEAVY;
            GlyphRenderer_DrawString("NO PALETTES FOUND");
        } else {
#if DEBUG
            PaletteViewer_Draw(id);
#endif
            GlyphRenderer_DrawString("%d: %s", paletteIndex[id], paletteName[id]);
        }
    }
}

// Move a player's palette selection a defined number of steps foreword or back (usually just 1 or -1)
// TODO: I accidentally remade modulo from first principles in this function and im too lazy to fix it right now
void ScrollPaletteList(u8 id, s8 step) {
    if (palLen[id] < 1 || id > 1 || palettes[id] == NULL) {
        return;
    }

    // step == 0 means the scroll is being called to update the displayed palette
    if (step == 0) {
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
    update_palette_name(id);
    UpdatePreviewColors(id, ID_of_Face[Cursor_Y[id]][Cursor_X[id]]);
}

// Toggles the palette select
void TogglePaletteSelect(u8 id) {
    if (id > 1) {
        return;
    }
    paletteSelectEnabled[id] ^= 1;
    paletteSelectVisible[id] = paletteSelectEnabled[id];
    if (paletteSelectEnabled[id] == 1) {
        GetCharacterPalettes(id, ID_of_Face[Cursor_Y[id]][Cursor_X[id]]);
        UpdatePreviewColors(id, ID_of_Face[Cursor_Y[id]][Cursor_X[id]]);
        paletteIndex[id] = 0;
        update_palette_name(id);
    } else {
        // reset the stored indexes upon closing the menu
        ReleaseCharacterPalettes(id);
        paletteIndex[id] = 0;
        SDL_zero(paletteName[id]);
        SDL_strlcpy(debug_config.palette_override[id], "", sizeof(debug_config.palette_override[id]));
#if DEBUG
        // give the sprite space back to the characters
        PaletteViewer_Release();
#endif
    }
}

void PaletteSelectVisibility(u8 id, bool vis) {
    if (id > 1) {
        return;
    }
    paletteSelectVisible[id] = vis;
    if (!vis) {
#if DEBUG
        PaletteViewer_Release(); // confirmed / hidden
#endif
    }
}

int GetPalCount(u8 id) {
    if (id <= 1 && id >= 0) {
        return palLen[id];
    } else {
        return -1;
    }
}
// Clears all palette select data for both players
void ClearPaletteSelect() {
    for (int i = 0; i < 2; i++) {
        ReleaseCharacterPalettes(i);
        SDL_zero(paletteName[i]);
        paletteIndex[i] = 0;
        paletteSelectEnabled[i] = 0;
        paletteSelectVisible[i] = 0;
        SDL_strlcpy(debug_config.palette_override[i], "", sizeof(debug_config.palette_override[i]));
    }
#if DEBUG
    PaletteViewer_Release();
#endif
}
