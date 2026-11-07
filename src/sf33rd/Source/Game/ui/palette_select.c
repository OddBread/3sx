/**
 * @file palette_select.c
 * character select custom palette selection UI
 */

#include "SDL3/SDL_filesystem.h"
#include "SDL3/SDL_stdinc.h"
#include "\sf33rd\Source\Game\debug\PaletteBuddy.h"
#include "\sf33rd\Source\Game\ui\glyph_renderer.h"
#include "\sf33rd\Source\Game\ui\palette_select.h"
#include "core/renderer.h"
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
// stores the colors to use for the custom palette previews
u32 palPrevCol[2][3];

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

// Update the preview colors for a specified player
static void UpdatePreviewColors(u8 id, u8 character) {
    if (id > 1 || id < 0 || character > 19) {
        return;
    }
    for (int i = 0; i < 3; i++) {
        u8 rgbBuf[3];
        getColorFromPalette(
            debug_config.palette_override[id], character_names[character], selectPreviewLookup[character][i], rgbBuf
        );
        unsigned int argb =
            0xFF000000u | ((unsigned int)rgbBuf[0] << 16) | ((unsigned int)rgbBuf[1] << 8) | (unsigned int)rgbBuf[2];
        palPrevCol[id][i] = argb;
    }
}

// Draws a rect with the specified dimensions and colours
static void DrawPreviewRect(float x, float y, float w, float h, unsigned int argb, f32 z) {
    Quad quad;
    quad.v[0] = (Vec3) { x, y, z };         // top-left
    quad.v[1] = (Vec3) { x + w, y, z };     // top-right
    quad.v[2] = (Vec3) { x, y + h, z };     // bottom-left
    quad.v[3] = (Vec3) { x + w, y + h, z }; // bottom-right
    Renderer_DrawSolidQuad(&quad, argb);
}

// Draws a filled swatch with a solid outline (outline is `thickness` px on every side).
static void DrawPreviewSwatch(
    float x, float y, float w, float h, unsigned int fillArgb, unsigned int outlineArgb, float thickness, f32 z
) {
    // outline
    DrawPreviewRect(x - thickness, y - thickness, w + thickness * 2.0f, h + thickness * 2.0f, outlineArgb, z);
    // fill
    DrawPreviewRect(x, y, w, h, fillArgb, z);
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
            for (int i = 0; i < 3; i++) {
                DrawPreviewSwatch(
                    (paletteSelectPosition[id].x) + (PALPREVSIZE * i) + (PALPREVSPACING * i) + 10,
                    (paletteSelectPosition[id].y) + 10,
                    PALPREVSIZE,
                    (float)PALPREVSIZE / 1.7,
                    palPrevCol[id][i],
                    0xFF212121u,
                    1.0f,
                    PrioBase[2]
                );
            }
            GlyphRenderer_DrawString("%d: %s", paletteIndex[id], palettes[id][paletteIndex[id]]);
        }
    }
}

// Move a player's palette selection a defined number of steps foreword or back (usually just 1 or -1)
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
    } else {
        // reset the stored indexes upon closing the menu
        ReleaseCharacterPalettes(id);
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

// Clears all palette select data for both players
void ClearPaletteSelect() {
    for (int i = 0; i < 2; i++) {
        ReleaseCharacterPalettes(i);
        paletteIndex[i] = 0;
        paletteSelectEnabled[i] = 0;
        paletteSelectVisible[i] = 0;
        SDL_strlcpy(debug_config.palette_override[i], "", sizeof(debug_config.palette_override[i]));
    }
}
