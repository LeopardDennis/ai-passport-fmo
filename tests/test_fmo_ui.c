/* Native LVGL smoke test and RGB565 framebuffer capture. Uses firmware UI. */
#include "fmo_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
LV_FONT_DECLARE(fmo_channel_font);
LV_FONT_DECLARE(fmo_callsign_bold_14);
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
        if (lv_obj_check_type(child, &lv_label_class) && !strcmp(lv_label_get_text(child), text) &&
            lv_obj_get_style_text_font(child,0)!=&fmo_callsign_bold_14) return child;
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

static char drawn_dates[FMO_HISTORY_COUNT][20];
static unsigned drawn_count;
static void observe_qso_draw(lv_event_t *event)
{
    lv_draw_label_dsc_t *dsc = lv_draw_task_get_label_dsc(lv_event_get_draw_task(event));
    if (!dsc || !dsc->text || strlen(dsc->text) != 19) return;
    assert(dsc->font == &lv_font_montserrat_14); /* Time remains regular weight. */
    assert(lv_color_eq(dsc->color,lv_color_hex(0x929292)));
    lv_point_t size;
    lv_text_get_size(&size,dsc->text,dsc->font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    assert(size.x<=196);
    assert(dsc->base.id1<FMO_HISTORY_COUNT);
    memcpy(drawn_dates[dsc->base.id1],dsc->text,20);
    drawn_count |= 1u << dsc->base.id1;
}
static void verify_dates(lv_display_t *display, const char *first, const char *last)
{
    lv_obj_t *panel=lv_obj_get_parent(find_text(lv_screen_active(),"QSO"));
    drawn_count=0;
    lv_obj_add_flag(panel,LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
    lv_obj_add_event_cb(panel,observe_qso_draw,LV_EVENT_DRAW_TASK_ADDED,NULL);
    lv_obj_invalidate(panel);
    lv_refr_now(display);
    lv_obj_remove_event_cb(panel,observe_qso_draw);
    lv_obj_remove_flag(panel,LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
    assert(drawn_count==((1u << FMO_HISTORY_COUNT)-1));
    assert(!strcmp(drawn_dates[0],first));
    assert(!strcmp(drawn_dates[FMO_HISTORY_COUNT-1],last));
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
    int first = top + 32, last = top - 1;
    for (int y = top; y < top + 32; ++y)
        for (int x = 12; x < 228; ++x)
            if (pixels[y * 240 + x] != 0) {
                if (y < first) first = y;
                if (y > last) last = y;
            }
    assert(first <= last);
    assert(abs((first - top) - (top + 31 - last)) <= 2);
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

static int audio_bar_width(lv_display_t *display)
{
    lv_refr_now(display);
    int count = 0;
    for (int y = 116; y < 160; ++y) {
        for (int x = 0; x < 240; ++x) {
            if (pixels[y * 240 + x] == 0xFE80) { /* Yellow PCM bar, RGB565. */
                assert(x >= 12 && x < 228);
                ++count;
            }
        }
    }
    assert(count % 2 == 0);
    int width = count / 2;
    return width;
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
    state.history = (fmo_history_t){.count=FMO_HISTORY_COUNT,.entries={
        {.callsign="BG5ESN",.timestamp=1767272220},
        {.callsign="BI1XYZ",.timestamp=1767272160},
        {.callsign="BG1ABC",.timestamp=1767272100}}};
    fmo_monitor_set_channel(&state, 42, "安吉FMO中继");
    fmo_ui_render(&state, "", "", "", 82, 12000, false, &controls);
    assert(find_text(lv_screen_active(), "正在通联"));
    assert(find_text(lv_screen_active(), "音频: 50%  长按OK: 配网"));
    /* Measure real rendered pixels: speech metadata alone must not light the
     * bar. PCM animates width and silence erases the old, longer rectangle. */
    assert(audio_bar_width(display) == 0);
    fmo_ui_set_audio_level(100, 12000);
    fmo_ui_set_audio_level(100, 12050);
    int attack_width = audio_bar_width(display);
    assert(attack_width > 0 && attack_width < 216);
    fmo_ui_set_audio_level(100, 12100);
    assert(audio_bar_width(display) == 216);
    fmo_ui_set_audio_level(0, 12200);
    assert(audio_bar_width(display) == 173);
    fmo_ui_set_audio_level(0, 12600);
    assert(audio_bar_width(display) == 0);
    fmo_ui_set_audio_level(50, 12680);
    assert(audio_bar_width(display) == 108);
    controls.audio_enabled=false;
    fmo_ui_render(&state,"","","",82,12000,false,&controls);
    assert(find_text(lv_screen_active(), "音频: 0%  长按OK: 配网"));
    fmo_ui_set_audio_level(100, 12780);
    assert(audio_bar_width(display) == 0);
    controls.audio_enabled=true; controls.volume=0;
    fmo_ui_render(&state,"","","",82,12800,false,&controls);
    fmo_ui_set_audio_level(100, 12880);
    assert(audio_bar_width(display) == 0);
    controls.volume=100; controls.audio_enabled=true;
    fmo_ui_render(&state,"","","",82,12000,true,&controls);
    assert(find_text(lv_screen_active(), "音频: 100%  长按OK: 配网"));
    lv_obj_t *refresh_link = find_text(lv_screen_active(), "FMO 已连接 已请求刷新");
    assert(refresh_link);
    lv_point_t hint_size;
    lv_text_get_size(&hint_size, "音频: 100%  长按OK: 配网", &fmo_channel_font,
                     0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    assert(hint_size.x <= 216);
    lv_text_get_size(&hint_size, lv_label_get_text(refresh_link), &fmo_channel_font,
                     0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    assert(hint_size.x <= 216);
    controls.volume=50;
    fmo_ui_render(&state,"","","",82,12000,false,&controls);
    assert(find_text(lv_screen_active(), "安吉FMO中继"));
    verify("BG5ESN", 0xFF8A00);
    state.speaker_cross_server=true;
    fmo_ui_render(&state,"","","",82,12000,false,&controls);
    verify("BG5ESN",0xF66969);
    verify("PM01",0x929292);
    state.speaker_cross_server=false;
    fmo_ui_render(&state,"","","",82,12000,false,&controls);
    verify("BG5ESN",0xFF8A00);
    assert(find_text(lv_screen_active(), "QSO"));
    verify_dates(display,"2026-01-01 20:57:00","2026-01-01 20:55:00");
    lv_obj_t *history_panel=lv_obj_get_parent(find_text(lv_screen_active(),"QSO"));
    lv_obj_t *history_call=lv_obj_get_child(history_panel,1);
    assert(!strcmp(lv_label_get_text(history_call),"BG5ESN"));
    assert(lv_obj_get_style_text_font(history_call,0)==&fmo_callsign_bold_14);
    assert(lv_color_eq(lv_obj_get_style_text_color(history_call,0),lv_color_hex(0xF4F4F4)));
    state.channel_valid=false;
    state.control_connected=false;
    fmo_ui_render(&state,"","","",82,12000,false,&controls);
    assert(find_text(lv_screen_active(),"频道确认中"));
    assert(find_text(lv_screen_active(),"等待频道确认"));
    assert(!find_text(lv_screen_active(),"安吉FMO中继"));
    assert(find_text(lv_screen_active(),"正在通联"));
    verify("BG5ESN",0xFF8A00);verify("PM01",0x929292);
    fmo_ui_set_audio_level(60,60000);fmo_ui_set_audio_level(60,60100);
    assert(audio_bar_width(display)>0);
    verify_dates(display,"2026-01-01 20:57:00","2026-01-01 20:55:00");
    state.control_connected=true;
    state.channel_valid=true;
    fmo_ui_render(&state,"","","",82,12000,false,&controls);
    verify("PM01",0x929292);
    lv_obj_update_layout(lv_screen_active());
    lv_obj_t *live_call=find_text(lv_screen_active(),"BG5ESN");
    lv_obj_t *live_grid=find_text(lv_screen_active(),"PM01");
    assert(lv_obj_get_style_text_font(live_grid,0)==&fmo_channel_font);
    assert(lv_obj_get_x(live_grid)>=lv_obj_get_x(live_call)+lv_obj_get_width(live_call)+8);
    assert(lv_obj_get_x(live_grid)+lv_obj_get_width(live_grid)<=228);
    assert(lv_obj_get_style_text_font(find_text(lv_screen_active(), "BG5ESN"), 0) == &fmo_callsign_bold_32);
    assert(find_text(lv_screen_active(), "PM01"));
    assert(!find_text(lv_screen_active(), "电台 / PM01"));
    state.speaker_is_host = true;
    state.speaker_cross_server = true;
    fmo_ui_render(&state, "", "", "", 82, 12000, false, &controls);
    assert(find_text(lv_screen_active(), "PM01"));
    assert(!find_text(lv_screen_active(), "本机 / PM01"));
    fmo_monitor_apply_speaker(&state, "", "", false, false, false, 12000);
    fmo_ui_render(&state, "", "", "", -1, 12000, false, &controls);
    verify("BG5ESN", 0xF4F4F4);
    assert(find_text(lv_screen_active(), "上次通联"));
    assert(find_text(lv_screen_active(), "安吉FMO中继"));
    assert(!find_text(lv_screen_active(), "等待频道同步"));
    verify("PM01", 0x929292);
    assert(!find_text(lv_screen_active(), "PM01 / 0 秒前"));
    fmo_ui_render(&state, "", "", "", -1, 72000, false, &controls);
    verify("PM01", 0x929292); /* Retained beyond the former 30-second age. */
    fmo_monitor_state_t last_contact = state;
    fmo_monitor_apply_speaker(&state, "BI1XYZ", "ON80", true, false, false, 73000);
    fmo_ui_render(&state, "", "", "", -1, 73000, false, &controls);
    verify("ON80", 0x929292);
    assert(!find_text(lv_screen_active(), "PM01"));
    fmo_monitor_apply_speaker(&state, "BG1ABC", "", true, false, false, 74000);
    fmo_ui_render(&state, "", "", "", -1, 74000, false, &controls);
    assert(!find_text(lv_screen_active(), "ON80"));
    assert(!find_text(lv_screen_active(), "PM01"));
    state = last_contact;
    fmo_ui_render(&state, "", "", "", -1, 12000, false, &controls);
    /* Audio may finish after the PTT-end event. Preserve its smooth tail. */
    fmo_ui_set_audio_level(100, 20000);
    fmo_ui_set_audio_level(100, 20080);
    assert(audio_bar_width(display) == 216);
    fmo_ui_set_audio_level(0, 20180);
    assert(audio_bar_width(display) == 173);
    fmo_ui_set_audio_level(0, 20580);
    assert(audio_bar_width(display) == 0);
    fmo_ui_render(&state, "", "", "", -1, 60000, false, &controls);
    verify("PM01", 0x929292);
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
    /* Empty profile, initial idle and populated speech share fixed rows. */
    lv_obj_update_layout(lv_screen_active());
    lv_refr_now(display);
    verify_callsign_center(116);
    lv_obj_t *callsign = find_text(lv_screen_active(), "BG5ESN");
    int call_y = lv_obj_get_y(callsign);
    int status_y = lv_obj_get_y(find_text(lv_screen_active(), "上次通联"));
    lv_obj_t *profile = lv_obj_get_parent(find_text(lv_screen_active(), "QSO"));
    int profile_y = lv_obj_get_y(profile), profile_h = lv_obj_get_height(profile);
    fmo_history_t saved_history = state.history;
    memset(&state.history, 0, sizeof(state.history));
    state.last_speaker[0] = 0;
    fmo_ui_render(&state, "", "", "", 82, 12000, false, &controls);
    lv_obj_update_layout(lv_screen_active());
    lv_refr_now(display);
    verify_callsign_center(116);
    lv_obj_t *idle_status = find_text(lv_screen_active(), "等待电台发言");
    assert(idle_status && lv_obj_get_y(idle_status) == status_y);
    assert(lv_obj_get_y(profile) == profile_y && lv_obj_get_height(profile) == profile_h);
    assert(!strcmp(lv_label_get_text(callsign), "--"));
    state.history = saved_history;
    strcpy(state.last_speaker, "BG5ESN");
    fmo_ui_render(&state, "", "", "", 82, 12000, false, &controls);
    lv_obj_update_layout(lv_screen_active());
    assert(find_text(lv_screen_active(), "上次通联"));
    assert(!find_text(lv_screen_active(), "等待电台发言"));
    assert(lv_obj_get_y(find_text(lv_screen_active(), "BG5ESN")) == call_y);
    assert(lv_obj_get_y(profile) == profile_y && lv_obj_get_height(profile) == profile_h);
    state.events_connected = false;
    fmo_ui_render(&state, "", "", "", 82, 12000, false, &controls);
    assert(!find_text(lv_screen_active(), "BG5ESN"));
    // Losing event metadata clears the live call but does not suppress real PCM.
    fmo_ui_set_audio_level(100, 61000);fmo_ui_set_audio_level(100, 61100);
    assert(audio_bar_width(display) > 0);
    state.wifi_connected=false;
    fmo_ui_render(&state,"","","",82,12000,false,&controls);
    fmo_ui_set_audio_level(100,61200);
    assert(audio_bar_width(display)==0);
    state.wifi_connected=true;
    fmo_controls_observe_setup(&controls, true);
    controls.setup_info = true;
    fmo_ui_render(&state, "", "FMO-Setup-TEST", "ABCDEF012345", 82, 12000, false, &controls);
    verify("ABCDEF012345", 0xF4F4F4);
    assert(find_text(lv_screen_active(), "192.168.9.1"));
    /* Repeated view changes must release the QR canvas and overlay labels. */
    for (unsigned i = 0; i < 20; ++i) {
        controls.setup_info = false;
        fmo_ui_render(&state, "", "FMO-Setup-TEST", "ABCDEF012345", 82, 12000, false, &controls);
        fmo_ui_set_audio_level(100, 62000 + i * 50); /* Deleted monitor objects. */
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
    if (argc > 1 && !strcmp(argv[1],"cross")) {
        fmo_monitor_apply_speaker(&state,"BG5ESN","PM01",true,false,true,12000);
    }
    state.events_connected = true;
    bool meter_preview = argc > 1 && !strncmp(argv[1], "meter_", 6);
    if (argc < 2 || !strcmp(argv[1], "onair") || !strcmp(argv[1], "muted") || !strcmp(argv[1], "lan") || meter_preview)
        fmo_monitor_apply_speaker(&state, "BG5ESN", "PM01", true, false, false, 12000);
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
        strcpy(state.history.entries[0].callsign,"BG5ESN/12345678");
    }
    if (argc > 1 && !strcmp(argv[1], "rare")) fmo_monitor_set_channel(&state, 42, "龍龠龯");
    if (argc > 1 && !strcmp(argv[1], "long")) {
        fmo_monitor_apply_speaker(&state,"BG5ESN/12345678","PM01ABC1234",true,false, false,12000);
    }
    if (argc > 1 && !strcmp(argv[1], "muted")) controls.audio_enabled=false;
    if (argc > 1 && (!strcmp(argv[1], "idle") || !strcmp(argv[1], "empty"))) state.last_speaker[0]=0;
    if (argc > 1 && (!strcmp(argv[1], "empty") || !strcmp(argv[1], "lan"))) memset(&state.history, 0, sizeof(state.history));
    if (argc > 1 && !strcmp(argv[1], "error")) error="NETWORK START FAILED";
    fmo_ui_render(&state, error, ssid, password, 82, 12000, false, &controls);
    uint8_t preview_level = 0;
    if (meter_preview) preview_level = !strcmp(argv[1], "meter_low") ? 20 :
                                      !strcmp(argv[1], "meter_mid") ? 55 : 95;
    else if (state.speaking) preview_level = 65;
    fmo_ui_set_audio_level(preview_level, 70000);
    fmo_ui_set_audio_level(preview_level, 70100);
    if (argc > 1 && !strcmp(argv[1], "offline")) {
        verify_dates(display,"---------- --:--:--","---------- --:--:--");
    }
    if (argc > 1 && !strcmp(argv[1], "long"))
        assert(lv_obj_get_style_text_font(find_text(lv_screen_active(), state.last_speaker), 0) == &fmo_callsign_bold_20);
    lv_obj_update_layout(lv_screen_active());
    lv_refr_now(display);
    verify_clock_center();
    if (controls.view == FMO_VIEW_MONITOR) {
        verify_channel_center();
        verify_connection_gap();
        verify_callsign_center(116);
    }
    if (controls.view == FMO_VIEW_MONITOR && !ssid[0]) {
        char footer[80];
        snprintf(footer,sizeof(footer),"音频: %u%%  长按OK: 配网",
                 controls.audio_enabled ? controls.volume : 0);
        lv_obj_t *hint = find_text(lv_screen_active(), footer);
        assert(hint);
        lv_point_t size;
        lv_text_get_size(&size, footer, lv_obj_get_style_text_font(hint, 0),
                         0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        assert(size.x <= 216); /* Key actions must fit with no ellipsis. */
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
