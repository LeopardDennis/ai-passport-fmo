/* Native LVGL smoke test and RGB565 framebuffer capture. Uses firmware UI. */
#include "fmo_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
LV_FONT_DECLARE(fmo_channel_font);
LV_FONT_DECLARE(fmo_callsign_bold_20);
LV_FONT_DECLARE(fmo_callsign_bold_32);
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
    int first = 94, last = 61;
    /* Black glyphs on the solid orange channel strip; inspect rendered ink,
     * not label geometry (font ascent padding can hide misalignment). */
    for (int y = 62; y < 94; ++y)
        for (int x = 20; x < 220; ++x)
            if (pixels[y * 240 + x] == 0) {
                if (y < first) first = y;
                if (y > last) last = y;
            }
    assert(first <= last);
    assert(abs((first - 62) - (93 - last)) <= 2);
}

static void verify_connection_gap(void)
{
    int first = 56, last = 31;
    for (int y = 32; y < 56; ++y)
        for (int x = 12; x < 228; ++x)
            if (pixels[y * 240 + x] != 0) {
                if (y < first) first = y;
                if (y > last) last = y;
            }
    assert(first <= last);
    assert(62 - last - 1 >= 10); /* Visible gap before the channel strip. */
}

static void verify_callsign_center(int top)
{
    int first = top + 36, last = top - 1;
    for (int y = top; y < top + 36; ++y)
        for (int x = 12; x < 228; ++x)
            if (pixels[y * 240 + x] != 0) {
                if (y < first) first = y;
                if (y > last) last = y;
            }
    assert(first <= last);
    assert(abs((first - top) - (top + 35 - last)) <= 2);
}
static void verify_clock_center(void)
{
    int first = 180, last = 79;
    for (int y = 0; y < 32; ++y)
        for (int x = 80; x < 180; ++x)
            if (pixels[y * 240 + x] != 0) {
                if (x < first) first = x;
                if (x > last) last = x;
            }
    assert(first <= last);
    assert(abs(first + last - 239) <= 2); /* Visible ink at screen center. */
}

