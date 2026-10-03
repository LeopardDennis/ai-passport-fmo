#include "fmo_ui.h"
#include "fmo_clock.h"
#include "fmo_audio_meter.h"
#include "fmo_wifi_qr.h"
#include "src/misc/lv_text_private.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

/* Approximation of the supplied FMO reference, not an official color spec. */
#define BLACK 0x000000
#define ORANGE 0xFF8A00
#define AUDIO_LEVEL_COLOR 0xFFD000
#define WHITE 0xF4F4F4
#define MUTED 0x929292
#define LINE 0x303030
#define RED 0xFF5252
/* Fixed bands: unknown metadata and first idle use the same geometry as
 * live speech. Status/error text shares the air row instead of moving panels. */
#define LINK_TOP 32
#define LINK_HEIGHT 24
#define CHANNEL_TOP 62
#define CHANNEL_HEIGHT 32
#define RADIO_TOP 102
#define RADIO_ROW_HEIGHT 26
#define AIR_TOP 218
#define CALLSIGN_TOP 245
#define ACTIVITY_TOP 289
#define FOOTER_TOP 296
LV_FONT_DECLARE(fmo_channel_font);
LV_FONT_DECLARE(fmo_callsign_bold_20);
LV_FONT_DECLARE(fmo_callsign_bold_32);

static lv_obj_t *link_label, *battery_label, *channel_label, *clock_label;
static lv_obj_t *battery_body;
static int last_battery = -2;
static lv_obj_t *air_label, *callsign_label, *hint_label;
static lv_obj_t *radio_panel, *radio_labels[4];
static fmo_audio_meter_t audio_meter;
static bool audio_meter_enabled;
static int audio_bar_width;

static void text(lv_obj_t *label, const char *value)
{
    if (strcmp(lv_label_get_text(label), value)) lv_label_set_text(label, value);
}

static void color(lv_obj_t *obj, uint32_t value)
{
    if (!lv_color_eq(lv_obj_get_style_text_color(obj, 0), lv_color_hex(value)))
        lv_obj_set_style_text_color(obj, lv_color_hex(value), 0);
}

/* Center the union of visible glyph boxes, not the font's ascent/descent
 * padding. LVGL's UTF-8 decoder matches the pinned renderer's behavior. */
static void center_ink(lv_obj_t *obj, int top, int height, uint32_t reference)
{
    const lv_font_t *font = lv_obj_get_style_text_font(obj, 0);
    lv_font_glyph_dsc_t glyph;
    const char *value = lv_label_get_text(obj);
    int ink_top = INT32_MAX, ink_bottom = INT32_MIN;
    uint32_t index = 0;
    while (value[index]) {
        uint32_t letter = lv_text_encoded_next(value, &index);
        if (!lv_font_get_glyph_dsc(font, &glyph, letter, 0) || !glyph.box_h || !glyph.box_w) continue;
        int y = font->line_height - font->base_line - glyph.box_h - glyph.ofs_y;
        if (y < ink_top) ink_top = y;
        if (y + glyph.box_h > ink_bottom) ink_bottom = y + glyph.box_h;
    }
    if (ink_top == INT32_MAX) {
        if (!lv_font_get_glyph_dsc(font, &glyph, reference, 0) || !glyph.box_h) return;
        ink_top = font->line_height - font->base_line - glyph.box_h - glyph.ofs_y;
        ink_bottom = ink_top + glyph.box_h;
    }
    int y = top + (height - (ink_bottom - ink_top)) / 2 - ink_top;
    /* Coordinates can be stale until the next layout pass. Compare the
     * requested style position so consecutive snapshots cannot skip a move. */
    if (lv_obj_get_style_y(obj, 0) != y) lv_obj_set_y(obj, y);
}

static lv_obj_t *label(lv_obj_t *parent, int x, int y, int w,
                        const lv_font_t *font, uint32_t ink, const char *value)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_width(obj, w);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(ink), 0);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_DOT);
    lv_label_set_text(obj, value);
    return obj;
}

static lv_obj_t *rect(lv_obj_t *parent, int x, int y, int w, int h, uint32_t ink)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, lv_color_hex(ink), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

/* Page objects belong solely to the LVGL timer. Keep the screen and battery,
 * but release inactive page objects to fit the firmware's 24 KiB LVGL pool. */
static lv_obj_t *monitor_content;
static void create_monitor(void);
static lv_obj_t *overlay, *menu_status, *menu_rows[3], *menu_labels[3];
static fmo_view_t overlay_view = FMO_VIEW_MONITOR;
static bool overlay_info;
static char overlay_ssid[33], overlay_password[17];

