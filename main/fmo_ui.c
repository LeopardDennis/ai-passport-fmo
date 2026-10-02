#include "fmo_ui.h"
#include "fmo_wifi_qr.h"
#include "src/misc/lv_text_private.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

/* Approximation of the supplied FMO reference, not an official color spec. */
#define BLACK 0x000000
#define ORANGE 0xFF8A00
#define WHITE 0xF4F4F4
#define MUTED 0x929292
#define LINE 0x303030
#define RED 0xFF5252
LV_FONT_DECLARE(fmo_channel_font);

static lv_obj_t *link_label, *battery_label, *channel_label;
static lv_obj_t *battery_body;
static int last_battery = -2;
static lv_obj_t *air_label, *callsign_label, *detail_label, *hint_label, *activity;

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
    }
    if (!overlay) {
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
                    qr = lv_qrcode_create(overlay);
                    if (qr) {
                        lv_qrcode_set_size(qr, 128);
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
    lv_obj_t *brand = label(screen, 12, 10, 120, &lv_font_montserrat_20, ORANGE, "FMO");
    center_ink(brand, 0, 40, 'H');
    /* iOS-inspired compact capsule with percentage inside; no charge icon
     * because the input only supplies SOC, not charging state. */
    battery_body = rect(screen, 192, 12, 32, 16, BLACK);
    lv_obj_set_style_radius(battery_body, 4, 0);
    lv_obj_set_style_border_width(battery_body, 1, 0);
    lv_obj_set_style_border_color(battery_body, lv_color_hex(MUTED), 0);
    lv_obj_t *terminal = rect(screen, 226, 17, 2, 6, MUTED);
    lv_obj_set_style_radius(terminal, 1, 0);
    battery_label = label(screen, 193, 12, 30, &lv_font_montserrat_14, MUTED, "--");
    lv_obj_set_style_text_align(battery_label, LV_TEXT_ALIGN_CENTER, 0);
    center_ink(battery_label, 12, 16, 'H');
    last_battery = -2;
    lv_screen_load(screen);
    create_monitor();
}

static void create_monitor(void)
{
    lv_obj_t *screen = rect(lv_screen_active(), 0, 0, 240, 320, BLACK);
    monitor_content = screen;
    lv_obj_move_background(screen);
    rect(screen, 12, 39, 216, 1, LINE);
    link_label = label(screen, 12, 49, 216, &fmo_channel_font, MUTED, "正在连接 Wi-Fi");
    center_ink(link_label, 40, 39, 'H');
    lv_obj_t *channel = rect(screen, 12, 79, 216, 36, ORANGE);
    channel_label = label(channel, 8, 6, 200, &fmo_channel_font, BLACK, "--");
    lv_obj_set_style_text_align(channel_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(channel_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    air_label = label(screen, 12, 136, 216, &fmo_channel_font, MUTED, "守听中");
    center_ink(air_label, 123, 34, 'H');
    callsign_label = label(screen, 12, 165, 216, &lv_font_montserrat_32, WHITE, "--");
    lv_label_set_long_mode(callsign_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    detail_label = label(screen, 12, 212, 216, &fmo_channel_font, MUTED, "等待电台发言");
    center_ink(detail_label, 204, 36, 'H');
    rect(screen, 12, 249, 216, 3, LINE);
    activity = rect(screen, 12, 249, 216, 3, ORANGE);
    lv_obj_add_flag(activity, LV_OBJ_FLAG_HIDDEN);
    hint_label = label(screen, 12, 269, 216, &fmo_channel_font, WHITE, "短按确认: 刷新");
    center_ink(hint_label, 259, 28, 'H');
    lv_obj_t *footer = label(screen, 12, 294, 216, &fmo_channel_font, MUTED, "上/下: 亮度  长按: 网络");
    center_ink(footer, 287, 28, 'H');
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
        !s->events_connected ? "正在连接 FMO" : !live ? "正在同步频道" : "FMO 已连接";

    int soc = battery >= 0 && battery <= 100 ? battery : -1;
    if (soc != last_battery) {
        if (soc >= 0) snprintf(buffer, sizeof(buffer), "%d", soc);
        else snprintf(buffer, sizeof(buffer), "--");
        text(battery_label, buffer);
        uint32_t fill = soc < 0 ? BLACK : soc <= 20 ? RED : WHITE;
        lv_obj_set_style_bg_color(battery_body, lv_color_hex(fill), 0);
        lv_obj_set_style_border_color(battery_body, lv_color_hex(soc < 0 ? MUTED : fill), 0);
        color(battery_label, soc < 0 ? MUTED : soc <= 20 ? WHITE : BLACK);
        center_ink(battery_label, 12, 16, 'H');
        last_battery = soc;
    }
    overlay_render(controls, s->wifi_connected, setup_ssid, setup_password);
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
    center_ink(channel_label, 0, 36, non_ascii ? 0x4E2D : 'H');
    const char *call = "--";
    const char *air = live ? "守听中" : "等待频道同步";
    buffer[0] = 0;
    if (setup) {
        air = "热点密码";
        call = setup_password;
        snprintf(buffer, sizeof(buffer), "打开 192.168.9.1");
    } else if (speaking) {
        air = "正在发言";
        call = s->speaker;
        snprintf(buffer, sizeof(buffer), "%s%s%s", s->speaker_is_host ? "本机" : "电台",
                 s->grid[0] ? " / " : "", s->grid);
    } else if (live && s->last_speaker[0]) {
        air = "上次通联";
        call = s->last_speaker;
        uint64_t age = now_ms >= s->last_speaker_ms ? (now_ms - s->last_speaker_ms) / 1000 : 0;
        snprintf(buffer, sizeof(buffer), "%s%s%" PRIu64 " 秒前", s->grid,
                 s->grid[0] ? " / " : "", age);
    } else if (error[0]) {
        air = "等待重试";
        snprintf(buffer, sizeof(buffer), "%s", !strcmp(error, "DATA RESET FAILED") ?
                 "数据清理失败，请重试" : !strcmp(error, "SETUP SAVE FAILED") ?
                 "配网设置保存失败" : "网络启动失败，请重试");
    } else if (live) snprintf(buffer, sizeof(buffer), "等待电台发言");
    text(air_label, air);
    color(air_label, speaking || setup ? ORANGE : MUTED);
    text(callsign_label, call);
    color(callsign_label, speaking ? ORANGE : WHITE);
    /* Keep full ordinary callsigns large, fit long suffixes/passwords at 20px. */
    lv_point_t size;
    lv_text_get_size(&size, call, &lv_font_montserrat_32, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    const lv_font_t *font = size.x > 216 ? &lv_font_montserrat_20 : &lv_font_montserrat_32;
    if (lv_obj_get_style_text_font(callsign_label, 0) != font)
        lv_obj_set_style_text_font(callsign_label, font, 0);
    center_ink(callsign_label, 158, 46, 'H');
    text(detail_label, buffer);
    if (speaking) lv_obj_remove_flag(activity, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(activity, LV_OBJ_FLAG_HIDDEN);
    text(hint_label, setup ? "手机连接上方热点" : sync_hint ? "已请求刷新" : "短按确认: 刷新");
    center_ink(link_label, 40, 39, 'H');
    center_ink(air_label, 123, 34, 'H');
    center_ink(detail_label, 204, 36, 'H');
    center_ink(hint_label, 259, 28, 'H');
}
