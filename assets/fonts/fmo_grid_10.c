/* Derived from Noto Sans CJK under SIL OFL 1.1; see OFL.txt. */
/*******************************************************************************
 * Size: 10 px
 * Bpp: 2
 * Opts: --font assets/fonts/NotoSansCJKsc-Regular.otf --range 0x20-0x7e --size 10 --bpp 2 --no-kerning --no-compress --format lvgl --lv-font-name fmo_grid_10 --lv-include lvgl.h --output assets/fonts/fmo_grid_10.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef FMO_GRID_10
#define FMO_GRID_10 1
#endif

#if FMO_GRID_10

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */

    /* U+0021 "!" */
    0x30, 0xc3, 0xc, 0x20, 0x41, 0xc,

    /* U+0022 "\"" */
    0x33, 0x33, 0x22, 0x0,

    /* U+0023 "#" */
    0x4, 0x81, 0x44, 0x7b, 0xc2, 0x10, 0x22, 0x7,
    0xb8, 0x22, 0x2, 0x20,

    /* U+0024 "$" */
    0x8, 0x3, 0x83, 0x4, 0xc0, 0x1c, 0x0, 0xd0,
    0xc, 0x46, 0x1e, 0x2, 0x0,

    /* U+0025 "%" */
    0x28, 0x8, 0x14, 0x84, 0x8, 0x22, 0x1, 0x49,
    0x28, 0x28, 0x94, 0x80, 0x44, 0x20, 0x21, 0x48,
    0x10, 0x28,

    /* U+0026 "&" */
    0xe, 0x0, 0x85, 0x1, 0x60, 0x7, 0x0, 0x39,
    0x22, 0x4d, 0x8a, 0xe, 0xb, 0xd9,

    /* U+0027 "'" */
    0x33, 0x20,

    /* U+0028 "(" */
    0x4, 0x22, 0x8, 0x20, 0x82, 0x8, 0x20, 0x50,
    0x80,

    /* U+0029 ")" */
    0x40, 0x82, 0x8, 0x14, 0x51, 0x48, 0x20, 0x84,
    0x0,

    /* U+002A "*" */
    0x8, 0xb, 0x82, 0xc0, 0x0,

    /* U+002B "+" */
    0x4, 0x0, 0x80, 0x8, 0x6, 0xf8, 0x8, 0x0,
    0x80,

    /* U+002C "," */
    0x0, 0xc2, 0x14, 0x0,

    /* U+002D "-" */
    0x68,

    /* U+002E "." */
    0x10, 0xc0,

    /* U+002F "/" */
    0x2, 0x5, 0x8, 0x8, 0x4, 0x10, 0x20, 0x20,
    0x50, 0x80, 0x40,

    /* U+0030 "0" */
    0x1f, 0x3, 0x18, 0x60, 0xc5, 0xc, 0x50, 0xc6,
    0xc, 0x31, 0x81, 0xf0,

    /* U+0031 "1" */
    0x1d, 0x2, 0x40, 0x50, 0x14, 0x5, 0x1, 0x40,
    0x50, 0xfe,

    /* U+0032 "2" */
    0x2e, 0x4, 0x28, 0x1, 0x80, 0x14, 0x3, 0x0,
    0x90, 0x24, 0x7, 0xfc,

    /* U+0033 "3" */
    0x2e, 0x0, 0x60, 0x14, 0x3c, 0x1, 0x80, 0x34,
    0x1c, 0xbc,

    /* U+0034 "4" */
    0x3, 0x40, 0xb4, 0xa, 0x42, 0x24, 0x62, 0x4b,
    0xfc, 0x2, 0x40, 0x24,

    /* U+0035 "5" */
    0x3f, 0x83, 0x0, 0x30, 0x3, 0xa0, 0x1, 0xc0,
    0xc, 0x41, 0xc2, 0xf0,

    /* U+0036 "6" */
    0xf, 0x83, 0x0, 0x20, 0x6, 0xb4, 0x60, 0xc6,
    0xc, 0x30, 0xc1, 0xf4,

    /* U+0037 "7" */
    0x7f, 0xc0, 0x18, 0x3, 0x0, 0x60, 0x9, 0x0,
    0xc0, 0xc, 0x0, 0xc0,

    /* U+0038 "8" */
    0x1f, 0x3, 0x8, 0x30, 0x81, 0xa0, 0x26, 0x45,
    0xc, 0x60, 0xc2, 0xf4,

    /* U+0039 "9" */
    0x2e, 0x6, 0x18, 0x90, 0xc6, 0x1c, 0x29, 0xc0,
    0xc, 0x2, 0x43, 0xe0,

    /* U+003A ":" */
    0x30, 0x40, 0x0, 0x10, 0xc0,

    /* U+003B ";" */
    0x30, 0x40, 0x0, 0x0, 0xc2, 0x14, 0x0,

    /* U+003C "<" */
    0x0, 0x0, 0x68, 0x78, 0x7, 0x80, 0x6, 0x80,
    0x0,

    /* U+003D "=" */
    0x6a, 0x80, 0x0, 0x6a, 0x80,

    /* U+003E ">" */
    0x0, 0x7, 0x80, 0x6, 0x80, 0x68, 0x78, 0x0,
    0x0,

    /* U+003F "?" */
    0x2e, 0x0, 0xc0, 0x30, 0x24, 0xc, 0x1, 0x0,
    0x40, 0x60,

    /* U+0040 "@" */
    0x2, 0xa8, 0x2, 0x0, 0x82, 0xa, 0x48, 0x88,
    0x52, 0x52, 0x20, 0x94, 0x8c, 0x55, 0x19, 0x90,
    0x80, 0x0, 0x18, 0x0, 0x0, 0xa8, 0x0,

    /* U+0041 "A" */
    0xa, 0x0, 0x3c, 0x1, 0xa4, 0x9, 0x60, 0x30,
    0xc0, 0xff, 0x45, 0x6, 0x30, 0xc,

    /* U+0042 "B" */
    0xfe, 0xc, 0x18, 0xc1, 0x4e, 0xf0, 0xc1, 0xcc,
    0xc, 0xc0, 0xcf, 0xf0,

    /* U+0043 "C" */
    0xb, 0xd0, 0x90, 0x43, 0x0, 0x18, 0x0, 0x60,
    0x0, 0xc0, 0x3, 0x41, 0x2, 0xf4,

    /* U+0044 "D" */
    0xfe, 0xc, 0x28, 0xc0, 0xcc, 0x8, 0xc0, 0x8c,
    0xc, 0xc2, 0x8f, 0xe0,

    /* U+0045 "E" */
    0xff, 0x30, 0xc, 0x3, 0xf8, 0xc0, 0x30, 0xc,
    0x3, 0xfd,

    /* U+0046 "F" */
    0xff, 0x30, 0xc, 0x3, 0x0, 0xfe, 0x30, 0xc,
    0x3, 0x0,

    /* U+0047 "G" */
    0xb, 0xd0, 0x90, 0x43, 0x0, 0x18, 0x0, 0x60,
    0xf0, 0xc0, 0x82, 0x43, 0x2, 0xf8,

    /* U+0048 "H" */
    0xc0, 0x9c, 0x9, 0xc0, 0x9f, 0xfd, 0xc0, 0x9c,
    0x9, 0xc0, 0x9c, 0x9,

    /* U+0049 "I" */
    0xff, 0xff,

    /* U+004A "J" */
    0x2, 0x40, 0x90, 0x24, 0x9, 0x2, 0x40, 0x95,
    0x30, 0xf8,

    /* U+004B "K" */
    0xc1, 0x8c, 0x30, 0xcc, 0xe, 0xc0, 0xe6, 0xc,
    0x30, 0xc1, 0x8c, 0xc,

    /* U+004C "L" */
    0xc0, 0x30, 0xc, 0x3, 0x0, 0xc0, 0x30, 0xc,
    0x3, 0xfc,

    /* U+004D "M" */
    0xd0, 0x33, 0x81, 0xce, 0xa, 0x39, 0x28, 0xc9,
    0x23, 0x28, 0x8c, 0x62, 0x30, 0x8,

    /* U+004E "N" */
    0xc0, 0x8e, 0x8, 0xe4, 0x8c, 0xc8, 0xc9, 0x8c,
    0x38, 0xc2, 0xcc, 0xc,

    /* U+004F "O" */
    0xb, 0xd0, 0xd0, 0xc3, 0x1, 0x98, 0x3, 0x60,
    0xc, 0xc0, 0x63, 0x43, 0x2, 0xf4,

    /* U+0050 "P" */
    0xfe, 0x30, 0x6c, 0xf, 0xa, 0xfe, 0x30, 0xc,
    0x3, 0x0,

    /* U+0051 "Q" */
    0xb, 0xd0, 0xd0, 0xc3, 0x1, 0x98, 0x3, 0x60,
    0xc, 0xc0, 0x63, 0x43, 0x2, 0xf4, 0x1, 0x80,
    0x1, 0xe0,

    /* U+0052 "R" */
    0xff, 0xc, 0x18, 0xc0, 0xcc, 0x28, 0xfe, 0xc,
    0x60, 0xc3, 0xc, 0x1c,

    /* U+0053 "S" */
    0x1f, 0x83, 0x4, 0x30, 0x1, 0xd0, 0x2, 0xc0,
    0x9, 0x10, 0x92, 0xf8,

    /* U+0054 "T" */
    0xbf, 0xe0, 0x50, 0x5, 0x0, 0x50, 0x5, 0x0,
    0x50, 0x5, 0x0, 0x50,

    /* U+0055 "U" */
    0x30, 0x20, 0xc0, 0x83, 0x2, 0xc, 0x8, 0x30,
    0x20, 0xc0, 0xc2, 0x43, 0x2, 0xf0,

    /* U+0056 "V" */
    0xc0, 0x69, 0x9, 0x60, 0xc3, 0x8, 0x21, 0x41,
    0x60, 0xf, 0x0, 0xe0,

    /* U+0057 "W" */
    0x90, 0xc1, 0x54, 0x70, 0x86, 0x29, 0x30, 0xc8,
    0x8c, 0x32, 0x32, 0x9, 0x49, 0x41, 0xc1, 0xd0,
    0x70, 0x30,

    /* U+0058 "X" */
    0x60, 0xc3, 0x18, 0x1a, 0x0, 0xe0, 0xe, 0x2,
    0x60, 0x30, 0xc9, 0x9,

    /* U+0059 "Y" */
    0x30, 0x30, 0x61, 0x80, 0xc8, 0x1, 0xa0, 0x3,
    0x40, 0xc, 0x0, 0x30, 0x0, 0xc0,

    /* U+005A "Z" */
    0x3f, 0xd0, 0xc, 0x2, 0x40, 0x30, 0xc, 0x1,
    0x80, 0x30, 0x7, 0xfd,

    /* U+005B "[" */
    0xe2, 0x8, 0x20, 0x82, 0x8, 0x20, 0x82, 0xa,
    0x0,

    /* U+005C "\\" */
    0x80, 0x40, 0x20, 0x20, 0x20, 0x14, 0x8, 0x8,
    0x4, 0x2, 0x1,

    /* U+005D "]" */
    0x64, 0x51, 0x45, 0x14, 0x51, 0x45, 0x14, 0x56,
    0x40,

    /* U+005E "^" */
    0x4, 0x3, 0x81, 0x60, 0x85, 0x20, 0x80,

    /* U+005F "_" */
    0xaa, 0x90,

    /* U+0060 "`" */
    0x1, 0xc1, 0x40,

    /* U+0061 "a" */
    0x2f, 0x0, 0x60, 0x6c, 0xc3, 0x61, 0xcb, 0x70,

    /* U+0062 "b" */
    0x30, 0x3, 0x0, 0x3b, 0x83, 0x9, 0x30, 0x63,
    0x6, 0x30, 0xd3, 0xb8,

    /* U+0063 "c" */
    0x1f, 0x4c, 0x6, 0x1, 0x80, 0x30, 0x7, 0xd0,

    /* U+0064 "d" */
    0x0, 0x90, 0x9, 0x1f, 0xd3, 0xd, 0x60, 0x96,
    0x9, 0x30, 0xd2, 0xe9,

    /* U+0065 "e" */
    0x1f, 0x42, 0xc, 0x7a, 0xc6, 0x0, 0x30, 0x1,
    0xf8,

    /* U+0066 "f" */
    0x1d, 0x30, 0x30, 0xbc, 0x30, 0x30, 0x30, 0x30,
    0x30,

    /* U+0067 "g" */
    0x2f, 0xd3, 0x18, 0x31, 0x82, 0xa0, 0x20, 0x3,
    0xf8, 0x50, 0x92, 0xa8,

    /* U+0068 "h" */
    0x30, 0x3, 0x0, 0x3b, 0x83, 0xc, 0x30, 0x93,
    0x9, 0x30, 0x93, 0x9,

    /* U+0069 "i" */
    0x30, 0x33, 0x33, 0x33,

    /* U+006A "j" */
    0xc, 0x0, 0xc3, 0xc, 0x30, 0xc3, 0xd, 0xd0,

    /* U+006B "k" */
    0x30, 0x3, 0x0, 0x30, 0xc3, 0x30, 0x3a, 0x3,
    0xb0, 0x31, 0x83, 0xc,

    /* U+006C "l" */
    0x30, 0xc3, 0xc, 0x30, 0xc3, 0xd,

    /* U+006D "m" */
    0x2b, 0x5f, 0xc, 0x34, 0x93, 0xc, 0x24, 0xc3,
    0x5, 0x30, 0xc1, 0x4c, 0x30, 0x50,

    /* U+006E "n" */
    0x2b, 0x83, 0xc, 0x30, 0x93, 0x9, 0x30, 0x93,
    0x9,

    /* U+006F "o" */
    0x1f, 0x43, 0xc, 0x60, 0x96, 0x9, 0x30, 0xc1,
    0xf4,

    /* U+0070 "p" */
    0x3b, 0x83, 0x9, 0x30, 0x63, 0x6, 0x30, 0xd3,
    0xb8, 0x30, 0x3, 0x0,

    /* U+0071 "q" */
    0x1f, 0x93, 0xd, 0x60, 0x96, 0x9, 0x30, 0xd2,
    0xed, 0x0, 0x90, 0x9,

    /* U+0072 "r" */
    0x2b, 0x34, 0x30, 0x30, 0x30, 0x30,

    /* U+0073 "s" */
    0x2e, 0x18, 0x3, 0x40, 0x1c, 0x2, 0x5f, 0x80,

    /* U+0074 "t" */
    0x10, 0x30, 0xbd, 0x30, 0x30, 0x30, 0x30, 0x1d,

    /* U+0075 "u" */
    0x30, 0x83, 0x8, 0x30, 0x83, 0x8, 0x30, 0xc2,
    0xe8,

    /* U+0076 "v" */
    0x80, 0xc6, 0x18, 0x32, 0x42, 0x30, 0x1a, 0x0,
    0xd0,

    /* U+0077 "w" */
    0x92, 0x82, 0x62, 0x85, 0x32, 0x88, 0x35, 0x5c,
    0x28, 0x28, 0x1c, 0x34,

    /* U+0078 "x" */
    0x62, 0x4c, 0xc1, 0xd0, 0x74, 0x33, 0x24, 0x60,

    /* U+0079 "y" */
    0x90, 0xc6, 0x18, 0x32, 0x1, 0x70, 0xe, 0x0,
    0xc0, 0xc, 0x7, 0x0,

    /* U+007A "z" */
    0x7f, 0x40, 0xc0, 0xc0, 0x60, 0x30, 0x2f, 0xd0,

    /* U+007B "{" */
    0x18, 0x20, 0x20, 0x20, 0x20, 0xa0, 0x20, 0x20,
    0x20, 0x20, 0x18,

    /* U+007C "|" */
    0xaa, 0xaa, 0xaa,

    /* U+007D "}" */
    0x60, 0x82, 0x8, 0x20, 0x72, 0x8, 0x20, 0x86,
    0x0,

    /* U+007E "~" */
    0x28, 0x40, 0x34
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 36, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 0, .adv_w = 52, .box_w = 3, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 6, .adv_w = 76, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 10, .adv_w = 89, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 22, .adv_w = 89, .box_w = 5, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 35, .adv_w = 147, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 53, .adv_w = 109, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 67, .adv_w = 44, .box_w = 2, .box_h = 4, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 69, .adv_w = 54, .box_w = 3, .box_h = 12, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 78, .adv_w = 54, .box_w = 3, .box_h = 12, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 87, .adv_w = 75, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 92, .adv_w = 89, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 101, .adv_w = 44, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 105, .adv_w = 56, .box_w = 4, .box_h = 1, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 106, .adv_w = 44, .box_w = 3, .box_h = 2, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 108, .adv_w = 63, .box_w = 4, .box_h = 11, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 119, .adv_w = 89, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 131, .adv_w = 89, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 141, .adv_w = 89, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 153, .adv_w = 89, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 163, .adv_w = 89, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 175, .adv_w = 89, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 187, .adv_w = 89, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 199, .adv_w = 89, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 211, .adv_w = 89, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 223, .adv_w = 89, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 235, .adv_w = 44, .box_w = 3, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 240, .adv_w = 44, .box_w = 3, .box_h = 9, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 247, .adv_w = 89, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 256, .adv_w = 89, .box_w = 6, .box_h = 3, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 261, .adv_w = 89, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 270, .adv_w = 76, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 280, .adv_w = 151, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 303, .adv_w = 97, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 317, .adv_w = 105, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 329, .adv_w = 102, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 343, .adv_w = 110, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 355, .adv_w = 94, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 365, .adv_w = 88, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 375, .adv_w = 110, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 389, .adv_w = 116, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 401, .adv_w = 47, .box_w = 1, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 403, .adv_w = 86, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 413, .adv_w = 103, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 425, .adv_w = 87, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 435, .adv_w = 130, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 449, .adv_w = 116, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 461, .adv_w = 119, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 475, .adv_w = 101, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 485, .adv_w = 119, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 503, .adv_w = 102, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 515, .adv_w = 95, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 527, .adv_w = 96, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 539, .adv_w = 115, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 553, .adv_w = 92, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 565, .adv_w = 140, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 583, .adv_w = 92, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 595, .adv_w = 85, .box_w = 7, .box_h = 8, .ofs_x = -1, .ofs_y = 0},
    {.bitmap_index = 609, .adv_w = 96, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 621, .adv_w = 54, .box_w = 3, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 630, .adv_w = 63, .box_w = 4, .box_h = 11, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 641, .adv_w = 54, .box_w = 3, .box_h = 11, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 650, .adv_w = 89, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 657, .adv_w = 89, .box_w = 6, .box_h = 1, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 659, .adv_w = 97, .box_w = 3, .box_h = 4, .ofs_x = 1, .ofs_y = 6},
    {.bitmap_index = 662, .adv_w = 90, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 670, .adv_w = 99, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 682, .adv_w = 82, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 690, .adv_w = 99, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 702, .adv_w = 89, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 711, .adv_w = 52, .box_w = 4, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 720, .adv_w = 90, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 732, .adv_w = 97, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 744, .adv_w = 44, .box_w = 2, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 748, .adv_w = 44, .box_w = 3, .box_h = 10, .ofs_x = -1, .ofs_y = -2},
    {.bitmap_index = 756, .adv_w = 88, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 768, .adv_w = 45, .box_w = 3, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 774, .adv_w = 148, .box_w = 9, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 788, .adv_w = 98, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 797, .adv_w = 97, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 806, .adv_w = 99, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 818, .adv_w = 99, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 830, .adv_w = 62, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 836, .adv_w = 75, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 844, .adv_w = 60, .box_w = 4, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 852, .adv_w = 97, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 861, .adv_w = 83, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 870, .adv_w = 128, .box_w = 8, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 882, .adv_w = 80, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 890, .adv_w = 83, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 902, .adv_w = 76, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 910, .adv_w = 54, .box_w = 4, .box_h = 11, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 921, .adv_w = 43, .box_w = 1, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 924, .adv_w = 54, .box_w = 3, .box_h = 11, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 933, .adv_w = 89, .box_w = 6, .box_h = 2, .ofs_x = 0, .ofs_y = 3}
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
const lv_font_t fmo_grid_10 = {
#else
lv_font_t fmo_grid_10 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 13,          /*The maximum line height required by the font*/
    .base_line = 3,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if FMO_GRID_10*/