static void overlay_render(const fmo_controls_t *controls, bool connected,
                           const char *ssid, const char *password)
{
    fmo_view_t view = controls->view;
    if (view != overlay_view || (view == FMO_VIEW_SETUP &&
        (controls->setup_info != overlay_info || strcmp(ssid, overlay_ssid) ||
         strcmp(password, overlay_password)))) {
        if (overlay) lv_obj_delete(overlay);
        overlay = NULL;
    }
    overlay_view = view;
    overlay_info = controls->setup_info;
    snprintf(overlay_ssid, sizeof(overlay_ssid), "%s", ssid);
    snprintf(overlay_password, sizeof(overlay_password), "%s", password);
    if (view == FMO_VIEW_MONITOR) {
        if (!monitor_content) create_monitor();
        return;
    }
    if (monitor_content) {
        lv_obj_delete(monitor_content);
        monitor_content = NULL;
        audio_bar_width = 0;
    }
    if (!overlay) {
        /* Reserve the final QR canvas before small page objects. Otherwise the
         * default canvas resize can split redraw headroom after live updates. */
        lv_draw_buf_t *qr_buffer = view == FMO_VIEW_SETUP && !controls->setup_info ?
            lv_draw_buf_create(128, 128, LV_COLOR_FORMAT_I1, LV_STRIDE_AUTO) : NULL;
        overlay = rect(lv_screen_active(), 0, 40, 240, 280, BLACK);
        if (view == FMO_VIEW_NETWORK) {
            label(overlay, 12, 10, 216, &fmo_channel_font, ORANGE, "网络设置");
            menu_status = label(overlay, 12, 52, 216, &fmo_channel_font, MUTED, "");
            const char *items[] = {"Wi-Fi 配网", "重连 Wi-Fi", "返回守听"};
            for (unsigned i = 0; i < 3; ++i) {
                menu_rows[i] = rect(overlay, 12, 96 + 44 * i, 216, 36, LINE);
                menu_labels[i] = label(menu_rows[i], 8, 6, 200, &fmo_channel_font, WHITE, items[i]);
                center_ink(menu_labels[i], 0, 36, 0x4E2D);
            }
            label(overlay, 12, 220, 216, &fmo_channel_font, WHITE, "上/下选择  确认进入");
            label(overlay, 12, 248, 216, &fmo_channel_font, MUTED, "长按确认: 返回");
        } else {
            bool info = controls->setup_info;
            if (!info) {
                char payload[160];
                lv_obj_t *qr = NULL;
                if (fmo_wifi_qr_payload(payload, sizeof(payload), ssid, password)) {
                    if (qr_buffer) qr = lv_qrcode_create(overlay);
                    if (qr) {
                        lv_draw_buf_t *default_buffer = lv_canvas_get_draw_buf(qr);
                        lv_canvas_set_draw_buf(qr, qr_buffer);
                        qr_buffer = NULL; /* The QR widget now owns the canvas. */
                        if (default_buffer) lv_draw_buf_destroy(default_buffer);
                        lv_qrcode_set_dark_color(qr, lv_color_hex(BLACK));
                        lv_qrcode_set_light_color(qr, lv_color_hex(0xFFFFFF));
                        lv_qrcode_set_quiet_zone(qr, true);
                        /* Balance the visible gaps to the heading and browser text. */
                        lv_obj_set_pos(qr, 56, 45);
                        lv_obj_update_layout(qr);
                        if (lv_obj_get_width(qr) != 128 ||
                            lv_qrcode_update(qr, payload, strlen(payload)) != LV_RESULT_OK) {
                            lv_obj_delete(qr);
                            qr = NULL;
                        }
                    }
                }
                if (qr_buffer) lv_draw_buf_destroy(qr_buffer);
                if (!qr) {
                    lv_obj_delete(overlay);
                    overlay = NULL;
                    fmo_controls_t fallback = *controls;
                    fallback.setup_info = true;
                    overlay_render(&fallback, connected, ssid, password);
                    overlay_info = controls->setup_info;
                    return;
                }
                memset(payload, 0, sizeof(payload));
            }

            if (info) {
                label(overlay, 12, 8, 216, &fmo_channel_font, ORANGE, "手机连接热点");
                label(overlay, 12, 45, 216, &lv_font_montserrat_20, WHITE, ssid);
                label(overlay, 12, 81, 216, &fmo_channel_font, MUTED, "热点密码");
                label(overlay, 12, 110, 216, &lv_font_montserrat_20, WHITE, password);
                label(overlay, 12, 152, 216, &fmo_channel_font, MUTED, "浏览器打开");
                label(overlay, 12, 181, 216, &lv_font_montserrat_20, ORANGE, "192.168.9.1");
            } else {
                label(overlay, 12, 2, 216, &fmo_channel_font, ORANGE, "扫码连接配网热点");
                label(overlay, 12, 184, 216, &fmo_channel_font, WHITE, "浏览器: 192.168.9.1");
            }
            label(overlay, 12, 220, 216, &fmo_channel_font, WHITE,
                  controls->setup_info ? "短按确认: 扫码连接" : "短按确认: 热点信息");
            label(overlay, 12, 248, 216, &fmo_channel_font, MUTED, "长按确认: 取消配网");

        }
    }
    if (view == FMO_VIEW_NETWORK) {
        text(menu_status, connected ? "Wi-Fi 已连接" : "Wi-Fi 未连接");
        for (unsigned i = 0; i < 3; ++i) {
            lv_obj_set_style_bg_color(menu_rows[i], lv_color_hex(i == controls->selection ? ORANGE : LINE), 0);
            color(menu_labels[i], i == controls->selection ? BLACK : WHITE);
        }
    }
}

