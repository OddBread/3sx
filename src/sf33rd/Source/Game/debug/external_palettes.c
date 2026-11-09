#include "sf33rd/Source/Game/debug/external_palettes.h"
#include "SDL3/SDL_iostream.h"
#include "SDL3/SDL_stdinc.h"
#include "\sf33rd\AcrSDK\ps2\foundaps2.h"
#include "sf33rd/AcrSDK/common/plcommon.h"
#include "sf33rd/AcrSDK/ps2/flps2vram.h"
#include "sf33rd/Source/Common/PPGWork.h"
#include "sf33rd/Source/Game/debug/PaletteBuddy.h"
#include "sf33rd/Source/Game/debug/debug_config.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/rendering/color3rd.h"
#include <SDL3/SDL.h>
#include <stdio.h>


static u16 g_actPalette[ACT_PALETTE_SIZE]; // converted, ready for ColorRAM
static bool g_actLoaded = false;

// backup of the original HUD face-palette colors so the override can be undone later.
static u16 g_facePalBackup[2][POR_PALETTE_SIZE];
// the memory bank the player's face palette backup is stored at
// -1 when nothing has been snapshotted for that player yet.
static s32 g_facePalBackedUpBank[2] = { -1, -1 };

// returns the HUD face-icon palette bank used by player `id`, or -1 if unavailable.
static s32 GetFaceIconBank(s16 id) {
    if (id > 1 || ppgScrPalFace.be == 0 || ppgScrPalFace.handle == NULL)
        return -1;

    const u16 bank = (u16)((Player_Color[id] + My_char[id] * 13) & 0x3FFF);
    if (bank >= ppgScrPalFace.total)
        return -1;

    return (s32)bank;
}

// Converts RGB data from ACT file into ram format integer
u16 rgbToRamPalette(u8 r, u8 g, u8 b) {
    return (1 << 15) | ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3);
}

// Gets the path of a .ACT file, given the name and what character the palette is for.
static char* getActPath(const char* palName, const char* character) {
    const char* base_path = SDL_GetBasePath();
    char* full_path = NULL;
    SDL_asprintf(&full_path, "%sassets\\%s\\%s", base_path, character, palName);
    return full_path;
}

// Converts a .ACT file to the palette format 3S uses internally. Takes a palette name and character ID to locate it in
// /assets to not assign palettes to characters they weren't made for.
u16* ConvActToPal(const char* palName, const char* character) {
    char* full_path = getActPath(palName, character);
    SDL_IOStream* io = SDL_IOFromFile(full_path, "rb");
    if (io) {
        Sint64 len = SDL_GetIOSize(io);
        if (len >= 768) {
            u8* buf = SDL_malloc((size_t)len);
            if (buf && SDL_ReadIO(io, buf, (size_t)len) == (size_t)len) {
                int count = 64;
                if (len >= 772) {
                    int n = (buf[768] << 24) | (buf[769] << 16) | (buf[770] << 8) | buf[771];
                    if (n > 0 && n <= count)
                        count = n;
                }
                // convert each RGB triplet straight into game RAM format
                for (int i = 0; i < count; i++) {
                    // printf("%d: reading rgb colours %d %d %d", i, buf[i * 3], buf[i * 3 + 1], buf[i * 3 + 2]);
                    g_actPalette[i] = rgbToRamPalette(buf[i * 3], buf[i * 3 + 1], buf[i * 3 + 2]);
                }
            }
            SDL_free(buf);
        }
        SDL_CloseIO(io);
    } else {
        printf(" palette file not found: %s\n", full_path);
        SDL_free(full_path);
        return NULL;
    }
    SDL_free(full_path);
    g_actPalette[0] = 0;
    return g_actPalette;
}

// grabs a singular RGB triplet from a .ACT file.
// Because the RAM palette colours cannot be cleanly converted back into RGB values, the values have to be found again
// inside the file.
bool getColorFromPalette(const char* palName, const char* character, const u8 index, u8 out[3]) {
    if (palName == NULL || character == NULL) {
        return false;
    }
    char* full_path = getActPath(palName, character);
    if (full_path == NULL) {
        return false;
    }
    SDL_IOStream* io = SDL_IOFromFile(full_path, "rb");
    if (io == NULL) {
        SDL_free(full_path);
        return false;
    }
    const s64 len = SDL_GetIOSize(io);
    const s64 offset = (s64)index * 3;
    bool found = false;

    if (len >= offset + 3 && SDL_SeekIO(io, offset, SDL_IO_SEEK_SET) >= 0) {
        found = SDL_ReadIO(io, out, 3) == 3;
    }

    SDL_CloseIO(io);
    SDL_free(full_path);

    if (!found) {
        printf("ERROR: bytes for index %d of palette cannot be found\n", index);
        return false;
    }
    return found;
}

