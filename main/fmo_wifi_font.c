#include "fmo_wifi_font.h"
#include <string.h>

LV_FONT_DECLARE(fmo_channel_font);

/* Reuse every original glyph instead of storing a second CJK font in Flash.
 * Font callbacks run in the single LVGL software draw context. The bounded
 * scratch buffer is used only while converting one glyph, never returned. */
#define SOURCE_SIDE 32
static uint8_t source_pixels[SOURCE_SIDE * SOURCE_SIDE + LV_DRAW_BUF_ALIGN];

static int scale(int value)
{
    return value < 0 ? -((-value * 7 + 4) / 8) : (value * 7 + 4) / 8;
}

static bool glyph_dsc(const lv_font_t *font, lv_font_glyph_dsc_t *out,
                      uint32_t letter, uint32_t next)
{
    (void)font;
    if (!lv_font_get_glyph_dsc(&fmo_channel_font, out, letter, next) || out->is_placeholder)
        return false;
    out->adv_w = scale(out->adv_w);
    out->box_w = scale(out->box_w);
    out->box_h = scale(out->box_h);
    out->ofs_x = scale(out->ofs_x);
    out->ofs_y = scale(out->ofs_y);
    out->stride = 0;
    out->format = LV_FONT_GLYPH_FORMAT_A8;
    out->gid.index = letter; // Resolve the original fallback font again in the bitmap callback.
    return true;
}

static const void *glyph_bitmap(lv_font_glyph_dsc_t *glyph, lv_draw_buf_t *out)
{
    if (!out || !glyph->box_w || !glyph->box_h) return NULL;
    lv_font_glyph_dsc_t original;
    if (!lv_font_get_glyph_dsc(&fmo_channel_font, &original, glyph->gid.index, 0) ||
        original.is_placeholder || original.box_w > SOURCE_SIDE || original.box_h > SOURCE_SIDE)
        return NULL;
    lv_draw_buf_t source;
    void *pixels = lv_draw_buf_align(source_pixels, LV_COLOR_FORMAT_A8);
    if (lv_draw_buf_init(&source, original.box_w, original.box_h, LV_COLOR_FORMAT_A8,
                        LV_STRIDE_AUTO, pixels, SOURCE_SIDE * SOURCE_SIDE) != LV_RESULT_OK)
        return NULL;
    if (!lv_font_get_glyph_bitmap(&original, &source)) return NULL;
    /* Area averaging retains antialiased strokes when reducing 16 px to 14 px. */
    unsigned width = glyph->box_w, height = glyph->box_h;
    unsigned sw = original.box_w, sh = original.box_h;
    memset(out->data, 0, out->header.stride * height);
    for (unsigned y = 0; y < height; ++y) {
        unsigned y0 = y * sh, y1 = (y + 1) * sh;
        for (unsigned x = 0; x < width; ++x) {
            unsigned x0 = x * sw, x1 = (x + 1) * sw, sum = 0;
            for (unsigned sy = y0 / height; sy * height < y1; ++sy) {
                unsigned lo_y = sy * height > y0 ? sy * height : y0;
                unsigned hi_y = (sy + 1) * height < y1 ? (sy + 1) * height : y1;
                for (unsigned sx = x0 / width; sx * width < x1; ++sx) {
                    unsigned lo_x = sx * width > x0 ? sx * width : x0;
                    unsigned hi_x = (sx + 1) * width < x1 ? (sx + 1) * width : x1;
                    sum += source.data[sy * source.header.stride + sx] * (hi_x - lo_x) * (hi_y - lo_y);
                }
            }
            out->data[y * out->header.stride + x] = (sum + sw * sh / 2) / (sw * sh);
        }
    }
    lv_draw_buf_flush_cache(out, NULL);
    return out;
}

const lv_font_t *fmo_wifi_font(void)
{
    static lv_font_t font = {.get_glyph_dsc=glyph_dsc, .get_glyph_bitmap=glyph_bitmap};
    if (!font.line_height) {
        font.line_height = scale(fmo_channel_font.line_height);
        font.base_line = scale(fmo_channel_font.base_line);
    }
    return &font;
}