void fmo_ui_create(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(BLACK), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *brand = label(screen, 12, 10, 70, &lv_font_montserrat_20, ORANGE, "FMO");
    center_ink(brand, 0, 32, 'H');
    clock_label = label(screen, (240 - 72) / 2, 8, 72, &lv_font_montserrat_20, WHITE, "--:--");
    lv_obj_set_style_text_align(clock_label, LV_TEXT_ALIGN_CENTER, 0);
    center_ink(clock_label, 0, 32, 'H');
    /* iOS-inspired compact capsule with percentage inside; no charge icon
     * because the input only supplies SOC, not charging state. */
    battery_body = rect(screen, 192, 8, 32, 16, BLACK);
    lv_obj_set_style_radius(battery_body, 4, 0);
    lv_obj_set_style_border_width(battery_body, 1, 0);
    lv_obj_set_style_border_color(battery_body, lv_color_hex(MUTED), 0);
    lv_obj_t *terminal = rect(screen, 226, 13, 2, 6, MUTED);
    lv_obj_set_style_radius(terminal, 1, 0);
    battery_label = label(screen, 193, 8, 30, &lv_font_montserrat_14, MUTED, "--");
    lv_obj_set_style_text_align(battery_label, LV_TEXT_ALIGN_CENTER, 0);
    center_ink(battery_label, 8, 16, 'H');
    last_battery = -2;
    lv_screen_load(screen);
    create_monitor();
}

void fmo_ui_set_clock(int64_t unix_seconds)
{
    char time_text[6];
    fmo_clock_format(unix_seconds, time_text);
    text(clock_label, time_text);
    center_ink(clock_label, 0, 32, 'H');
}

static void audio_bar_area(lv_obj_t *obj, lv_area_t *area, int width)
{
    lv_obj_get_coords(obj, area);
    area->x1 += 12;
    area->x2 = area->x1 + width - 1;
    area->y1 += ACTIVITY_TOP;
    area->y2 = area->y1 + 1;
}

static void draw_audio_bar(lv_event_t *event)
{
    if (!audio_bar_width) return;
    lv_area_t area;
    audio_bar_area(lv_event_get_target(event), &area, audio_bar_width);
    lv_draw_rect_dsc_t rect;
    lv_draw_rect_dsc_init(&rect);
    rect.bg_color = lv_color_hex(AUDIO_LEVEL_COLOR);
    rect.bg_opa = LV_OPA_COVER;
    lv_draw_rect(lv_event_get_layer(event), &rect, &area);
}

void fmo_ui_set_audio_level(uint8_t level, uint64_t now_ms)
{
    uint8_t visible = 0;
    if (!monitor_content || !audio_meter_enabled) {
        fmo_audio_meter_reset(&audio_meter);
    } else visible = fmo_audio_meter_step(&audio_meter, level, now_ms);
    int width = (216 * visible + 50) / 100;
    if (monitor_content && width != audio_bar_width) {
        lv_area_t area;
        audio_bar_area(monitor_content, &area, width > audio_bar_width ? width : audio_bar_width);
        /* Redraw just these two pixel rows. Draw directly on the existing
         * panel: no resizing/layout work or separate LVGL widget allocation. */
        lv_obj_invalidate_area(monitor_content, &area);
    }
    audio_bar_width = width;
}