int main(int argc, char **argv)
{
    lv_init();
    lv_font_glyph_dsc_t glyph;
    // Verify every existing CJK codepoint, including the compact font's fallback segment.
    for (uint32_t code = 0x4e00; code < 0x9ff0; ++code)
        assert(lv_font_get_glyph_dsc(&fmo_channel_font, &glyph, code, 0) && !glyph.is_placeholder);
    assert(lv_font_get_glyph_dsc(&fmo_channel_font, &glyph, 0x5409, 0) && !glyph.is_placeholder);
    assert(lv_font_get_glyph_dsc(&fmo_channel_font, &glyph, 0x7EE7, 0) && !glyph.is_placeholder);
    lv_display_t *display = lv_display_create(240, 320);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, buffer, NULL, sizeof(buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    fmo_ui_create();
    assert(find_text(lv_screen_active(),"--:--"));
    lv_refr_now(display);
    verify_clock_center();
    fmo_ui_set_clock(INT64_C(1767272280)); /* 2026-01-01 20:58 Beijing */
    assert(find_text(lv_screen_active(),"20:58"));
    fmo_ui_set_clock(INT64_C(1767272340));
    assert(find_text(lv_screen_active(),"20:59"));
    fmo_ui_set_clock(0);
    assert(find_text(lv_screen_active(),"--:--"));
    lv_refr_now(display);
    verify_clock_center();
    fmo_ui_set_clock(INT64_C(1767272280));
    fmo_controls_t controls = {.audio_enabled=true,.volume=50};
    fmo_monitor_state_t state = {.wifi_connected=true, .events_connected=true,
        .control_connected=true, .channel_valid=true, .speaking=true,
        .channel_uid=42, .speaker="BG5ESN", .last_speaker="BG5ESN", .grid="PM01"};
    state.radio = (fmo_radio_profile_t){.device_name="QUANSHENG",.antenna="示例GP",
        .frequency_100hz=4398750,.antenna_height_m=63,.height_valid=true};
    fmo_monitor_set_channel(&state, 42, "安吉FMO中继");
    fmo_ui_render(&state, "", "", "", 82, 12000, false, &controls);
    assert(find_text(lv_screen_active(), "音频:开50%  长按OK:配网"));
    controls.audio_enabled=false;
    fmo_ui_render(&state,"","","",82,12000,false,&controls);
    assert(find_text(lv_screen_active(), "音频:关50%  长按OK:配网"));
    controls.volume=100; controls.audio_enabled=true;
    fmo_ui_render(&state,"","","",82,12000,true,&controls);
    assert(find_text(lv_screen_active(), "音频:开100%  长按OK:配网"));
    lv_obj_t *refresh_link = find_text(lv_screen_active(), "FMO 已连接 已请求刷新");
    assert(refresh_link);
    lv_point_t hint_size;
    lv_text_get_size(&hint_size, "音频:开100%  长按OK:配网", &fmo_channel_font,
                     0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    assert(hint_size.x <= 216);
    lv_text_get_size(&hint_size, lv_label_get_text(refresh_link), &fmo_channel_font,
                     0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    assert(hint_size.x <= 216);
    controls.volume=50;
    fmo_ui_render(&state,"","","",82,12000,false,&controls);
    assert(find_text(lv_screen_active(), "安吉FMO中继"));
    verify("BG5ESN", 0xFF8A00);
    assert(find_text(lv_screen_active(), "QUANSHENG"));
    assert(find_text(lv_screen_active(), "439.8750 MHz"));
    assert(find_text(lv_screen_active(), "示例GP"));
    assert(find_text(lv_screen_active(), "高度: 63 m"));
    state.radio.antenna_height_m=0;
    fmo_ui_render(&state,"","","",82,12000,false,&controls);
    assert(find_text(lv_screen_active(),"高度: 0 m"));
    state.radio.antenna_height_m=63;
    assert(lv_obj_get_style_text_font(find_text(lv_screen_active(), "BG5ESN"), 0) == &fmo_callsign_bold_32);
    assert(!find_text(lv_screen_active(), "PM01"));
    assert(!find_text(lv_screen_active(), "电台 / PM01"));
    state.speaker_is_host = true;
    fmo_ui_render(&state, "", "", "", 82, 12000, false, &controls);
    assert(!find_text(lv_screen_active(), "PM01"));
    assert(!find_text(lv_screen_active(), "本机 / PM01"));
    fmo_monitor_apply_speaker(&state, "", "", false, false, 12000);
    fmo_ui_render(&state, "", "", "", -1, 12000, false, &controls);
    verify("BG5ESN", 0xF4F4F4);
    assert(find_text(lv_screen_active(), "上次通联"));
    assert(find_text(lv_screen_active(), "安吉FMO中继"));
    assert(!find_text(lv_screen_active(), "等待频道同步"));
    assert(!find_text(lv_screen_active(), "PM01"));
    assert(!find_text(lv_screen_active(), "PM01 / 0 秒前"));
    fmo_ui_render(&state, "", "", "", -1, 60000, false, &controls);
    assert(!find_text(lv_screen_active(), "PM01"));
    assert(!find_text(lv_screen_active(), "PM01 / 48 秒前"));
    assert(lv_obj_get_style_text_font(find_text(lv_screen_active(), "BG5ESN"), 0) == &fmo_callsign_bold_32);
    assert(find_text(lv_screen_active(), "--"));
    assert(!find_text(lv_screen_active(), "BAT --"));
    fmo_ui_render(&state, "", "", "", 15, 12000, false, &controls);
    verify("15", 0xF4F4F4);
    fmo_ui_render(&state, "", "", "", 100, 12000, false, &controls);
    verify("100", 0x000000);
    lv_obj_t *percent = find_text(lv_screen_active(), "100");
    lv_point_t number_size;
    lv_text_get_size(&number_size, "100", lv_obj_get_style_text_font(percent, 0),
                     0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    assert(number_size.x <= 30); /* All three digits fit the compact battery. */
    /* Reclaim the empty detail row, but restore it for genuine idle/errors.
     * Switching between them must keep the profile/status/callsign separate. */
    lv_obj_update_layout(lv_screen_active());
    lv_refr_now(display);
    verify_callsign_center(245);
    int compact_call_y = lv_obj_get_y(find_text(lv_screen_active(), "BG5ESN"));
    state.last_speaker[0] = 0;
    fmo_ui_render(&state, "", "", "", 82, 12000, false, &controls);
    lv_obj_update_layout(lv_screen_active());
    lv_refr_now(display);
    verify_callsign_center(221);
    lv_obj_t *idle_detail = find_text(lv_screen_active(), "等待电台发言");
    assert(idle_detail && !lv_obj_has_flag(idle_detail, LV_OBJ_FLAG_HIDDEN));
    strcpy(state.last_speaker, "BG5ESN");
    fmo_ui_render(&state, "", "", "", 82, 12000, false, &controls);
    lv_obj_update_layout(lv_screen_active());
    assert(lv_obj_has_flag(idle_detail, LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_get_y(find_text(lv_screen_active(), "BG5ESN")) == compact_call_y);
    state.events_connected = false;
    fmo_ui_render(&state, "", "", "", 82, 12000, false, &controls);
    assert(!find_text(lv_screen_active(), "BG5ESN"));
    fmo_controls_observe_setup(&controls, true);
    controls.setup_info = true;
    fmo_ui_render(&state, "", "FMO-Setup-TEST", "ABCDEF012345", 82, 12000, false, &controls);
    verify("ABCDEF012345", 0xF4F4F4);
    assert(find_text(lv_screen_active(), "192.168.9.1"));
    /* Repeated view changes must release the QR canvas and overlay labels. */
    for (unsigned i = 0; i < 20; ++i) {
        controls.setup_info = false;
        fmo_ui_render(&state, "", "FMO-Setup-TEST", "ABCDEF012345", 82, 12000, false, &controls);
        lv_refr_now(display);
        assert(find_text(lv_screen_active(), "扫码连接配网热点"));
        controls.setup_info = true;
        fmo_ui_render(&state, "", "FMO-Setup-TEST", "ABCDEF012345", 82, 12000, false, &controls);
        fmo_controls_observe_setup(&controls, false);
        fmo_ui_render(&state, "", "", "", 82, 12000, false, &controls);
        fmo_controls_observe_setup(&controls, true);
    }
    fmo_controls_observe_setup(&controls, false);
    const char *ssid = "", *password = "", *error = "";
    state.events_connected = true;
    if (argc < 2 || !strcmp(argv[1], "onair") || !strcmp(argv[1], "muted"))
        fmo_monitor_apply_speaker(&state, "BG5ESN", "PM01", true, false, 12000);
    if (argc > 1 && (!strcmp(argv[1], "setup") || !strcmp(argv[1], "setup_info"))) {
        ssid="FMO-Setup-TEST"; password="ABCDEF012345";
        fmo_controls_observe_setup(&controls, true);
        controls.setup_info = !strcmp(argv[1], "setup_info");
    }
    if (argc > 1 && !strcmp(argv[1], "network")) {
        controls.view = FMO_VIEW_NETWORK;
    }
    if (argc > 1 && !strcmp(argv[1], "offline")) state.wifi_connected = state.channel_valid = false;
    if (argc > 1 && !strcmp(argv[1], "long")) {
        strcpy(state.radio.device_name,"QUANSHENG-1234567890");
        strcpy(state.radio.antenna,"长型号测试天线名称玻璃钢GP");
    }
    if (argc > 1 && !strcmp(argv[1], "rare")) fmo_monitor_set_channel(&state, 42, "龍龠龯");
    if (argc > 1 && !strcmp(argv[1], "long")) strcpy(state.last_speaker, "BG5ESN/12345678");
    if (argc > 1 && !strcmp(argv[1], "muted")) controls.audio_enabled=false;
    if (argc > 1 && !strcmp(argv[1], "idle")) state.last_speaker[0]=0;
    if (argc > 1 && !strcmp(argv[1], "error")) error="NETWORK START FAILED";
    fmo_ui_render(&state, error, ssid, password, 82, 12000, false, &controls);
    if (argc > 1 && !strcmp(argv[1], "offline")) {
        assert(!find_text(lv_screen_active(),"QUANSHENG"));
        assert(find_text(lv_screen_active(),"-- MHz"));
        assert(find_text(lv_screen_active(),"高度: -- m"));
    }
    if (argc > 1 && !strcmp(argv[1], "long"))
        assert(lv_obj_get_style_text_font(find_text(lv_screen_active(), state.last_speaker), 0) == &fmo_callsign_bold_20);
    lv_obj_update_layout(lv_screen_active());
    lv_refr_now(display);
    verify_clock_center();
    if (controls.view == FMO_VIEW_MONITOR) {
        verify_channel_center();
        verify_connection_gap();
        verify_callsign_center(error[0] || !state.last_speaker[0] ? 221 : 245);
    }
    if (controls.view == FMO_VIEW_MONITOR && !ssid[0]) {
        const char *footer = controls.audio_enabled ? "音频:开50%  长按OK:配网" : "音频:关50%  长按OK:配网";
        lv_obj_t *hint = find_text(lv_screen_active(), footer);
        assert(hint);
        lv_point_t size;
        lv_text_get_size(&size, footer, lv_obj_get_style_text_font(hint, 0),
                         0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        assert(size.x <= 216); /* Sound and setup hints must fit together. */
    }
    lv_mem_monitor_t memory;
    lv_mem_monitor(&memory);
    if (memory.free_biggest_size < 4096)
        fprintf(stderr,"LVGL: peak=%u free=%u largest=%u\n",(unsigned)memory.max_used,(unsigned)memory.free_size,(unsigned)memory.free_biggest_size);
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
