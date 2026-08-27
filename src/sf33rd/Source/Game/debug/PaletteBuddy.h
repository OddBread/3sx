// stores the auto-conversion tables between the character sprite palettes and the hud portrait/judgement
// palettes.
//
#include <types.h>
#define POR_PALETTE_SIZE 16

u16* convPalSprPor(u16* palSpr, s16 id);

static const char* character_names[] = {
    "Gill", "Alex", "Ryu",  "Yun",   "Dudley", "Necro",   "Hugo",   "Ibuki", "Elena",  "Oro",
    "Yang", "Ken",  "Sean", "Urien", "Akuma",  "Chun-Li", "Makoto", "Q",     "Twelve", "Remy",
};
/**
 * Contains all the data necessary for swapping between palette formats.
 * Only contains 12 for each character because the first 3 and last 1 are always
 * the same.
 * -1 applies pure black, -2 applies pure white (special cases for ken and alex only).
 *
 * colourdefs taken from here
 * https://github.com/Preppy/PalMod/blob/master/palmod/Game/SFIII3_A_DEF.h
 */
static const s16 hudPortraitLookup[20][12] = {
    { 3, 4, 6, 10, 19, 20, 22, 26, 16, 33, 34, 37 },   { -1, 1, 4, 7, 8, 10, 60, 49, 51, 53, 54, 56 },
    { 1, 34, 35, 36, 37, 39, 40, 2, 7, 61, 49, 51 },   { 40, 41, 42, 44, 46, 1, 16, 2, 32, 26, 28, 20 },
    { 1, 3, 4, 5, 7, 9, 32, 33, 48, 39, 21, 23 },      { 3, 7, 9, 11, 33, 36, 39, 12, 14, 15, 20, 26 },
    { 1, 2, 3, 4, 6, 7, 9, 11, 18, 36, 37, 38 },       { 11, 13, 14, 15, 1, 6, 48, 51, 53, 55, 35, 37 },
    { 7, 8, 10, 14, 63, 2, 3, 22, 30, 56, 16, 60 },    { 16, 17, 18, 21, 23, 26, 27, 28, 29, 30, 15, 42 },
    { 40, 41, 42, 44, 45, 48, 17, 18, 19, 1, 26, 20 }, { 35, 36, 37, 39, 60, -2, 10, 12, 48, 49, 50, 51 },
    { 34, 35, 36, 37, 39, 1, 2, 3, 4, 6, 16, 17 },     { 3, 4, 5, 6, 7, 8, 43, 44, 16, 33, 35, 37 },
    { 34, 36, 37, 38, 39, 40, 61, 2, 48, 49, 50, 51 }, { 1, 2, 3, 4, 5, 6, 8, 10, 12, 35, 38, 46 },
    { 1, 2, 3, 6, 32, 12, 14, 15, 33, 35, 26, 19 },    { 42, 43, 46, 29, 20, 22, 24, 26, 28, 1, 3, 5 },
    { 2, 3, 4, 5, 9, 10, 11, 12, 13, 14, 15, 1 },      { 3, 4, 6, 7, 9, 10, 12, 13, 14, 15, 21, 23 }
};
