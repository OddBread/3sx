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
#define VIEWER_COLOR_ROW 100

#define PALETTE_VIEWER_GROUPS 20 // groups 1..20 == Gill..Remy
#define PALETTE_VIEWER_Z 0       // must stay < PRIO_BASE_SIZE (128)

// the game's character struct, which stores various object properties
// TODO: make 2 of these once the first is finished
static WORK viewer_work;
static s16 viewer_colcd = 0;
// Array of pointers to important character data sections
static CharInitData viewer_char_data;
static s16 viewer_data_key = -1;

// states for controlling
static int loaded_grp = -1;
static int palette_character = -1;
static bool anim_ready = false;
static struct ViewerPos viewer_positions[2] = { { 90, 100 }, { 310, 40 } };

// Get a palette from a .ACT file and load it into the color row the preview uses
// TODO: make this trigger on visibility change instead of just the swapping palettes call
bool ViewerPaletteFromAct(const char* file, const char* character, s16 mts_slot) {
    // ConvActToPal returns a pointer to a *static* buffer, so copy it immediately.
    u16* pal = ConvActToPal(file, character);
    if (pal == NULL) {
        return false;
    }
    // put the palette in
    SDL_memcpy(ColorRAM[VIEWER_COLOR_ROW], pal, 64 * sizeof(u16));
    if (mts[mts_slot].texList.pal == palGetChunkGhostCP3()) {
        // CP3 bank index == ColorRAM row
        palUpdateGhostCP3(VIEWER_COLOR_ROW, 1);
        viewer_colcd = VIEWER_COLOR_ROW;
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
static bool load_char_data(u8 grp) {
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
        ((uintptr_t*)&viewer_char_data)[i] = (uintptr_t)chd + ((u32*)chd)[i];
    }
    viewer_data_key = texgrplds[grp].key;
    return true;
}

// Loads the character's colours into CP3 rows 0..15 (what slot 13 reads).
// TODO: get rid of this and add in .ACT loader code above
static void ensure_palette(Character character) {
    if (palette_character == (int)character)
        return;
    const s8 saved_char = My_char[0];
    const s8 saved_color = Player_Color[0];
    My_char[0] = (s8)character;
    Player_Color[0] = 0;
    load_any_color((u16)character, 2);
    My_char[0] = saved_char;
    Player_Color[0] = saved_color;

    palette_character = (int)character;
    viewer_colcd = 0;
}

void PaletteViewer_Draw(u8 id) {
    if (!debug_config.spr_disp_on || No_Trans || G_No[1] != 1 || id > 1 || id < 0) {
        if (loaded_grp >= 1 && loaded_grp <= PALETTE_VIEWER_GROUPS && G_No[1] != 1) {
            purge_texture_group((u8)loaded_grp); // safe now: we no longer own any game state
        }
        loaded_grp = -1;
        viewer_data_key = -1;
        anim_ready = false;
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

    if (loaded_grp != (int)grp) {
        PaletteViewer_Release();
        loaded_grp = (int)grp;
    }

    if (texgrplds[grp].ok == 0 || texgrplds[grp].key != viewer_data_key) {
        if (texgrplds[grp].ok == 0 && load_any_texture_grpnum(grp, 5) == 0) {
            return;
        }

        if (!load_char_data(grp))
            return;
        anim_ready = false;
    }

    // ensure_palette(character);
    setup_screen_matrix();

    if (!anim_ready) {
        if (viewer_data_key < 0 || viewer_char_data.nmca == NULL) {
            return;
        }
        SDL_zero(viewer_work);

        const int idx = plid_data[character];
        const CharInitData saved = char_init_data[idx];
        // Borrow the game's char_init_data setter to init the preview, then put back whatever was in there
        char_init_data[idx] = viewer_char_data;
        viewer_work.charset_id = idx;
        set_char_base_data(&viewer_work);
        char_init_data[idx] = saved;

        // 0,0 sets it to the idle animation
        set_char_move_init(&viewer_work, 0, 0);
        anim_ready = true;
    }

    // advance the animation
    char_move(&viewer_work);
    // put the character on the matrix
    viewer_work.my_mts = PALETTE_VIEWER_MTS;
    viewer_work.my_trans_mode = get_my_trans_mode(PALETTE_VIEWER_MTS);
    // the slot of the matrix setup by setup_screen_matrix
    viewer_work.my_family = 0;
    // controls position
    viewer_work.position_x = viewer_positions[0].x;
    viewer_work.position_y = viewer_positions[0].y;
    viewer_work.position_z = PALETTE_VIEWER_Z;
    viewer_work.my_priority = PALETTE_VIEWER_Z;
    // controls palette
    viewer_work.current_colcd = viewer_colcd;
    // flip based on ID
    viewer_work.cg_flip = (id + 1) % 2;

    if (obj_group_table[viewer_work.cg_number] != grp)
        return;
    // render the viewer_work
    Mtrans_use_trans_mode(&viewer_work, 0);
}

void PaletteViewer_Release(void) {
    if (loaded_grp >= 1 && loaded_grp <= PALETTE_VIEWER_GROUPS) {
        purge_texture_group((u8)loaded_grp); // hands the ~2.5 MB back
    }

    loaded_grp = -1;
    viewer_data_key = -1;
    anim_ready = false;
}

// #endif
