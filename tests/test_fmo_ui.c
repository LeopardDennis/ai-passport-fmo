/* Native LVGL smoke test and RGB565 framebuffer capture. Uses firmware UI. */
#include "fmo_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
LV_FONT_DECLARE(fmo_channel_font);
static uint16_t pixels[240 * 320];
static uint8_t buffer[240 * 20 * 2];
static void flush(lv_display_t *display, const lv_area_t *a, uint8_t *p)
{
    for (int y = a->y1; y <= a->y2; ++y) {
        memcpy(pixels + y * 240 + a->x1, p, (a->x2 - a->x1 + 1) * 2);
        p += (a->x2 - a->x1 + 1) * 2;
    }
    lv_display_flush_ready(display);
}
static lv_obj_t *find_text(lv_obj_t *parent, const char *text)
{
    for (unsigned i = 0; i < lv_obj_get_child_count(parent); ++i) {
        lv_obj_t *child = lv_obj_get_child(parent, i);
        if (lv_obj_check_type(child, &lv_label_class) && !strcmp(lv_label_get_text(child), text)) return child;
        lv_obj_t *found = find_text(child, text);
        if (found) return found;
    }
    return NULL;
}
static void verify(const char *value, uint32_t color)
{
    lv_obj_t *obj = find_text(lv_screen_active(), value);
    assert(obj);
    assert(lv_color_eq(lv_obj_get_style_text_color(obj, 0), lv_color_hex(color)));
}

static void verify_channel_center(void)
{
    int first = 115, last = 78;
    /* Black glyphs on the solid orange channel strip; inspect rendered ink,
     * not label geometry (font ascent padding can hide misalignment). */
    for (int y = 79; y < 115; ++y)
        for (int x = 20; x < 220; ++x)
            if (pixels[y * 240 + x] == 0) {
                if (y < first) first = y;
                if (y > last) last = y;
            }
    assert(first <= last);
    assert(abs((first - 79) - (114 - last)) <= 2);
}

static void verify_callsign_center(void)
{
    int first = 204, last = 157;
    for (int y = 158; y < 204; ++y)
        for (int x = 12; x < 228; ++x)
            if (pixels[y * 240 + x] != 0) {
                if (y < first) first = y;
                if (y > last) last = y;
            }
    assert(first <= last);
    assert(abs((first - 158) - (203 - last)) <= 2);
}
int main(int argc, char **argv)
{
    lv_init();
    lv_font_glyph_dsc_t glyph;
    assert(lv_font_get_glyph_dsc(&fmo_channel_font, &glyph, 0x5409, 0) && !glyph.is_placeholder);
    assert(lv_font_get_glyph_dsc(&fmo_channel_font, &glyph, 0x7EE7, 0) && !glyph.is_placeholder);
    lv_display_t *display = lv_display_create(240, 320);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, buffer, NULL, sizeof(buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    fmo_ui_create();
    fmo_monitor_state_t state = {.wifi_connected=true, .events_connected=true,
        .control_connected=true, .channel_valid=true, .speaking=true,
        .channel_uid=42, .speaker="BG5ESN", .last_speaker="BG5ESN", .channel_name="安吉FMO中继"};
    fmo_ui_render(&state, "", "", "", 82, 12000, false);
    verify("BG5ESN", 0xFF8A00);
    state.speaking = false;
    fmo_ui_render(&state, "", "", "", -1, 12000, false);
    verify("BG5ESN", 0xF4F4F4);
    assert(find_text(lv_screen_active(), "--"));
    assert(!find_text(lv_screen_active(), "BAT --"));
    fmo_ui_render(&state, "", "", "", 15, 12000, false);
    verify("15", 0xF4F4F4);
    fmo_ui_render(&state, "", "", "", 100, 12000, false);
    verify("100", 0x000000);
    lv_obj_t *percent = find_text(lv_screen_active(), "100");
    lv_point_t number_size;
    lv_text_get_size(&number_size, "100", lv_obj_get_style_text_font(percent, 0),
                     0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    assert(number_size.x <= 30); /* All three digits fit the compact battery. */
    state.events_connected = false;
    fmo_ui_render(&state, "", "", "", 82, 12000, false);
    assert(!find_text(lv_screen_active(), "BG5ESN"));
    fmo_ui_render(&state, "", "FMO-Setup-TEST", "ABCDEF012345", 82, 12000, false);
    verify("ABCDEF012345", 0xF4F4F4);
    assert(find_text(lv_screen_active(), "打开 192.168.9.1"));
    const char *ssid = "", *password = "", *error = "";
    state.events_connected = true;
    state.speaking = argc < 2 || !strcmp(argv[1], "onair");
    if (argc > 1 && !strcmp(argv[1], "setup")) {ssid="FMO-Setup-TEST"; password="ABCDEF012345";}
    if (argc > 1 && !strcmp(argv[1], "offline")) state.wifi_connected = state.channel_valid = false;
    if (argc > 1 && !strcmp(argv[1], "long")) strcpy(state.last_speaker, "BG5ESN/12345678");
    if (argc > 1 && !strcmp(argv[1], "error")) error="NETWORK START FAILED";
    fmo_ui_render(&state, error, ssid, password, 82, 12000, false);
    lv_obj_update_layout(lv_screen_active());
    lv_refr_now(display);
    verify_channel_center();
    verify_callsign_center();
    lv_mem_monitor_t memory;
    lv_mem_monitor(&memory);
    assert(memory.free_biggest_size >= 4096); /* Leave room for later LVGL redraws. */
    if (argc > 2) {
        FILE *f = fopen(argv[2], "wb"); assert(f);
        fprintf(f, "P6\n240 320\n255\n");
        for (unsigned i=0;i<240*320;++i) {
            unsigned v=pixels[i];
            unsigned char rgb[3]={(v>>11)*255/31, ((v>>5)&63)*255/63, (v&31)*255/31};
            fwrite(rgb,1,3,f);
        }
        fclose(f);
    }
    printf("FMO UI: PASS (states, layout, battery, LVGL peak=%u free=%u)\n",
           (unsigned)memory.max_used, (unsigned)memory.free_size);
    return 0;
}
