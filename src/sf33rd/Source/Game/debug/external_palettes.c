#include <SDL3/SDL.h>
#include <stdio.h>

#include "sf33rd/AcrSDK/common/plcommon.h"
#include "sf33rd/AcrSDK/ps2/flps2vram.h"
#include "sf33rd/Source/Common/PPGWork.h"
#include "sf33rd/Source/Game/debug/PaletteBuddy.h"
#include "sf33rd/Source/Game/debug/debug_config.h"
#include "sf33rd/Source/Game/debug/external_palettes.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/rendering/color3rd.h"

static u16 g_actPalette[ACT_PALETTE_SIZE]; // converted, ready for ColorRAM
static bool g_actLoaded = false;

// Converts RGB data from ACT file into ram format integer
u16 rgbToRamPalette(u8 r, u8 g, u8 b) {
    return (1 << 15) | ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3);
}

// Converts a .ACT file to the palette format 3S uses internally. Takes a palette name and character ID to locate it in
// /assets to not assign palettes to characters they weren't made for.
u16* ConvActToPal(const char* palName, char* character) {

    // get path from given file name
    const char* base_path = SDL_GetBasePath();
    char* full_path = NULL;
    SDL_asprintf(&full_path, "%sassets\\%s\\%s.act", base_path, character, palName);

    // open file, check for inconsistent file size
    SDL_IOStream* io = SDL_IOFromFile(full_path, "rb");
    if (io) {
        Sint64 len = SDL_GetIOSize(io);
        if (len >= 768) {
            Uint8* buf = SDL_malloc((size_t)len);
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
                printf("palette converted!\n");
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
    printf("emptied transparent palette colour...\n");
    printf("palette created!\n");

    return g_actPalette;
}

// Writes the converted palette into the HUD face-icon bank used by player `id`.
//
// The in-match health-bar face is drawn by scfont_sqput_face() (ui/sc_sub.c),
// which selects its palette with:
//     njSetPaletteBankNumG(0, (Player_Color[id] + My_char[id] * 13) & 0x3FFF)
// against the ppgScrListFace data list, whose .pal is &ppgScrPalFace. That
// amounts to using ppgScrPalFace.handle[bank] as the active palette texture.
// We lock that same handle and overwrite it, mirroring exactly what
// palUpdateGhostCP3() does for the character sprite palettes in color3rd.c.
static void OverrideFaceIconPalette(s16 id, const u16* pal) {
    // convert the 64-entry sprite palette into the 16-entry portrait format
    u16* por = convPalSprPor((u16*)pal, id);

    if (por == NULL) {
        printf("face palette conversion failed for player %d\n", id + 1);
        return;
    }

    const u16 bank = (u16)((Player_Color[id] + My_char[id] * 13) & 0x3FFF);

    if (ppgScrPalFace.be == 0 || ppgScrPalFace.handle == NULL) {
        return;
    }

    if (bank >= ppgScrPalFace.total) {
        printf("face bank %d out of range (total %d)\n", bank, ppgScrPalFace.total);
        return;
    }

    // lock the bank's GPU texture, overwrite with the portrait palette, unlock
    plContext bits;

    flLockPalette(NULL, ppgScrPalFace.handle[bank], &bits, 2);

    if (bits.ptr != NULL) {
        s32 copyBytes = bits.height * bits.pitch;

        if (copyBytes <= 0) {
            copyBytes = 0x20; // fall back to 16 entries if the context isn't populated
        }

        if (copyBytes > (s32)(POR_PALETTE_SIZE * sizeof(u16))) {
            copyBytes = POR_PALETTE_SIZE * sizeof(u16);
        }

        SDL_memcpy(bits.ptr, por, (size_t)copyBytes);
    }

    flUnlockPalette(ppgScrPalFace.handle[bank]);
}

// applies external .ACT palettes onto characters from the CSS, based on chosen debug settings. Currently only tied to
// debug commands.
void ApplyExternalPalette(s16 id) {
#if DEBUG
    const u16* pal = NULL;
    char* character = character_names[My_char[id]];
    if (id > 1 || (debug_config.palette_override[id][0] == '\0' && debug_config.palette_override_enabled[id] == 1)) {
        printf("No palette specified for player %d, ignoring...\n", id + 1);
        return;
    }

    if (debug_config.palette_override_enabled[id] == 1) {
        pal = ConvActToPal(debug_config.palette_override[id], character);
    } else {
        printf("custom palette disabled for player %d, skipping...\n", id);
        return;
    }

    if (pal == NULL) {
        printf("could not return formatted palette for player %d, ignoring...\n", id + 1);
        return;
    }

    // each character palette uses 2 different banks for facing left/right, which is why there are 2 fills to ColorRAM
    // in each case
    if (debug_config.palette_override_enabled[id] == 1) {
        for (int i = 0; i < 64; i++) {
            // writes to character sprite RAM
            ColorRAM[id * 16][i] = pal[i];
            ColorRAM[id * 16 + 8][i] = pal[i];
            // writes to face icon sprite RAM
            ColorRAM[id * 16 + 6][i] = pal[i];
            ColorRAM[id * 16 + 14][i] = pal[i];
        }
        // update char sprite banks
        palUpdateGhostCP3(id * 16, 1);
        palUpdateGhostCP3(id * 16 + 8, 1);

        palUpdateGhostCP3(id * 16 + 6, 1);
        palUpdateGhostCP3(id * 16 + 14, 1);

        // update the HUD face-icon palette (ppgScrPalFace bank) for this player
        OverrideFaceIconPalette(id, pal);
    }

#endif
}