// Writes the converted palette into the HUD face-icon bank used by player `id`.
static void OverrideFaceIconPalette(s16 id, const u16* pal) {
    // convert the character palette to a portrait palette
    u16* por = convPalSprPor((u16*)pal, id);
    if (por == NULL)
        return;

    // the memory bank the palette is located at. multiplied by 13 for the 13 default palettes
    const u16 bank = (u16)((Player_Color[id] + My_char[id] * 13) & 0x3FFF);
    if (ppgScrPalFace.be == 0 || ppgScrPalFace.handle == NULL)
        return;
    if (bank >= ppgScrPalFace.total)
        return;
    // the location of the GPU texture
    const u32 handle = ppgScrPalFace.handle[bank];
    if (handle == 0 || handle > FL_PALETTE_MAX)
        return; // avoid flPalette[handle-1] with handle==0

    plContext bits = { 0 };
    // lock the gpu texture, write to it with SDL_memcpy, then unlock the gpu texture
    // flLockPalette writes a pointer to handle (where the palette is stored) to bits
    if (flLockPalette(NULL, handle, &bits, 2) == 1 && bits.ptr != NULL) {
        if (g_facePalBackedUpBank[id] != bank) {
            // backup the old bank
            g_facePalBackedUpBank[id] = bank;
            SDL_memcpy(g_facePalBackup[id], bits.ptr, sizeof(u16) * POR_PALETTE_SIZE);
        }

        s32 copyBytes = bits.height * bits.pitch;
        // clamp the copied bytes to the size of the portrait
        if (copyBytes <= 0)
            copyBytes = (s32)(POR_PALETTE_SIZE * sizeof(u16));
        if (copyBytes > (s32)(POR_PALETTE_SIZE * sizeof(u16)))
            copyBytes = (s32)(POR_PALETTE_SIZE * sizeof(u16));
        // copy the portrait colours into the address stored in bits
        SDL_memcpy(bits.ptr, por, (size_t)copyBytes);
        flUnlockPalette(handle);
    }
}

// Applies external .ACT palettes onto characters from the CSS, based on chosen debug settings. Currently only tied
// to debug commands.
// TODO:make this no longer debug-only once palette overrides moved out of debugconfig
void ApplyExternalPalette(s16 id) {
#if DEBUG
    if (id < 0 || id > 1) {
        return;
    }

    if (debug_config.palette_override_enabled[id] != 1) {
        RestoreFaceIconPalette(id);
        return;
    }

    const u16 character_id = (u16)My_char[id];
    const char* character =
        (size_t)character_id < SDL_arraysize(character_names) ? character_names[character_id] : NULL;
    if (character == NULL || debug_config.palette_override[id][0] == '\0') {
        printf("No palette specified for player %d, ignoring...\n", id + 1);
        return;
    }
    u16* pal = ConvActToPal(debug_config.palette_override[id], character);
    if (pal == NULL) {
        printf("could not return formatted palette for player %d, ignoring...\n", id + 1);
        return;
    }
    // TEST TEST TEST
    u8 singleColor[3];
    getColorFromPalette(debug_config.palette_override[id], character, 2, singleColor);
    printf("found colours: ");
    for (int i = 0; i < 3; i++) {
        printf("%d, ", singleColor[i]);
    }
    printf(".\n");
    // change the colour of the character in the RAM, based on the given palette
    for (int i = 0; i < 64; i++) {
        ColorRAM[id * 16][i] = pal[i];
        ColorRAM[id * 16 + 8][i] = pal[i];
    }

    // update the character palette
    palUpdateGhostCP3(id * 16, 1);
    palUpdateGhostCP3(id * 16 + 8, 1);
    OverrideFaceIconPalette(id, pal);
#endif
}
// Puts the original HUD face-icon palette back for player `id`.
void RestoreFaceIconPalette(s16 id) {
    if (id > 1 || g_facePalBackedUpBank[id] == -1)
        return;

    const s32 bank = GetFaceIconBank(id);

    // if the current bank changed since the snapshot, the backup is stale
    if (bank != g_facePalBackedUpBank[id]) {
        g_facePalBackedUpBank[id] = -1;
        return;
    }

    g_facePalBackedUpBank[id] = -1;

    const u32 handle = ppgScrPalFace.handle[bank];
    if (handle == 0 || handle > FL_PALETTE_MAX)
        return;

    plContext bits = { 0 };
    if (flLockPalette(NULL, handle, &bits, 2) == 1 && bits.ptr != NULL) {
        SDL_memcpy(bits.ptr, g_facePalBackup[id], sizeof(u16) * POR_PALETTE_SIZE);
        flUnlockPalette(handle);
    }
}

// Restores both players' HUD face-icon palettes. Call this when a match ends.
void RestoreFaceIconPalettes(void) {
    RestoreFaceIconPalette(0);
    RestoreFaceIconPalette(1);
}
