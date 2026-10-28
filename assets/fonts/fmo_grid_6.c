/* Derived from Noto Sans CJK under SIL OFL 1.1; see OFL.txt. */
/*******************************************************************************
 * Size: 6 px
 * Bpp: 2
 * Opts: --font assets/fonts/NotoSansCJKsc-Regular.otf --range 0x20-0x7e --size 6 --bpp 2 --no-kerning --no-compress --format lvgl --lv-font-name fmo_grid_6 --lv-include lvgl.h --output assets/fonts/fmo_grid_6.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef FMO_GRID_6
#define FMO_GRID_6 1
#endif

#if FMO_GRID_6

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */

    /* U+0021 "!" */
    0x44, 0x0,

    /* U+0022 "\"" */
    0x54, 0x0,

    /* U+0023 "#" */
    0x14, 0x58, 0x58, 0x44,

    /* U+0024 "$" */
    0x21, 0x1, 0x42, 0x20,

    /* U+0025 "%" */
    0x51, 0x4, 0x54, 0x15, 0x1, 0x14,

    /* U+0026 "&" */
    0x14, 0x20, 0x55, 0x59,

    /* U+0027 "'" */
    0x40,

    /* U+0028 "(" */
    0x1, 0x44, 0x41, 0x0,

    /* U+0029 ")" */
    0x4, 0x11, 0x14, 0x0,

    /* U+002A "*" */
    0x20, 0x40,

    /* U+002B "+" */
    0x10, 0x64, 0x10,

    /* U+002C "," */
    0x4,

    /* U+002D "-" */
    0x50,

    /* U+002E "." */
    0x0,

    /* U+002F "/" */
    0x10, 0x41, 0x10, 0x40,

    /* U+0030 "0" */
    0x64, 0x88, 0x88, 0x54,

    /* U+0031 "1" */
    0x20, 0x41, 0x9,

    /* U+0032 "2" */
    0x54, 0x4, 0x10, 0x64,

    /* U+0033 "3" */
    0x14, 0x90, 0x95,

    /* U+0034 "4" */
    0x18, 0x18, 0x58, 0x8,

    /* U+0035 "5" */
    0x54, 0x54, 0x8, 0x54,

    /* U+0036 "6" */
    0x24, 0x94, 0x44, 0x14,

    /* U+0037 "7" */
    0x58, 0x4, 0x20, 0x20,

    /* U+0038 "8" */
    0x54, 0x54, 0x48, 0x54,

    /* U+0039 "9" */
    0x55, 0x21, 0x85,

    /* U+003A ":" */
    0x40, 0x0,

    /* U+003B ";" */
    0x40, 0x4,

    /* U+003C "<" */
    0x0, 0x14, 0x14, 0x0,

    /* U+003D "=" */
    0x54, 0x54,

    /* U+003E ">" */
    0x0, 0x54, 0x54, 0x0,

    /* U+003F "?" */
    0x14, 0x51, 0x4,

    /* U+0040 "@" */
    0x15, 0x44, 0x54, 0x41, 0x4, 0x54, 0x14, 0x0,

    /* U+0041 "A" */
    0x24, 0x14, 0x58, 0x81,

    /* U+0042 "B" */
    0x64, 0x58, 0x42, 0x54,

    /* U+0043 "C" */
    0x24, 0x80, 0x80, 0x24,

    /* U+0044 "D" */
    0x68, 0x42, 0x42, 0x68,

    /* U+0045 "E" */
    0x64, 0x54, 0x40, 0x64,

    /* U+0046 "F" */
    0x64, 0x40, 0x54, 0x40,

    /* U+0047 "G" */
    0x29, 0x80, 0x85, 0x25,

    /* U+0048 "H" */
    0x42, 0x66, 0x42, 0x42,

    /* U+0049 "I" */
    0x44, 0x44,

    /* U+004A "J" */
    0x8, 0x20, 0x95,

    /* U+004B "K" */
    0x44, 0x64, 0x58, 0x41,

    /* U+004C "L" */
    0x40, 0x40, 0x40, 0x64,

    /* U+004D "M" */
    0x52, 0x58, 0x95, 0x95, 0x11,

    /* U+004E "N" */
    0x52, 0x52, 0x4a, 0x42,

    /* U+004F "O" */
    0x29, 0x20, 0x88, 0x20, 0xa4,

    /* U+0050 "P" */
    0x64, 0x41, 0x54, 0x40,

    /* U+0051 "Q" */
    0x25, 0x20, 0x84, 0x20, 0xa0, 0x5, 0x0,

    /* U+0052 "R" */
    0x64, 0x41, 0x68, 0x45,

    /* U+0053 "S" */
    0x14, 0x60, 0x8, 0x58,

    /* U+0054 "T" */
    0x64, 0x10, 0x10, 0x10,

    /* U+0055 "U" */
    0x42, 0x42, 0x42, 0x25,

    /* U+0056 "V" */
    0x85, 0x48, 0x14, 0x20,

    /* U+0057 "W" */
    0x88, 0x84, 0x98, 0x61, 0x43, 0x24,

    /* U+0058 "X" */
    0x48, 0x24, 0x24, 0x48,

    /* U+0059 "Y" */
    0x88, 0x14, 0x20, 0x20,

    /* U+005A "Z" */
    0x18, 0x4, 0x20, 0xa4,

    /* U+005B "[" */
    0x54, 0x44, 0x40,

    /* U+005C "\\" */
    0x41, 0x1, 0x4, 0x10,

    /* U+005D "]" */
    0x11, 0x11, 0x11,

    /* U+005E "^" */
    0x10, 0x94, 0x40,

    /* U+005F "_" */
    0x54,

    /* U+0060 "`" */
    0x10, 0x0,

    /* U+0061 "a" */
    0x14, 0x65, 0x80,

    /* U+0062 "b" */
    0x40, 0x58, 0x41, 0x58,

    /* U+0063 "c" */
    0x16, 0x1, 0x40,

    /* U+0064 "d" */
    0x4, 0x18, 0x84, 0x58,

    /* U+0065 "e" */
    0x14, 0x94, 0x14,

    /* U+0066 "f" */
    0x22, 0x44, 0x10,

    /* U+0067 "g" */
    0x58, 0x54, 0x54, 0x54,

    /* U+0068 "h" */
    0x40, 0x58, 0x44, 0x44,

    /* U+0069 "i" */
    0x4, 0x44,

    /* U+006A "j" */
    0x0, 0x41, 0x4, 0x20,

    /* U+006B "k" */
    0x40, 0x48, 0x64, 0x48,

    /* U+006C "l" */
    0x44, 0x44,

    /* U+006D "m" */
    0x59, 0x84, 0x48, 0x44, 0x40,

    /* U+006E "n" */
    0x58, 0x44, 0x44,

    /* U+006F "o" */
    0x18, 0x84, 0x18,

    /* U+0070 "p" */
    0x58, 0x41, 0x58, 0x40,

    /* U+0071 "q" */
    0x18, 0x84, 0x58, 0x4,

    /* U+0072 "r" */
    0x51, 0x4, 0x0,

    /* U+0073 "s" */
    0x50, 0x45, 0x40,

    /* U+0074 "t" */
    0x42, 0x84, 0x8,

    /* U+0075 "u" */
    0x44, 0x44, 0x68,

    /* U+0076 "v" */
    0x88, 0x54, 0x30,

    /* U+0077 "w" */
    0x88, 0x55, 0x42, 0x30,

    /* U+0078 "x" */
    0x44, 0xc4, 0x40,

    /* U+0079 "y" */
    0x88, 0x14, 0x20, 0x50,

    /* U+007A "z" */
    0x24, 0x89, 0x40,

    /* U+007B "{" */
    0x10, 0x40, 0x1,

    /* U+007C "|" */
    0x55, 0x50,

    /* U+007D "}" */
    0x41, 0x11, 0x14,

    /* U+007E "~" */
    0x0, 0x4
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 22, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 0, .adv_w = 31, .box_w = 2, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 2, .adv_w = 46, .box_w = 3, .box_h = 2, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 4, .adv_w = 53, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 8, .adv_w = 53, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 12, .adv_w = 88, .box_w = 6, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 18, .adv_w = 65, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 22, .adv_w = 27, .box_w = 2, .box_h = 2, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 23, .adv_w = 32, .box_w = 2, .box_h = 7, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 27, .adv_w = 32, .box_w = 2, .box_h = 7, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 31, .adv_w = 45, .box_w = 3, .box_h = 2, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 33, .adv_w = 53, .box_w = 4, .box_h = 3, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 36, .adv_w = 27, .box_w = 2, .box_h = 2, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 37, .adv_w = 33, .box_w = 2, .box_h = 1, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 38, .adv_w = 27, .box_w = 2, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 39, .adv_w = 38, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 43, .adv_w = 53, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 47, .adv_w = 53, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 50, .adv_w = 53, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 54, .adv_w = 53, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 57, .adv_w = 53, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 61, .adv_w = 53, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 65, .adv_w = 53, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 69, .adv_w = 53, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 73, .adv_w = 53, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 77, .adv_w = 53, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 80, .adv_w = 27, .box_w = 2, .box_h = 3, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 82, .adv_w = 27, .box_w = 2, .box_h = 4, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 84, .adv_w = 53, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 88, .adv_w = 53, .box_w = 4, .box_h = 2, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 90, .adv_w = 53, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 94, .adv_w = 46, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 97, .adv_w = 91, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 105, .adv_w = 58, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 109, .adv_w = 63, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 113, .adv_w = 61, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 117, .adv_w = 66, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 121, .adv_w = 57, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 125, .adv_w = 53, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 129, .adv_w = 66, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 133, .adv_w = 70, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 137, .adv_w = 28, .box_w = 2, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 139, .adv_w = 51, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 142, .adv_w = 62, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 146, .adv_w = 52, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 150, .adv_w = 78, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 155, .adv_w = 69, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 159, .adv_w = 71, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 164, .adv_w = 61, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 168, .adv_w = 71, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 175, .adv_w = 61, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 179, .adv_w = 57, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 183, .adv_w = 58, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 187, .adv_w = 69, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 191, .adv_w = 55, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 195, .adv_w = 84, .box_w = 6, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 201, .adv_w = 55, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 205, .adv_w = 51, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 209, .adv_w = 58, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 213, .adv_w = 32, .box_w = 2, .box_h = 6, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 216, .adv_w = 38, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 220, .adv_w = 32, .box_w = 2, .box_h = 6, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 223, .adv_w = 53, .box_w = 3, .box_h = 3, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 226, .adv_w = 54, .box_w = 4, .box_h = 1, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 227, .adv_w = 58, .box_w = 3, .box_h = 2, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 229, .adv_w = 54, .box_w = 3, .box_h = 3, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 232, .adv_w = 59, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 236, .adv_w = 49, .box_w = 3, .box_h = 3, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 239, .adv_w = 60, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 243, .adv_w = 53, .box_w = 4, .box_h = 3, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 246, .adv_w = 31, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 249, .adv_w = 54, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 253, .adv_w = 58, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 257, .adv_w = 26, .box_w = 2, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 259, .adv_w = 26, .box_w = 3, .box_h = 5, .ofs_x = -1, .ofs_y = -1},
    {.bitmap_index = 263, .adv_w = 53, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 267, .adv_w = 27, .box_w = 2, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 269, .adv_w = 89, .box_w = 6, .box_h = 3, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 274, .adv_w = 59, .box_w = 4, .box_h = 3, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 277, .adv_w = 58, .box_w = 4, .box_h = 3, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 280, .adv_w = 60, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 284, .adv_w = 60, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 288, .adv_w = 37, .box_w = 3, .box_h = 3, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 291, .adv_w = 45, .box_w = 3, .box_h = 3, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 294, .adv_w = 36, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 297, .adv_w = 58, .box_w = 4, .box_h = 3, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 300, .adv_w = 50, .box_w = 4, .box_h = 3, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 303, .adv_w = 77, .box_w = 5, .box_h = 3, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 307, .adv_w = 48, .box_w = 3, .box_h = 3, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 310, .adv_w = 50, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 314, .adv_w = 46, .box_w = 3, .box_h = 3, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 317, .adv_w = 32, .box_w = 2, .box_h = 6, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 320, .adv_w = 26, .box_w = 1, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 322, .adv_w = 32, .box_w = 2, .box_h = 6, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 325, .adv_w = 53, .box_w = 4, .box_h = 2, .ofs_x = 0, .ofs_y = 1}
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
const lv_font_t fmo_grid_6 = {
#else
lv_font_t fmo_grid_6 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 7,          /*The maximum line height required by the font*/
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



#endif /*#if FMO_GRID_6*/