static void create_monitor(void)
{
    lv_obj_t *screen = rect(lv_screen_active(), 0, 0, 240, 320, BLACK);
    monitor_content = screen;
    lv_obj_move_background(screen);
    link_label = label(screen, 12, LINK_TOP, 216, &fmo_channel_font, MUTED, "正在连接 Wi-Fi");
    center_ink(link_label, LINK_TOP, LINK_HEIGHT, 'H');
    lv_obj_t *channel = rect(screen, 12, CHANNEL_TOP, 216, CHANNEL_HEIGHT, ORANGE);
    channel_label = label(channel, 8, 0, 200, &fmo_channel_font, BLACK, "--");
    lv_obj_set_style_text_align(channel_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(channel_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    radio_panel = rect(screen, 12, RADIO_TOP, 216, 8 + 4 * RADIO_ROW_HEIGHT, BLACK);
    lv_obj_set_style_border_color(radio_panel, lv_color_hex(ORANGE), 0);
    lv_obj_set_style_border_width(radio_panel, 1, 0);
    lv_obj_set_style_radius(radio_panel, 8, 0);
    const char *values[] = {"--", "-- MHz", "--", "高度: -- m"};
    for (unsigned i = 0; i < 4; ++i) {
        radio_labels[i] = label(radio_panel, 10, 4 + RADIO_ROW_HEIGHT * i, 196,
            i == 1 ? &lv_font_montserrat_20 : &fmo_channel_font, WHITE, values[i]);
        lv_obj_set_height(radio_labels[i], lv_obj_get_style_text_font(radio_labels[i], 0)->line_height);
        center_ink(radio_labels[i], 4 + RADIO_ROW_HEIGHT * i, RADIO_ROW_HEIGHT, 'H');
    }
    air_label = label(screen, 12, AIR_TOP, 216, &fmo_channel_font, MUTED, "等待电台发言");
    center_ink(air_label, AIR_TOP, 24, 'H');
    callsign_label = label(screen, 12, CALLSIGN_TOP, 216, &fmo_callsign_bold_32, WHITE, "--");
    lv_label_set_long_mode(callsign_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    center_ink(callsign_label, CALLSIGN_TOP, 36, 'H');
    audio_bar_width = 0;
    lv_obj_add_event_cb(screen, draw_audio_bar, LV_EVENT_DRAW_MAIN_END, NULL);
    fmo_audio_meter_reset(&audio_meter);
    audio_meter_enabled = false;
    hint_label = label(screen, 12, FOOTER_TOP, 216, &fmo_channel_font, MUTED, "音频:开50%  长按OK:配网");
    center_ink(hint_label, FOOTER_TOP, 24, 'H');
}

void fmo_ui_render(const fmo_monitor_state_t *s, const char *error,
                   const char *setup_ssid, const char *setup_password,
                   int battery, uint64_t now_ms, bool sync_hint, const fmo_controls_t *controls)
{
    char buffer[80];
    bool setup = setup_ssid[0] != 0;
    bool live = !setup && !error[0] && s->wifi_connected && s->events_connected &&
                s->control_connected && s->channel_valid;
    bool speaking = live && s->speaking;
    const char *link = setup ? "手机配网" : error[0] ? "网络连接异常" :
        !s->wifi_connected ? "正在连接 Wi-Fi" :
        !s->events_connected ? "正在连接 FMO" : !live ? "正在同步频道" :
        sync_hint ? "FMO 已连接 已请求刷新" : "FMO 已连接";

    int soc = battery >= 0 && battery <= 100 ? battery : -1;
    if (soc != last_battery) {
        if (soc >= 0) snprintf(buffer, sizeof(buffer), "%d", soc);
        else snprintf(buffer, sizeof(buffer), "--");
        text(battery_label, buffer);
        uint32_t fill = soc < 0 ? BLACK : soc <= 20 ? RED : WHITE;
        lv_obj_set_style_bg_color(battery_body, lv_color_hex(fill), 0);
        lv_obj_set_style_border_color(battery_body, lv_color_hex(soc < 0 ? MUTED : fill), 0);
        color(battery_label, soc < 0 ? MUTED : soc <= 20 ? WHITE : BLACK);
        center_ink(battery_label, 8, 16, 'H');
        last_battery = soc;
    }
    overlay_render(controls, s->wifi_connected, setup_ssid, setup_password);
    audio_meter_enabled = controls->view == FMO_VIEW_MONITOR && live &&
                          controls->audio_enabled && controls->volume > 0;
    if (!audio_meter_enabled) fmo_ui_set_audio_level(0, now_ms);
    if (controls->view != FMO_VIEW_MONITOR) return;
    text(link_label, link);
    color(link_label, error[0] ? RED : live ? WHITE : ORANGE);
    if (setup) snprintf(buffer, sizeof(buffer), "%s", setup_ssid);
    else if (!s->channel_valid) snprintf(buffer, sizeof(buffer), "等待 FMO 连接");
    else if (s->channel_name[0]) snprintf(buffer, sizeof(buffer), "%s", s->channel_name);
    else snprintf(buffer, sizeof(buffer), "频道 #%" PRIu32, s->channel_uid);
    text(channel_label, buffer);
    bool non_ascii = false;
    for (const unsigned char *p = (const unsigned char *)buffer; *p; ++p)
        if (*p >= 0x80) { non_ascii = true; break; }
    center_ink(channel_label, 0, CHANNEL_HEIGHT, non_ascii ? 0x4E2D : 'H');
    const char *call = "--";
    const char *air = live ? "等待电台发言" : "等待频道同步";
    if (setup) {
        air = "热点密码";
        call = setup_password;
    } else if (speaking) {
        air = "正在发言";
        call = s->speaker;
    } else if (live && s->last_speaker[0]) {
        air = "上次通联";
        call = s->last_speaker;
    } else if (error[0]) {
        air = !strcmp(error, "DATA RESET FAILED") ?
                 "数据清理失败 请重试" : !strcmp(error, "SETUP SAVE FAILED") ?
                 "配网设置保存失败" : "网络启动失败 请重试";
    }
    text(air_label, air);
    color(air_label, speaking || setup ? ORANGE : MUTED);
    text(callsign_label, call);
    color(callsign_label, speaking ? ORANGE : WHITE);
    /* Use real bold glyphs for callsigns; keep setup passwords in regular text.
     * Measure the selected weight before fitting long suffixes at 20px. */
    const lv_font_t *large = setup ? &lv_font_montserrat_32 : &fmo_callsign_bold_32;
    const lv_font_t *small = setup ? &lv_font_montserrat_20 : &fmo_callsign_bold_20;
    lv_point_t size;
    lv_text_get_size(&size, call, large, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    const lv_font_t *font = size.x > 216 ? small : large;
    if (lv_obj_get_style_text_font(callsign_label, 0) != font)
        lv_obj_set_style_text_font(callsign_label, font, 0);
    center_ink(callsign_label, CALLSIGN_TOP, 36, 'H');
    bool radio_online = !setup && !error[0] && s->wifi_connected && s->control_connected;
    const fmo_radio_profile_t *r = &s->radio;
    text(radio_labels[0], radio_online && r->device_name[0] ? r->device_name : "--");
    if (radio_online && r->frequency_100hz)
        snprintf(buffer, sizeof(buffer), "%" PRIu32 ".%04" PRIu32 " MHz",
                 r->frequency_100hz / 10000, r->frequency_100hz % 10000);
    else snprintf(buffer, sizeof(buffer), "-- MHz");
    text(radio_labels[1], buffer);
    text(radio_labels[2], radio_online && r->antenna[0] ? r->antenna : "--");
    if (radio_online && r->height_valid)
        snprintf(buffer, sizeof(buffer), "高度: %" PRIu32 " m", r->antenna_height_m);
    else snprintf(buffer, sizeof(buffer), "高度: -- m");
    text(radio_labels[3], buffer);
    for (unsigned i = 0; i < 4; ++i) {
        color(radio_labels[i], radio_online ? WHITE : MUTED);
        center_ink(radio_labels[i], 4 + RADIO_ROW_HEIGHT * i, RADIO_ROW_HEIGHT, 'H');
    }
    if (setup) snprintf(buffer, sizeof(buffer), "打开 192.168.9.1");
    else snprintf(buffer, sizeof(buffer), "音频:%s%u%%  长按OK:配网", controls->audio_enabled ? "开" : "关",
                  controls->volume);
    text(hint_label, buffer);
    center_ink(link_label, LINK_TOP, LINK_HEIGHT, 'H');
    center_ink(air_label, AIR_TOP, 24, 'H');
    center_ink(hint_label, FOOTER_TOP, 24, 'H');
}
