#include "sf33rd/Source/Game/debug/PaletteViewer.h"

// #if DEBUG

#include "sf33rd/Source/Game/debug/debug_config.h"
#include "sf33rd/Source/Game/debug/external_palettes.h"
#include "sf33rd/Source/Game/engine/charid.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/rendering/aboutspr.h"
#include "sf33rd/Source/Game/rendering/chren3rd.h"
#include "sf33rd/Source/Game/rendering/color3rd.h"
#include "sf33rd/Source/Game/rendering/dc_ghost.h"
#include "sf33rd/Source/Game/rendering/texcash.h"
#include "sf33rd/Source/Game/rendering/texgroup.h"
#include "sf33rd/Source/Game/rendering/texgroup_data.h"
#include "sf33rd/Source/Game/system/ramcnt.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "structs.h"

#include <SDL3/SDL.h>
// Colour row the viewer's palette should occupy.

#define PALETTE_VIEWER_GROUPS 20 // groups 1..20 == Gill..Remy
#define PALETTE_VIEWER_Z 0       // must stay < PRIO_BASE_SIZE (128)

// the game's character struct, which stores various object properties
static WORK viewer_work[2];
static s16 viewer_colcd[2] = { 0, 0 };
// Array of pointers to important character data sections
static CharInitData viewer_char_data[2];
static s16 viewer_data_key[2] = { -1, -1 };

// states for controlling
static int loaded_grp[2] = { -1, -1 };
static bool anim_ready[2] = { false, false };
static struct ViewerPos viewer_positions[2] = { { 84, 100 }, { 310, 30 } };
static int viewer_color_rows[2] = { 100, 164 };

// Get a palette from a .ACT file and load it into the color row the preview uses
// TODO: make this trigger on visibility change instead of just the swapping palettes call
bool ViewerPaletteFromAct(const char* file, const char* character, s16 mts_slot, u8 id) {
    // ConvActToPal returns a pointer to a *static* buffer, so copy it immediately.
    u16* pal = ConvActToPal(file, character);
    if (pal == NULL) {
        return false;
    }
    // put the palette in
    SDL_memcpy(ColorRAM[viewer_color_rows[id]], pal, 64 * sizeof(u16));
    if (mts[mts_slot].texList.pal == palGetChunkGhostCP3()) {
        // CP3 bank index == ColorRAM row
        palUpdateGhostCP3(viewer_color_rows[id], 1);
        viewer_colcd[id] = viewer_color_rows[id];
    }
    return true;
}
// Sets up the screen space the previews exist in.
// BgMATRIX slot 0 is never used, so we have a clear spot for screen-space sprites. Thanks Capcom!
static void setup_screen_matrix(void) {
    static bool ready = false;
    // the matrix only has to be set up once
    if (ready)
        return;
    njUnitMatrix(NULL);
    njTranslate(NULL, 0, 224, 0);
    njScale(NULL, 1, -1, 1);
    njGetMatrix(&BgMATRIX[0]);
    ready = true;
}

// Load character data, such as animations, since they haven't been loaded at CSS
static bool load_char_data(u8 grp, u8 id) {
    if (texgrplds[grp].ok == 0 || texgrplds[grp].key < 0)
        return false;
    if (texgrpdat[grp].mode != TEXGROUP_MODE_CHARACTER)
        return false;

    u8* base = Get_ramcnt_pointer(texgrplds[grp].key);
    if (base == NULL)
        return false;
    // move to the character data
    u8* chd = base + texgrpdat[grp].to_chd;
    // initialize the character data
    for (int i = 0; i < 25; i++) {
        ((uintptr_t*)&viewer_char_data[id])[i] = (uintptr_t)chd + ((u32*)chd)[i];
    }
    viewer_data_key[id] = texgrplds[grp].key;
    return true;
}

void PaletteViewer_ReleaseSlot(u8 id) {
    if (id > 1 || id < 0)
        return;
    const int grp = loaded_grp[id];

    loaded_grp[id] = -1;
    viewer_data_key[id] = -1;
    anim_ready[id] = false;

    if (grp >= 1 && grp <= PALETTE_VIEWER_GROUPS && loaded_grp[id ^ 1] != grp) {
        purge_texture_group((u8)grp);
    }
}

void PaletteViewer_Release(void) {
    for (int i = 0; i < 2; i++) {
        PaletteViewer_ReleaseSlot(i);
    }
}

void PaletteViewer_Draw(u8 id) {
    if (!debug_config.spr_disp_on || No_Trans || id > 1 || id < 0) {
        if (G_No[1] != 1) {
            PaletteViewer_ReleaseSlot(id);
        }
        return;
    }
    const int sel = ID_of_Face[Cursor_Y[id]][Cursor_X[id]] + 1;

    if (sel < 1 || sel > PALETTE_VIEWER_GROUPS)
        return;

    const u8 grp = (u8)sel;
    const Character character = (Character)(sel - 1);

    if (mts_ok[PALETTE_VIEWER_MTS].be == 0) {
        make_texcash_work(PALETTE_VIEWER_MTS);

        if (mts_ok[PALETTE_VIEWER_MTS].be == 0)
            return;
    }

    if (loaded_grp[id] != (int)grp) {
        PaletteViewer_ReleaseSlot(id);
        loaded_grp[id] = (int)grp;
    }

    if (texgrplds[grp].ok == 0 || texgrplds[grp].key != viewer_data_key[id]) {
        if (texgrplds[grp].ok == 0 && load_any_texture_grpnum(grp, 5) == 0) {
            return;
        }

        if (!load_char_data(grp, id))
            return;
        anim_ready[id] = false;
    }

    // ensure_palette(character);
    setup_screen_matrix();

    if (!anim_ready[id]) {
        if (viewer_data_key[id] < 0 || viewer_char_data[id].nmca == NULL) {
            return;
        }
        SDL_zero(viewer_work[id]);

        const int idx = plid_data[character];
        const CharInitData saved = char_init_data[idx];
        // Borrow the game's char_init_data setter to init the preview, then put back whatever was in there
        char_init_data[idx] = viewer_char_data[id];
        viewer_work[id].charset_id = idx;
        set_char_base_data(&viewer_work[id]);
        char_init_data[idx] = saved;

        // 0,0 sets it to the idle animation
        set_char_move_init(&viewer_work[id], 0, 0);
        anim_ready[id] = true;
    }

    // advance the animation
    char_move(&viewer_work[id]);
    // put the character on the matrix
    viewer_work[id].my_mts = PALETTE_VIEWER_MTS;
    viewer_work[id].my_trans_mode = get_my_trans_mode(PALETTE_VIEWER_MTS);
    // the slot of the matrix setup by setup_screen_matrix
    viewer_work[id].my_family = 0;
    // controls position
    viewer_work[id].position_x = viewer_positions[id].x;
    viewer_work[id].position_y = viewer_positions[id].y;
    viewer_work[id].position_z = PALETTE_VIEWER_Z;
    viewer_work[id].my_priority = PALETTE_VIEWER_Z;
    // controls palette
    viewer_work[id].current_colcd = viewer_colcd[id];
    // flip based on ID
    viewer_work[id].cg_flip = (id + 1) % 2;

    if (obj_group_table[viewer_work[id].cg_number] != grp)
        return;
    // render the viewer_work
    Mtrans_use_trans_mode(&viewer_work[id], 0);
}

// #endif
