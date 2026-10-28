/* Derived from Noto Sans CJK under SIL OFL 1.1; see OFL.txt. */
/*******************************************************************************
 * Size: 8 px
 * Bpp: 2
 * Opts: --font assets/fonts/NotoSansCJKsc-Regular.otf --range 0x20-0x7e --size 8 --bpp 2 --no-kerning --no-compress --format lvgl --lv-font-name fmo_grid_8 --lv-include lvgl.h --output assets/fonts/fmo_grid_8.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef FMO_GRID_8
#define FMO_GRID_8 1
#endif

#if FMO_GRID_8

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */

    /* U+0021 "!" */
    0x22, 0x22, 0x10, 0x20,

    /* U+0022 "\"" */
    0x58, 0x18, 0x4,

    /* U+0023 "#" */
    0x11, 0x4, 0x47, 0xa0, 0x44, 0x7a, 0x5, 0x1,
    0x40,

    /* U+0024 "$" */
    0x4, 0x1c, 0x21, 0x20, 0x18, 0x2, 0x2, 0x29,
    0x8,

    /* U+0025 "%" */
    0x24, 0x10, 0x88, 0x40, 0x88, 0x40, 0x89, 0x14,
    0x25, 0x48, 0x4, 0x48, 0x4, 0x24,

    /* U+0026 "&" */
    0x18, 0x2, 0x10, 0x28, 0x3, 0x44, 0x88, 0x88,
    0x34, 0x39, 0x80,

    /* U+0027 "'" */
    0x51, 0x0,

    /* U+0028 "(" */
    0x0, 0x42, 0x8, 0x10, 0x41, 0x8, 0x20, 0x0,

    /* U+0029 ")" */
    0x4, 0x22, 0x22, 0x22, 0x54,

    /* U+002A "*" */
    0x10, 0x38, 0x14,

    /* U+002B "+" */
    0x4, 0x2, 0x6, 0xe0, 0x20, 0x8, 0x0,

    /* U+002C "," */
    0x6, 0x14,

    /* U+002D "-" */
    0x64,

    /* U+002E "." */
    0x6,

    /* U+002F "/" */
    0x8, 0x8, 0x4, 0x10, 0x20, 0x10, 0x40, 0x80,
    0x40,

    /* U+0030 "0" */
    0x29, 0x14, 0x88, 0x22, 0x8, 0x82, 0x14, 0x82,
    0x80,

    /* U+0031 "1" */
    0x28, 0x8, 0x8, 0x8, 0x8, 0x8, 0x2e,

    /* U+0032 "2" */
    0x2c, 0x0, 0x80, 0x20, 0x14, 0x8, 0x8, 0xb,
    0xa0,

    /* U+0033 "3" */
    0x28, 0x2, 0x2, 0x1c, 0x2, 0x3, 0x69,

    /* U+0034 "4" */
    0x9, 0x6, 0x42, 0x51, 0x54, 0xaf, 0x1, 0x40,
    0x50,

    /* U+0035 "5" */
    0x3a, 0x4, 0x6, 0x80, 0xc, 0x3, 0x0, 0xc6,
    0xd0,

    /* U+0036 "6" */
    0x1a, 0x8, 0x5, 0x2, 0xa4, 0x52, 0x14, 0x82,
    0x90,

    /* U+0037 "7" */
    0x6b, 0x0, 0x40, 0x80, 0x20, 0x8, 0x5, 0x1,
    0x40,

    /* U+0038 "8" */
    0x29, 0x14, 0x82, 0x20, 0xb4, 0x43, 0x20, 0x82,
    0x90,

    /* U+0039 "9" */
    0x28, 0x14, 0x88, 0x21, 0x4c, 0x2a, 0x0, 0x82,
    0x80,

    /* U+003A ":" */
    0x60, 0x0, 0x60,

    /* U+003B ";" */
    0x60, 0x0, 0x61, 0x40,

    /* U+003C "<" */
    0x0, 0x2, 0x8a, 0x0, 0xa4, 0x1, 0x0,

    /* U+003D "=" */
    0x6a, 0x0, 0x6, 0xa0,

    /* U+003E ">" */
    0x0, 0x19, 0x0, 0x70, 0xa4, 0x40, 0x0,

    /* U+003F "?" */
    0x68, 0x5, 0x8, 0x14, 0x10, 0x0, 0x20,

    /* U+0040 "@" */
    0x6, 0xa0, 0x20, 0x8, 0x11, 0x88, 0x44, 0x84,
    0x48, 0x88, 0x45, 0x60, 0x20, 0x0, 0xa, 0x80,

    /* U+0041 "A" */
    0xc, 0x6, 0x42, 0x20, 0x88, 0x7b, 0x20, 0x58,
    0x8,

    /* U+0042 "B" */
    0x3a, 0x8, 0x52, 0x14, 0xec, 0x20, 0x88, 0x33,
    0xa4,

    /* U+0043 "C" */
    0x1e, 0x48, 0x5, 0x1, 0x0, 0x50, 0x8, 0x1,
    0xe4,

    /* U+0044 "D" */
    0x3a, 0x2, 0x18, 0x20, 0xc2, 0xc, 0x20, 0xc2,
    0x18, 0x3a, 0x0,

    /* U+0045 "E" */
    0x3a, 0x8, 0x2, 0x0, 0xe8, 0x20, 0x8, 0x3,
    0xa0,

    /* U+0046 "F" */
    0x3a, 0x8, 0x2, 0x0, 0xe8, 0x20, 0x8, 0x2,
    0x0,

    /* U+0047 "G" */
    0x1e, 0x48, 0x5, 0x1, 0xa, 0x50, 0x88, 0x21,
    0xf8,

    /* U+0048 "H" */
    0x20, 0x82, 0x8, 0x20, 0x83, 0xac, 0x20, 0x82,
    0x8, 0x20, 0x80,

    /* U+0049 "I" */
    0x22, 0x22, 0x22, 0x20,

    /* U+004A "J" */
    0x2, 0x2, 0x2, 0x2, 0x2, 0x5, 0x6c,

    /* U+004B "K" */
    0x21, 0x82, 0x20, 0x28, 0x3, 0xd0, 0x33, 0x2,
    0x14, 0x20, 0xc0,

    /* U+004C "L" */
    0x20, 0x8, 0x2, 0x0, 0x80, 0x20, 0x8, 0x3,
    0xa0,

    /* U+004D "M" */
    0x30, 0x63, 0xa, 0x24, 0xa1, 0x86, 0x1a, 0x21,
    0x62, 0x10, 0x20,

    /* U+004E "N" */
    0x30, 0x8d, 0x22, 0x88, 0x56, 0x12, 0x84, 0xb1,
    0xc,

    /* U+004F "O" */
    0x1f, 0x42, 0x8, 0x50, 0x58, 0x5, 0x50, 0x52,
    0x8, 0x1a, 0x40,

    /* U+0050 "P" */
    0x3a, 0x8, 0x22, 0x8, 0x85, 0x3a, 0x8, 0x2,
    0x0,

    /* U+0051 "Q" */
    0x1f, 0x42, 0x8, 0x50, 0x58, 0x5, 0x50, 0x52,
    0x8, 0x2a, 0x80, 0x20, 0x1, 0x90,

    /* U+0052 "R" */
    0x3a, 0x8, 0x22, 0x14, 0xf8, 0x22, 0x8, 0x82,
    0x8,

    /* U+0053 "S" */
    0x2e, 0x18, 0x3, 0x0, 0x78, 0x2, 0x0, 0x52,
    0xa0,

    /* U+0054 "T" */
    0xae, 0x42, 0x0, 0x80, 0x20, 0x8, 0x2, 0x0,
    0x80,

    /* U+0055 "U" */
    0x20, 0x88, 0x22, 0x8, 0x82, 0x20, 0x88, 0x21,
    0xf0,

    /* U+0056 "V" */
    0xc1, 0x60, 0x85, 0x20, 0x88, 0x25, 0xa, 0x1,
    0xc0,

    /* U+0057 "W" */
    0x83, 0xa, 0xc, 0x25, 0x55, 0x4a, 0x24, 0x28,
    0xa0, 0xa2, 0x82, 0x46, 0x0,

    /* U+0058 "X" */
    0x92, 0x8, 0x82, 0x80, 0x70, 0x29, 0x8, 0x88,
    0x20,

    /* U+0059 "Y" */
    0x20, 0x81, 0x48, 0x9, 0x0, 0xa0, 0x6, 0x0,
    0x10, 0x1, 0x0,

    /* U+005A "Z" */
    0x2b, 0x40, 0x80, 0x50, 0x20, 0x24, 0x8, 0xb,
    0xa4,

    /* U+005B "[" */
    0x20, 0x41, 0x4, 0x10, 0x41, 0x4, 0x20,

    /* U+005C "\\" */
    0x80, 0x40, 0x10, 0x20, 0x20, 0x10, 0x4, 0x8,
    0x4,

    /* U+005D "]" */
    0x62, 0x22, 0x22, 0x22, 0x60,

    /* U+005E "^" */
    0x18, 0x28, 0x21, 0x52,

    /* U+005F "_" */
    0x6a, 0x0,

    /* U+0060 "`" */
    0x42, 0x0,

    /* U+0061 "a" */
    0x2d, 0x2, 0x2b, 0x53, 0x2a,

    /* U+0062 "b" */
    0x50, 0x14, 0x6, 0xb1, 0x85, 0x51, 0x54, 0x96,
    0xa0,

    /* U+0063 "c" */
    0x29, 0x50, 0x80, 0x50, 0x2a,

    /* U+0064 "d" */
    0x2, 0x0, 0x82, 0xb1, 0x48, 0x82, 0x14, 0x83,
    0xa0,

    /* U+0065 "e" */
    0x29, 0x14, 0x8a, 0xa1, 0x40, 0x29, 0x0,

    /* U+0066 "f" */
    0x24, 0x8b, 0x48, 0x20, 0x82, 0x0,

    /* U+0067 "g" */
    0x2b, 0x14, 0x83, 0x81, 0x40, 0x3a, 0x10, 0x52,
    0xa0,

    /* U+0068 "h" */
    0x50, 0x14, 0x6, 0xa1, 0x88, 0x52, 0x14, 0x85,
    0x20,

    /* U+0069 "i" */
    0x50, 0x55, 0x55, 0x50,

    /* U+006A "j" */
    0x14, 0x0, 0x41, 0x4, 0x10, 0x45, 0x20,

    /* U+006B "k" */
    0x50, 0x14, 0x5, 0x21, 0x60, 0x7c, 0x19, 0x85,
    0x20,

    /* U+006C "l" */
    0x55, 0x55, 0x55, 0x20,

    /* U+006D "m" */
    0x6a, 0x75, 0x88, 0x25, 0x20, 0x94, 0x82, 0x52,
    0x8,

    /* U+006E "n" */
    0x6a, 0x18, 0x85, 0x21, 0x48, 0x52, 0x0,

    /* U+006F "o" */
    0x2a, 0x14, 0x88, 0x15, 0x48, 0x2a, 0x0,

    /* U+0070 "p" */
    0x6b, 0x18, 0x55, 0x15, 0x49, 0x7a, 0x14, 0x5,
    0x0,

    /* U+0071 "q" */
    0x2a, 0x14, 0x88, 0x21, 0x48, 0x3b, 0x0, 0x80,
    0x20,

    /* U+0072 "r" */
    0x68, 0x60, 0x50, 0x50, 0x50,

    /* U+0073 "s" */
    0x28, 0x50, 0x28, 0x5, 0x68,

    /* U+0074 "t" */
    0x10, 0x8b, 0x88, 0x20, 0x82, 0x80,

    /* U+0075 "u" */
    0x52, 0x14, 0x85, 0x20, 0x48, 0x2a, 0x0,

    /* U+0076 "v" */
    0x82, 0x14, 0x82, 0x50, 0xa0, 0x18, 0x0,

    /* U+0077 "w" */
    0x82, 0x21, 0x58, 0x82, 0x92, 0xa, 0x24, 0x24,
    0xc0,

    /* U+0078 "x" */
    0x55, 0x28, 0x28, 0x28, 0x82,

    /* U+0079 "y" */
    0x82, 0x14, 0x82, 0x50, 0xa0, 0x18, 0x5, 0x6,
    0x0,

    /* U+007A "z" */
    0x6d, 0x8, 0x24, 0x20, 0xb9,

    /* U+007B "{" */
    0x20, 0x82, 0x8, 0x90, 0x82, 0x8, 0x10,

    /* U+007C "|" */
    0x11, 0x11, 0x11, 0x11, 0x11,

    /* U+007D "}" */
    0x50, 0x82, 0x8, 0x24, 0x82, 0x8, 0x50,

    /* U+007E "~" */
    0x20, 0x2, 0x40
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 29, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 0, .adv_w = 41, .box_w = 2, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 4, .adv_w = 61, .box_w = 4, .box_h = 3, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 7, .adv_w = 71, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 16, .adv_w = 71, .box_w = 4, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 25, .adv_w = 118, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 39, .adv_w = 87, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 50, .adv_w = 36, .box_w = 2, .box_h = 3, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 52, .adv_w = 43, .box_w = 3, .box_h = 10, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 60, .adv_w = 43, .box_w = 2, .box_h = 10, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 65, .adv_w = 60, .box_w = 4, .box_h = 3, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 68, .adv_w = 71, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 75, .adv_w = 36, .box_w = 2, .box_h = 4, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 77, .adv_w = 44, .box_w = 3, .box_h = 1, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 78, .adv_w = 36, .box_w = 2, .box_h = 2, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 79, .adv_w = 50, .box_w = 4, .box_h = 9, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 88, .adv_w = 71, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 97, .adv_w = 71, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 104, .adv_w = 71, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 113, .adv_w = 71, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 120, .adv_w = 71, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 129, .adv_w = 71, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 138, .adv_w = 71, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 147, .adv_w = 71, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 156, .adv_w = 71, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 165, .adv_w = 71, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 174, .adv_w = 36, .box_w = 2, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 177, .adv_w = 36, .box_w = 2, .box_h = 7, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 181, .adv_w = 71, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 188, .adv_w = 71, .box_w = 5, .box_h = 3, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 192, .adv_w = 71, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 199, .adv_w = 61, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 206, .adv_w = 121, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 222, .adv_w = 78, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 231, .adv_w = 84, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 240, .adv_w = 82, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 249, .adv_w = 88, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 260, .adv_w = 75, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 269, .adv_w = 71, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 278, .adv_w = 88, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 287, .adv_w = 93, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 298, .adv_w = 38, .box_w = 2, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 302, .adv_w = 68, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 309, .adv_w = 83, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 320, .adv_w = 70, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 329, .adv_w = 104, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 340, .adv_w = 93, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 349, .adv_w = 95, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 360, .adv_w = 81, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 369, .adv_w = 95, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 383, .adv_w = 81, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 392, .adv_w = 76, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 401, .adv_w = 77, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 410, .adv_w = 92, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 419, .adv_w = 74, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 428, .adv_w = 112, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 441, .adv_w = 73, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 450, .adv_w = 68, .box_w = 6, .box_h = 7, .ofs_x = -1, .ofs_y = 0},
    {.bitmap_index = 461, .adv_w = 77, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 470, .adv_w = 43, .box_w = 3, .box_h = 9, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 477, .adv_w = 50, .box_w = 4, .box_h = 9, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 486, .adv_w = 43, .box_w = 2, .box_h = 9, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 491, .adv_w = 71, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 495, .adv_w = 72, .box_w = 5, .box_h = 1, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 497, .adv_w = 78, .box_w = 2, .box_h = 3, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 499, .adv_w = 72, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 504, .adv_w = 79, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 513, .adv_w = 65, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 518, .adv_w = 79, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 527, .adv_w = 71, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 534, .adv_w = 42, .box_w = 3, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 540, .adv_w = 72, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 549, .adv_w = 78, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 558, .adv_w = 35, .box_w = 2, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 562, .adv_w = 35, .box_w = 3, .box_h = 9, .ofs_x = -1, .ofs_y = -2},
    {.bitmap_index = 569, .adv_w = 71, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 578, .adv_w = 36, .box_w = 2, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 582, .adv_w = 119, .box_w = 7, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 591, .adv_w = 78, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 598, .adv_w = 78, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 605, .adv_w = 79, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 614, .adv_w = 79, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 623, .adv_w = 50, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 628, .adv_w = 60, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 633, .adv_w = 48, .box_w = 3, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 639, .adv_w = 78, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 646, .adv_w = 67, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 653, .adv_w = 103, .box_w = 7, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 662, .adv_w = 64, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 667, .adv_w = 67, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 676, .adv_w = 61, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 681, .adv_w = 43, .box_w = 3, .box_h = 9, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 688, .adv_w = 35, .box_w = 2, .box_h = 10, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 693, .adv_w = 43, .box_w = 3, .box_h = 9, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 700, .adv_w = 71, .box_w = 5, .box_h = 2, .ofs_x = 0, .ofs_y = 2}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 2,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t fmo_grid_8 = {
#else
lv_font_t fmo_grid_8 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 10,          /*The maximum line height required by the font*/
    .base_line = 2,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 0,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if FMO_GRID_8*/
