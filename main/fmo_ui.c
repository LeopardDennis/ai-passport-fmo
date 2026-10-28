#include "fmo_ui.h"
#include "fmo_clock.h"
#include "fmo_audio_meter.h"
#include "fmo_wifi_qr.h"
#include "fmo_wifi_font.h"
#include "src/misc/lv_text_private.h"
#include "src/misc/lv_area_private.h"
#include <inttypes.h>
#include <math.h>
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
#define CROSS_SERVER_COLOR 0xF66969
#define GRID_COLOR MUTED
/* Fixed bands: unknown metadata and first idle use the same geometry as
 * live speech. Status/error text shares the air row instead of moving panels. */
#define LINK_TOP 32
#define LINK_HEIGHT 24
#define CHANNEL_TOP 62
#define CHANNEL_HEIGHT 32
#define QSO_TOP 183
#define QSO_ROW_HEIGHT 36
#define QSO_ROWS_TOP 29
#define QSO_TEXT_HEIGHT 14
#define QSO_TIME_OFFSET 19
#define LOCATION_TOP 150
#define LOCATION_HEIGHT 16
#define AIR_TOP 98
#define AIR_HEIGHT 16
#define CALLSIGN_TOP 116
#define ACTIVITY_TOP 173
#define FOOTER_TOP 294
LV_FONT_DECLARE(fmo_channel_font);
LV_FONT_DECLARE(fmo_callsign_bold_14);
LV_FONT_DECLARE(fmo_callsign_bold_32);
LV_FONT_DECLARE(fmo_grid_14);
LV_FONT_DECLARE(fmo_grid_12);
LV_FONT_DECLARE(fmo_grid_10);
LV_FONT_DECLARE(fmo_grid_8);
LV_FONT_DECLARE(fmo_grid_6);
LV_FONT_DECLARE(fmo_grid_4);
LV_FONT_DECLARE(fmo_grid_2);


static lv_obj_t *audio_label, *link_label, *battery_label, *channel_label, *clock_label;
static lv_obj_t *battery_body;
static int last_battery = -2;
static lv_obj_t *air_label, *callsign_label, *grid_label, *hint_label;
static lv_obj_t *qso_panel, *qso_labels[FMO_HISTORY_COUNT];
static char path_text[64], location_text[FMO_LOCATION_MAX + 4];
static bool path_cache_valid, path_ready;
static fmo_position_t path_origin, path_target;
static int path_left;
static int location_offset, location_overflow;
static uint64_t location_scroll_started;
static char qso_times[FMO_HISTORY_COUNT][20];
static int64_t qso_stamps[FMO_HISTORY_COUNT];
static bool qso_present[FMO_HISTORY_COUNT];
static bool callsign_layout_valid, callsign_setup;
static int64_t clock_minute = INT64_MIN;
static fmo_audio_meter_t audio_meter;
static bool audio_meter_enabled;
static int audio_bar_width;

static bool text(lv_obj_t *label, const char *value)
{
    if (!strcmp(lv_label_get_text(label), value)) return false;
    lv_label_set_text(label, value);
    return true;
}

static void color(lv_obj_t *obj, uint32_t value)
{
    if (!lv_color_eq(lv_obj_get_style_text_color(obj, 0), lv_color_hex(value)))
        lv_obj_set_style_text_color(obj, lv_color_hex(value), 0);
}

/* Center the union of visible glyph boxes, not the font's ascent/descent
 * padding. LVGL's UTF-8 decoder matches the pinned renderer's behavior. */
static int center_ink(lv_obj_t *obj, int top, int height, uint32_t reference)
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
        if (!lv_font_get_glyph_dsc(font, &glyph, reference, 0) || !glyph.box_h) return top + height - 1;
        ink_top = font->line_height - font->base_line - glyph.box_h - glyph.ofs_y;
        ink_bottom = ink_top + glyph.box_h;
    }
    int y = top + (height - (ink_bottom - ink_top)) / 2 - ink_top;
    /* Coordinates can be stale until the next layout pass. Compare the
     * requested style position so consecutive snapshots cannot skip a move. */
    if (lv_obj_get_style_y(obj, 0) != y) lv_obj_set_y(obj, y);
    return y + ink_bottom - 1;
}

/* Center the letter bodies in saved names. Underscores and descenders must
 * not push Latin names visibly above the middle of the row. */
static void center_wifi_name(lv_obj_t *obj, int top, int height)
{
    const lv_font_t *font = lv_obj_get_style_text_font(obj, 0);
    lv_font_glyph_dsc_t glyph;
    if (!lv_font_get_glyph_dsc(font, &glyph, 'H', 0)) { center_ink(obj, top, height, 'H'); return; }
    int baseline = font->line_height - font->base_line - glyph.ofs_y;
    int ink_top = INT32_MAX, ink_bottom = INT32_MIN;
    const char *value = lv_label_get_text(obj);
    uint32_t index = 0;
    while (value[index]) {
        uint32_t code = lv_text_encoded_next(value, &index);
        if (code == '_' || !lv_font_get_glyph_dsc(font, &glyph, code, 0) || !glyph.box_h || !glyph.box_w) continue;
        int y = font->line_height - font->base_line - glyph.box_h - glyph.ofs_y;
        int bottom = y + glyph.box_h;
        if (code < 128 && bottom > baseline) bottom = baseline;
        if (bottom <= y) continue;
        if (y < ink_top) ink_top = y;
        if (bottom > ink_bottom) ink_bottom = bottom;
    }
    if (ink_top == INT32_MAX) { center_ink(obj, top, height, 'H'); return; }
    int y = top + (height - (ink_bottom - ink_top) + 1) / 2 - ink_top;
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
static lv_obj_t *overlay, *menu_status, *menu_rows[5], *menu_labels[5], *wifi_badge;
static unsigned wifi_row_count, wifi_row_selection;
#define WIFI_ROWS_TOP 72
#define WIFI_ROW_STEP 30
#define WIFI_ROW_HEIGHT 25

/* Draw row backgrounds directly, retaining redraw headroom in the 24 KiB pool. */
static void draw_wifi_rows(lv_event_t *event)
{
    lv_area_t parent;
    lv_obj_get_coords(lv_event_get_target(event), &parent);
    for (unsigned i = 0; i < wifi_row_count; ++i) {
        lv_area_t area = {.x1=parent.x1+12, .x2=parent.x1+227,
            .y1=parent.y1+WIFI_ROWS_TOP+WIFI_ROW_STEP*i,
            .y2=parent.y1+WIFI_ROWS_TOP+WIFI_ROW_STEP*i+WIFI_ROW_HEIGHT-1};
        lv_draw_rect_dsc_t dsc;
        lv_draw_rect_dsc_init(&dsc);
        dsc.bg_color = lv_color_hex(i == wifi_row_selection ? ORANGE : LINE);
        dsc.bg_opa = LV_OPA_COVER;
        lv_draw_rect(lv_event_get_layer(event), &dsc, &area);
    }
}
static lv_obj_t *station_status, *station_page, *station_footer, *overlay_footer;
static lv_obj_t *station_names[FMO_STATION_PAGE_SIZE], *station_badge;
static unsigned station_draw_count, station_draw_selection;

/* Six fixed row backgrounds share the overlay draw pass. Avoid six container
 * objects so list/QR/monitor transitions keep the existing 24 KiB pool budget. */
static void draw_station_rows(lv_event_t *event)
{
    lv_area_t panel;
    lv_obj_get_coords(lv_event_get_target(event), &panel);
    for (unsigned i = 0; i < station_draw_count; ++i) {
        lv_area_t area = {.x1 = panel.x1 + 12, .x2 = panel.x1 + 227,
            .y1 = panel.y1 + 64 + 28 * i, .y2 = panel.y1 + 89 + 28 * i};
        lv_draw_rect_dsc_t dsc;
        lv_draw_rect_dsc_init(&dsc);
        dsc.bg_color = lv_color_hex(i == station_draw_selection ? ORANGE : LINE);
        dsc.bg_opa = LV_OPA_COVER;
        lv_draw_rect(lv_event_get_layer(event), &dsc, &area);
    }
}
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
    if (strcmp(overlay_ssid, ssid)) snprintf(overlay_ssid, sizeof(overlay_ssid), "%s", ssid);
    if (strcmp(overlay_password, password)) snprintf(overlay_password, sizeof(overlay_password), "%s", password);
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
        overlay_footer = NULL;
        /* Reserve the final QR canvas before small page objects. Otherwise the
         * default canvas resize can split redraw headroom after live updates. */
        lv_draw_buf_t *qr_buffer = view == FMO_VIEW_SETUP && !controls->setup_info ?
            lv_draw_buf_create(128, 128, LV_COLOR_FORMAT_I1, LV_STRIDE_AUTO) : NULL;
        overlay = rect(lv_screen_active(), 0, 40, 240, 280, BLACK);
        if (view == FMO_VIEW_STATIONS) {
            lv_obj_t *title = label(overlay, 12, 10, 80, &fmo_channel_font, ORANGE,
                controls->stations.category == FMO_STATION_PINNED ? "收藏台站" :
                controls->stations.category == FMO_STATION_ACTIVE ? "活跃台站" : "所有台站");
            center_ink(title, 10, 20, 'H');
            station_page = label(overlay, 96, 10, 132, &lv_font_montserrat_14, MUTED, "");
            lv_obj_set_style_text_align(station_page, LV_TEXT_ALIGN_RIGHT, 0);
            center_ink(station_page, 10, 20, 'H');
            station_status = label(overlay, 12, 38, 216, &fmo_channel_font, MUTED, "");
            lv_obj_set_height(station_status, fmo_channel_font.line_height);
            station_draw_count = 0;
            station_draw_selection = UINT32_MAX;
            lv_obj_add_event_cb(overlay, draw_station_rows, LV_EVENT_DRAW_MAIN, NULL);
            for (unsigned i = 0; i < FMO_STATION_PAGE_SIZE; ++i) {
                station_names[i] = label(overlay, 20, 64 + 28 * i, 176, &fmo_channel_font, WHITE, "");
                lv_obj_set_height(station_names[i], fmo_channel_font.line_height);
            }
            station_badge = label(overlay, 204, 64, 16, &lv_font_montserrat_14, ORANGE, LV_SYMBOL_OK);
            station_footer = label(overlay, 12, FOOTER_TOP - 40, 216, fmo_wifi_font(), MUTED, "");
            lv_obj_set_style_text_align(station_footer, LV_TEXT_ALIGN_CENTER, 0);
        } else if (view == FMO_VIEW_NETWORK || view == FMO_VIEW_STATION_MENU) {
            label(overlay, 12, 10, 216, &fmo_channel_font, ORANGE,
                  view == FMO_VIEW_STATION_MENU ? "台站列表" : "网络设置");
            menu_status = label(overlay, 12, 52, 216, &fmo_channel_font, MUTED, "");
            const char *network_items[] = {"Wi-Fi 配网", "选择 Wi-Fi", "返回"};
            const char *station_items[] = {"收藏台站", "活跃台站", "所有台站"};
            const char *const *items = view == FMO_VIEW_STATION_MENU ? station_items : network_items;
            for (unsigned i = 0; i < 3; ++i) {
                menu_rows[i] = rect(overlay, 12, 96 + 44 * i, 216, 36, LINE);
                menu_labels[i] = label(menu_rows[i], 8, 6, 200, &fmo_channel_font, WHITE, items[i]);
                center_ink(menu_labels[i], 0, 36, 0x4E2D);
            }
            overlay_footer = label(overlay, 12, FOOTER_TOP - 40, 216, fmo_wifi_font(), MUTED, "上/下: 选择 长按确认: 返回");
        } else if (view == FMO_VIEW_WIFI) {
            label(overlay, 12, 10, 216, &fmo_channel_font, ORANGE, "选择 Wi-Fi");
            menu_status = label(overlay, 12, 36, 216, &fmo_channel_font, MUTED, "");
            lv_obj_add_event_cb(overlay, draw_wifi_rows, LV_EVENT_DRAW_MAIN, NULL);
            for (unsigned i = 0; i < FMO_WIFI_PROFILE_MAX; ++i)
                menu_labels[i] = label(overlay, 20, WIFI_ROWS_TOP + 2 + WIFI_ROW_STEP * i, 200, fmo_wifi_font(), WHITE, "");
            wifi_badge = label(overlay, 204, WIFI_ROWS_TOP + 2, 16, &lv_font_montserrat_14, ORANGE, LV_SYMBOL_OK);
            overlay_footer = label(overlay, 12, FOOTER_TOP - 40, 216, fmo_wifi_font(), MUTED, "");
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
                lv_obj_t *setup_title = label(overlay, 12, 2, 216, &fmo_channel_font, ORANGE, "扫码连接配网热点");
                lv_obj_set_style_text_align(setup_title, LV_TEXT_ALIGN_CENTER, 0);
                lv_obj_t *setup_address = label(overlay, 12, 184, 216, &fmo_channel_font, WHITE, "浏览器打开: 192.168.9.1");
                lv_obj_set_style_text_align(setup_address, LV_TEXT_ALIGN_CENTER, 0);
            }
            overlay_footer = label(overlay, 12, FOOTER_TOP - 40, 216, fmo_wifi_font(), MUTED,
                  info ? "确认: 查看二维码 长按确认: 返回" : "确认: 查看详情 长按确认: 返回");

        }
    }
    if (overlay_footer) {
        if (view == FMO_VIEW_WIFI)
            text(overlay_footer, controls->wifi.count ? "上/下: 选择 长按确认: 返回" : "长按确认: 返回");
        if (lv_obj_get_style_text_align(overlay_footer, 0) != LV_TEXT_ALIGN_CENTER)
            lv_obj_set_style_text_align(overlay_footer, LV_TEXT_ALIGN_CENTER, 0);
        center_ink(overlay_footer, FOOTER_TOP - 40, 18, 'H');
    }
    if (view == FMO_VIEW_WIFI) {
        char status[48];
        snprintf(status, sizeof(status), controls->wifi.count ? "已保存 %u/5 组" : "尚未保存 Wi-Fi", controls->wifi.count);
        text(menu_status, controls->wifi.count && controls->selection >= controls->wifi.count ?
             "列表已更新，请重新选择" : status);
        if (wifi_row_count != controls->wifi.count || wifi_row_selection != controls->selection) {
            wifi_row_count = controls->wifi.count;
            wifi_row_selection = controls->selection;
            lv_obj_invalidate(overlay);
        }
        lv_obj_add_flag(wifi_badge, LV_OBJ_FLAG_HIDDEN);
        for (unsigned i = 0; i < FMO_WIFI_PROFILE_MAX; ++i) {
            if (i >= controls->wifi.count) { lv_obj_add_flag(menu_labels[i], LV_OBJ_FLAG_HIDDEN); continue; }
            lv_obj_remove_flag(menu_labels[i], LV_OBJ_FLAG_HIDDEN);
            bool selected = i == controls->selection;
            bool current = connected && !strcmp(controls->wifi.names[i], controls->connected_ssid);
            text(menu_labels[i], controls->wifi.names[i]);
            lv_obj_set_width(menu_labels[i], 176);
            lv_label_long_mode_t mode = selected ? LV_LABEL_LONG_SCROLL_CIRCULAR : LV_LABEL_LONG_DOT;
            if (lv_label_get_long_mode(menu_labels[i]) != mode)
                lv_label_set_long_mode(menu_labels[i], mode);
            center_wifi_name(menu_labels[i], WIFI_ROWS_TOP + WIFI_ROW_STEP * i, WIFI_ROW_HEIGHT);
            color(menu_labels[i], selected ? BLACK : WHITE);
            if (current) {
                color(wifi_badge, selected ? BLACK : ORANGE);
                center_ink(wifi_badge, WIFI_ROWS_TOP + WIFI_ROW_STEP * i, WIFI_ROW_HEIGHT, 'H');
                lv_obj_remove_flag(wifi_badge, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
    if (view == FMO_VIEW_NETWORK || view == FMO_VIEW_STATION_MENU) {
        if (view == FMO_VIEW_STATION_MENU)
            text(menu_status, controls->stations.request != FMO_STATION_NONE ? "正在处理 请稍候" : "确认: 打开列表");
        else text(menu_status, controls->hotspot_active ? "配网热点已开启" :
             connected ? "Wi-Fi 已连接" : "Wi-Fi 未连接");
        for (unsigned i = 0; i < 3; ++i) {
            lv_color_t fill = lv_color_hex(i == controls->selection ? ORANGE : LINE);
            if (!lv_color_eq(lv_obj_get_style_bg_color(menu_rows[i], 0), fill))
                lv_obj_set_style_bg_color(menu_rows[i], fill, 0);
            color(menu_labels[i], i == controls->selection ? BLACK : WHITE);
        }
    }
}

static void render_stations(const fmo_controls_t *controls, const fmo_monitor_state_t *state)
{
    const fmo_stations_t *s = &controls->stations;
    char buffer[80];
    unsigned selected = controls->selection < s->count ? controls->selection : 0;
    if (s->count && s->total_known) snprintf(buffer, sizeof(buffer), "%lu/%lu",
        (unsigned long)s->start + selected + 1, (unsigned long)s->total);
    else if (s->count) snprintf(buffer, sizeof(buffer), "%lu/--",
        (unsigned long)s->start + selected + 1);
    else if (s->status == FMO_STATIONS_EMPTY && s->total_known) snprintf(buffer, sizeof(buffer), "0/%lu", (unsigned long)s->total);
    else buffer[0] = '\0';
    lv_point_t page_size;
    lv_text_get_size(&page_size, buffer, &lv_font_montserrat_14, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    int letter_space = page_size.x > 132 ? -1 : 0;
    if (lv_obj_get_style_text_letter_space(station_page, 0) != letter_space)
        lv_obj_set_style_text_letter_space(station_page, letter_space, 0);
    if (text(station_page, buffer)) center_ink(station_page, 10, 20, 'H');
    const char *status = NULL;
    switch (s->status) {
    case FMO_STATIONS_IDLE:
    case FMO_STATIONS_LOADING: status = "正在读取台站"; break;
    case FMO_STATIONS_LOAD_FAILED:
        status = !state->wifi_connected ? "Wi-Fi未连接" :
            !state->control_connected ? "FMO未连接" : "读取失败";
        break;
    case FMO_STATIONS_EMPTY:
        status = s->category == FMO_STATION_PINNED ? "暂无收藏台站" :
                 s->category == FMO_STATION_ACTIVE ? "暂无活跃台站" : "暂无台站";
        break;
    case FMO_STATIONS_SWITCHING: status = "正在切换台站"; break;
    case FMO_STATIONS_FAILED: status = s->audio_paused ? "切换失败 正在确认" : "切换失败"; break;
    case FMO_STATIONS_UNKNOWN: status = s->audio_paused ? "结果待确认 请稍候" : "未确认切换"; break;
    default: break;
    }
    if (!status) {
        if (state->channel_valid) snprintf(buffer, sizeof(buffer), "当前: %s", state->channel_name);
        else snprintf(buffer, sizeof(buffer), "正在确认当前台站");
        status = buffer;
    }
    if (text(station_status, status)) center_ink(station_status, 36, 20, 'H');
    color(station_status, s->status == FMO_STATIONS_FAILED ||
        s->status == FMO_STATIONS_LOAD_FAILED ? RED : MUTED);
    unsigned count = s->status == FMO_STATIONS_LOADING ? 0 : s->count;
    if (count != station_draw_count || controls->selection != station_draw_selection) {
        station_draw_count = count;
        station_draw_selection = controls->selection;
        lv_obj_invalidate(overlay);
    }
    lv_obj_add_flag(station_badge, LV_OBJ_FLAG_HIDDEN);
    for (unsigned i = 0; i < FMO_STATION_PAGE_SIZE; ++i) {
        bool present = i < s->count && s->status != FMO_STATIONS_LOADING;
        if (!present) { lv_obj_add_flag(station_names[i], LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_remove_flag(station_names[i], LV_OBJ_FLAG_HIDDEN);
        bool selected = i == controls->selection;
        color(station_names[i], selected ? BLACK : WHITE);
        const fmo_station_t *row = &s->rows[i];
        bool current = state->channel_valid && row->uid == state->channel_uid;
        bool duplicate = false;
        for (unsigned j = 0; j < s->count; ++j)
            if (i != j && !strcmp(row->name, s->rows[j].name)) duplicate = true;
        if (duplicate) snprintf(buffer, sizeof(buffer), "#%lu %s", (unsigned long)row->uid, row->name);
        else if (row->name[0]) snprintf(buffer, sizeof(buffer), "%s", row->name);
        else snprintf(buffer, sizeof(buffer), "台站 #%lu", (unsigned long)row->uid);
        lv_label_long_mode_t mode = selected ? LV_LABEL_LONG_SCROLL_CIRCULAR : LV_LABEL_LONG_DOT;
        if (lv_label_get_long_mode(station_names[i]) != mode) lv_label_set_long_mode(station_names[i], mode);
        if (text(station_names[i], buffer)) center_ink(station_names[i], 64 + 28 * i, 26, 'H');
        if (current) {
            color(station_badge, selected ? BLACK : ORANGE);
            center_ink(station_badge, 64 + 28 * i, 26, 'H');
            lv_obj_remove_flag(station_badge, LV_OBJ_FLAG_HIDDEN);
        }
    }
    const char *footer;
    if (s->audio_paused || s->request != FMO_STATION_NONE ||
        s->status == FMO_STATIONS_IDLE || s->status == FMO_STATIONS_LOADING ||
        s->status == FMO_STATIONS_SWITCHING)
        footer = "长按确认: 返回";
    else if (s->status == FMO_STATIONS_LOAD_FAILED || s->status == FMO_STATIONS_EMPTY)
        footer = "确认: 重试 长按确认: 返回";
    else if (s->count)
        footer = "长按上下翻页 长按确认返回";
    else footer = "长按确认: 返回";
    if (text(station_footer, footer)) center_ink(station_footer, FOOTER_TOP - 40, 18, 'H');
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
    clock_minute = INT64_MIN;
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
    /* Validity boundaries in fmo_clock_format are minute-aligned. A backward
     * SNTP correction or invalid-to-valid transition also changes this key. */
    int64_t minute = unix_seconds / 60;
    if (minute == clock_minute) return;
    clock_minute = minute;
    char time_text[6];
    fmo_clock_format(unix_seconds, time_text);
    if (text(clock_label, time_text)) center_ink(clock_label, 0, 32, 'H');
}

static void audio_bar_area(lv_obj_t *obj, lv_area_t *area, int width)
{
    lv_obj_get_coords(obj, area);
    area->x1 += 12;
    area->x2 = area->x1 + width - 1;
    area->y1 += ACTIVITY_TOP;
    area->y2 = area->y1 + 1;
}

/* Fixed position leaves eight clear pixels above and below the PCM bar. */
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

/* Reuse the application's 50 ms LVGL timer, without a new timer or widget.
 * Only the place viewport is invalidated; bearing and distance stay fixed. */
void fmo_ui_tick(uint64_t now_ms)
{
    if (!monitor_content || location_overflow <= 0) return;
    const uint64_t pause = 1000;
    uint64_t travel = ((uint64_t)location_overflow * 1000 + 29) / 30;
    uint64_t elapsed = now_ms >= location_scroll_started ? now_ms - location_scroll_started : 0;
    uint64_t phase = elapsed % (2 * (pause + travel));
    int offset;
    if (phase < pause) offset = 0;
    else if (phase < pause + travel) offset = (int)((phase - pause) * location_overflow / travel);
    else if (phase < 2 * pause + travel) offset = location_overflow;
    else offset = location_overflow - (int)((phase - 2 * pause - travel) * location_overflow / travel);
    if (offset == location_offset) return;
    location_offset = offset;
    lv_area_t area;
    lv_obj_get_coords(monitor_content, &area);
    area.x2 = area.x1 + path_left - 9;
    area.x1 += 12;
    area.y1 += LOCATION_TOP;
    area.y2 = area.y1 + LOCATION_HEIGHT - 1;
    lv_obj_invalidate_area(monitor_content, &area);
}

/* Dates are fixed text without scrolling. Draw them in the panel instead of
 * allocating extra date labels, preserving the 24 KiB LVGL redraw headroom. */
static void draw_qso_times(lv_event_t *event)
{
    lv_area_t panel;
    lv_obj_get_coords(lv_event_get_target(event), &panel);
    const lv_font_t *font = &lv_font_montserrat_14;
    lv_font_glyph_dsc_t glyph;
    if (!lv_font_get_glyph_dsc(font, &glyph, '0', 0)) return;
    int ink_top = font->line_height - font->base_line - glyph.box_h - glyph.ofs_y;
    for (unsigned i = 0; i < FMO_HISTORY_COUNT; ++i) {
        lv_area_t area = {
            .x1 = panel.x1 + 12, .x2 = panel.x1 + 207,
            .y1 = panel.y1 + QSO_ROWS_TOP + QSO_ROW_HEIGHT * i + QSO_TIME_OFFSET +
                  (QSO_TEXT_HEIGHT - glyph.box_h) / 2 - ink_top,
        };
        area.y2 = area.y1 + font->line_height - 1;
        lv_draw_label_dsc_t dsc;
        lv_draw_label_dsc_init(&dsc);
        dsc.base.obj = lv_event_get_target(event);
        dsc.base.id1 = i;
        dsc.font = font;
        dsc.color = lv_color_hex(MUTED);
        dsc.text = qso_times[i];
        dsc.text_local = true;
        lv_draw_label(lv_event_get_layer(event), &dsc, &area);
    }
}

/* Geometry is recomputed only when the source coordinates or validity change. */
static void render_path(const fmo_monitor_state_t *s, bool live)
{
    fmo_position_t target = s->position;
    bool ready = live && s->last_speaker[0] && s->local_position_valid &&
        (s->position_valid || fmo_location_grid(s->grid, &target));
    if (path_cache_valid && ready == path_ready && (!ready ||
        (s->local_position.lat_e6 == path_origin.lat_e6 && s->local_position.lon_e6 == path_origin.lon_e6 &&
         target.lat_e6 == path_target.lat_e6 && target.lon_e6 == path_target.lon_e6))) return;
    path_cache_valid = true;
    path_ready = ready;
    path_origin = s->local_position;
    path_target = target;
    char value[64] = "--° --  -- km";
    double km, bearing;
    if (ready && fmo_location_path(path_origin, target, &km, &bearing)) {
        char distance[24];
        if (km < 1) snprintf(distance, sizeof(distance), "%.1f km", km);
        else snprintf(distance, sizeof(distance), "%ld km", lround(km));
        if (isfinite(bearing)) {
            static const char *const compass[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
            unsigned degrees = (unsigned)lround(bearing) % 360;
            unsigned sector = (unsigned)((bearing + 22.5) / 45) % 8;
            snprintf(value, sizeof(value), "%u° %s  %s", degrees, compass[sector], distance);
        } else snprintf(value, sizeof(value), "--° --  %s", distance);
    }
    if (strcmp(path_text, value)) {
        snprintf(path_text, sizeof(path_text), "%s", value);
        lv_obj_invalidate(monitor_content);
    }
}

/* Both parts use fixed buffers and one draw callback, without extra widgets. */
static void draw_path(lv_event_t *event)
{
    lv_obj_t *screen = lv_event_get_target(event);
    lv_area_t bounds;
    lv_obj_get_coords(screen, &bounds);
    for (unsigned part = 0; part < 2; ++part) {
        const lv_font_t *font = part ? &lv_font_montserrat_14 : &fmo_channel_font;
        const char *value = part ? path_text : location_text;
        int first = INT32_MAX, last = INT32_MIN;
        uint32_t index = 0;
        while (value[index]) {
            uint32_t code = lv_text_encoded_next(value, &index);
            lv_font_glyph_dsc_t glyph;
            if (!lv_font_get_glyph_dsc(font, &glyph, code, 0) || !glyph.box_h) continue;
            int top = font->line_height - font->base_line - glyph.box_h - glyph.ofs_y;
            if (top < first) first = top;
            if (top + glyph.box_h > last) last = top + glyph.box_h;
        }
        if (first == INT32_MAX) continue;
        lv_area_t area = {
            .x1 = bounds.x1 + (part ? path_left : 12),
            .x2 = bounds.x1 + (part ? 227 : path_left - 9),
            .y1 = bounds.y1 + LOCATION_TOP + (LOCATION_HEIGHT - (last - first)) / 2 - first,
        };
        area.y2 = area.y1 + font->line_height - 1;
        lv_draw_label_dsc_t dsc;
        lv_draw_label_dsc_init(&dsc);
        dsc.base.obj = screen;
        dsc.base.id1 = part ? 0x50415448 : 0x504C4143; /* PATH / PLAC */
        dsc.font = font;
        dsc.color = lv_color_hex(MUTED);
        dsc.text = value;
        dsc.text_local = true;
        dsc.flag = LV_TEXT_FLAG_EXPAND; /* One line, even when the full name is wider. */
        lv_text_get_size(&dsc.text_size, value, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        dsc.ofs_x = part ? 0 : -location_offset;
        lv_layer_t *layer = lv_event_get_layer(event);
        lv_area_t original_clip = layer->_clip_area, clip;
        lv_area_t viewport = area;
        viewport.y1 = bounds.y1 + LOCATION_TOP;
        viewport.y2 = viewport.y1 + LOCATION_HEIGHT - 1;
        if (lv_area_intersect(&clip, &viewport, &original_clip)) {
            layer->_clip_area = clip;
            lv_draw_label(layer, &dsc, &area);
            layer->_clip_area = original_clip;
        }
    }
}

static void create_monitor(void)
{
    /* Widgets are recreated after overlays; cached layout/date state must not
     * suppress the first render even when the network snapshot is unchanged. */
    callsign_layout_valid = false;
    path_cache_valid = false;
    path_left = 12;
    location_offset = location_overflow = 0;
    location_scroll_started = 0;
    memset(qso_stamps, 0, sizeof(qso_stamps));
    memset(qso_present, 0, sizeof(qso_present));
    lv_obj_t *screen = rect(lv_screen_active(), 0, 0, 240, 320, BLACK);
    monitor_content = screen;
    lv_obj_move_background(screen);
    link_label = label(screen, 12, LINK_TOP, 120, &fmo_channel_font, MUTED, "正在连接 Wi-Fi");
    center_ink(link_label, LINK_TOP, LINK_HEIGHT, 'H');
    audio_label = label(screen, 140, LINK_TOP, 88, fmo_wifi_font(), MUTED, "音频：0%");
    lv_obj_set_style_text_align(audio_label, LV_TEXT_ALIGN_RIGHT, 0);
    center_ink(audio_label, LINK_TOP, LINK_HEIGHT, 'H');
    lv_obj_t *channel = rect(screen, 12, CHANNEL_TOP, 216, CHANNEL_HEIGHT, ORANGE);
    channel_label = label(channel, 8, 0, 200, &fmo_channel_font, BLACK, "--");
    lv_obj_set_style_text_align(channel_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(channel_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    center_ink(channel_label, 0, CHANNEL_HEIGHT, 'H');
    snprintf(location_text, sizeof(location_text), "--");
    qso_panel = rect(screen, 12, QSO_TOP, 216, 107, BLACK);
    lv_obj_set_style_border_color(qso_panel, lv_color_hex(ORANGE), 0);
    lv_obj_set_style_border_width(qso_panel, 1, 0);
    lv_obj_set_style_radius(qso_panel, 8, 0);
    lv_obj_t *history_title = label(qso_panel, 10, 8, 196, &fmo_channel_font, ORANGE, "QSO");
    center_ink(history_title, 8, 16, 'H');
    for (unsigned i = 0; i < FMO_HISTORY_COUNT; ++i) {
        int top = QSO_ROWS_TOP + QSO_ROW_HEIGHT * i;
        qso_labels[i] = label(qso_panel, 10, top, 196,
                             &fmo_callsign_bold_14, MUTED, "--");
        lv_label_set_long_mode(qso_labels[i], LV_LABEL_LONG_CLIP);
        lv_obj_set_height(qso_labels[i], fmo_callsign_bold_14.line_height);
        center_ink(qso_labels[i], top, QSO_TEXT_HEIGHT, 'H');
        memcpy(qso_times[i], "---------- --:--:--", sizeof(qso_times[i]));
    }
    snprintf(path_text, sizeof(path_text), "--° --  -- km");
    lv_obj_add_event_cb(screen, draw_path, LV_EVENT_DRAW_MAIN_END, NULL);
    lv_obj_add_event_cb(qso_panel, draw_qso_times, LV_EVENT_DRAW_MAIN_END, NULL);
    air_label = label(screen, 12, AIR_TOP, 216, &fmo_channel_font, MUTED, "等待电台发言");
    center_ink(air_label, AIR_TOP, AIR_HEIGHT, 'H');
    callsign_label = label(screen, 12, CALLSIGN_TOP, 216, &fmo_callsign_bold_32, WHITE, "--");
    lv_label_set_long_mode(callsign_label, LV_LABEL_LONG_CLIP);
    center_ink(callsign_label, CALLSIGN_TOP, 32, 'H');
    grid_label = label(screen, 12, CALLSIGN_TOP, 80, &fmo_channel_font, GRID_COLOR, "");
    lv_label_set_long_mode(grid_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_height(grid_label, fmo_channel_font.line_height);
    lv_obj_add_flag(grid_label, LV_OBJ_FLAG_HIDDEN);
    audio_bar_width = 0;
    lv_obj_add_event_cb(screen, draw_audio_bar, LV_EVENT_DRAW_MAIN_END, NULL);
    fmo_audio_meter_reset(&audio_meter);
    audio_meter_enabled = false;
    hint_label = label(screen, 12, FOOTER_TOP, 216, fmo_wifi_font(), MUTED,
                       "确认: 台站列表 长按确认: 配网");
    lv_obj_set_style_text_align(hint_label, LV_TEXT_ALIGN_CENTER, 0);
    center_ink(hint_label, FOOTER_TOP, 18, 'H');
}

void fmo_ui_render(const fmo_monitor_state_t *s, const char *error,
                   const char *setup_ssid, const char *setup_password,
                   int battery, uint64_t now_ms, bool sync_hint, const fmo_controls_t *controls)
{
    char buffer[80];
    bool setup = setup_ssid[0] != 0;
    // Live speech belongs to /events; channel confirmation belongs to /ws.
    // Keep receiving speech visible while channel metadata recovers, but never
    // present the cached channel name as confirmed until a fresh reply arrives.
    bool live = !controls->stations.audio_paused && !setup && !error[0] && s->wifi_connected && s->events_connected;
    bool channel_ready = live && s->control_connected && s->channel_valid;
    bool speaking = live && s->speaking;
    const char *link = controls->stations.audio_paused ? "正在确认台站" : setup ? "手机配网" : error[0] ? "网络连接异常" :
        !s->wifi_connected ? "正在连接 Wi-Fi" :
        !s->events_connected ? "正在连接 FMO" : !channel_ready ? "频道确认中" :
        sync_hint ? (controls->station_already_current ? "已是当前台站" : "已切换到新台站") : "FMO 已连接";

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
    audio_meter_enabled = controls->view == FMO_VIEW_MONITOR && !setup && !error[0] &&
                          s->wifi_connected && !controls->stations.audio_paused && controls->audio_enabled && controls->volume > 0;
    if (!audio_meter_enabled) fmo_ui_set_audio_level(0, now_ms);
    if (controls->view == FMO_VIEW_STATIONS) render_stations(controls, s);
    if (controls->view != FMO_VIEW_MONITOR) return;
    if (text(link_label, link)) center_ink(link_label, LINK_TOP, LINK_HEIGHT, 'H');
    color(link_label, error[0] ? RED : channel_ready ? WHITE : ORANGE);
    snprintf(buffer, sizeof(buffer), "音频：%u%%", controls->audio_enabled ? controls->volume : 0);
    if (text(audio_label, buffer)) center_ink(audio_label, LINK_TOP, LINK_HEIGHT, 'H');
    if (setup) snprintf(buffer, sizeof(buffer), "%s", setup_ssid);
    else if (!channel_ready) snprintf(buffer, sizeof(buffer), "等待频道确认");
    else if (s->channel_name[0]) snprintf(buffer, sizeof(buffer), "%s", s->channel_name);
    else snprintf(buffer, sizeof(buffer), "频道 #%" PRIu32, s->channel_uid);
    if (text(channel_label, buffer)) {
        bool non_ascii = false;
        for (const unsigned char *p = (const unsigned char *)buffer; *p; ++p)
            if (*p >= 0x80) { non_ascii = true; break; }
        center_ink(channel_label, 0, CHANNEL_HEIGHT, non_ascii ? 0x4E2D : 'H');
    }
    const char *call = "--";
    const char *air = live ? "等待电台发言" : "等待频道同步";
    if (setup) {
        air = "热点密码";
        call = setup_password;
    } else if (speaking) {
        air = "正在通联";
        call = s->speaker;
    } else if (live && s->last_speaker[0]) {
        air = "上次通联";
        call = s->last_speaker;
    } else if (error[0]) {
        air = !strcmp(error, "DATA RESET FAILED") ?
                 "数据清理失败 请重试" : !strcmp(error, "SETUP SAVE FAILED") ?
                 "配网设置保存失败" : "网络启动失败 请重试";
    }
    if (text(air_label, air)) center_ink(air_label, AIR_TOP, AIR_HEIGHT, 'H');
    color(air_label, speaking || setup ? ORANGE : MUTED);
    bool call_changed = text(callsign_label, call);
    color(callsign_label, speaking ? (s->speaker_cross_server ? CROSS_SERVER_COLOR : ORANGE) : WHITE);
    /* Keep callsigns at the original 32px; only the grid may become smaller.
     * Six-character alphanumeric callsigns fit without scrolling or ellipsis. */
    const lv_font_t *large = setup ? &lv_font_montserrat_32 : &fmo_callsign_bold_32;
    /* A release retains the last speaker's grid; the next start replaces it,
     * including clearing it when that talker supplies no grid. */
    const char *grid = live && s->last_speaker[0] ? s->grid : "";
    const char *location = live && s->last_speaker[0] && s->location[0] ? s->location : "--";
    render_path(s, live);
    char compact[FMO_LOCATION_MAX + 4];
    fmo_location_short_name(location, compact, sizeof(compact));
    lv_point_t place_size, path_size;
    lv_text_get_size(&place_size, compact, &fmo_channel_font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_text_get_size(&path_size, path_text, &lv_font_montserrat_14, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    int available = 216 - path_size.x - 8;
    /* Reserve the complete bearing/distance. Long names scroll in the remaining
     * viewport; unchanged network snapshots must not restart the animation. */
    if (available < 1) available = 1;
    int place_width = place_size.x > available ? available : place_size.x;
    int next_path_left = 12 + place_width + 8;
    if (strcmp(location_text, compact) || path_left != next_path_left) {
        snprintf(location_text, sizeof(location_text), "%s", compact);
        path_left = next_path_left;
        location_overflow = place_size.x - place_width;
        location_offset = 0;
        location_scroll_started = now_ms;
        lv_obj_invalidate(monitor_content);
    }
    bool grid_changed = text(grid_label, grid);
    if (!callsign_layout_valid || call_changed || grid_changed || setup != callsign_setup) {
        lv_point_t size;
        lv_text_get_size(&size, call, large, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        const lv_font_t *font = setup && size.x > 216 ? &lv_font_montserrat_20 : large;
        if (lv_obj_get_style_text_font(callsign_label, 0) != font)
            lv_obj_set_style_text_font(callsign_label, font, 0);
        if (font != large)
            lv_text_get_size(&size, call, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        int call_width = size.x < 216 ? size.x : 216;
        lv_obj_set_width(callsign_label, call_width);
        lv_obj_set_height(callsign_label, font->line_height);
        center_ink(callsign_label, CALLSIGN_TOP, 32, 'H');
        int grid_space = 216 - call_width - 4;
        if (grid[0] && grid_space > 0) {
            /* Pre-rasterized sizes avoid transformed layers and runtime bitmap
             * allocations on the no-PSRAM board. Preserve the full grid text. */
            static const lv_font_t *const fonts[] = {
                &fmo_channel_font, &fmo_grid_14, &fmo_grid_12, &fmo_grid_10,
                &fmo_grid_8, &fmo_grid_6, &fmo_grid_4, &fmo_grid_2
            };
            const lv_font_t *grid_font = fonts[0];
            lv_point_t grid_size;
            int grid_letter_space = 0;
            for (unsigned i = 0; i < sizeof(fonts) / sizeof(fonts[0]); ++i) {
                grid_font = fonts[i];
                grid_letter_space = 0;
                lv_text_get_size(&grid_size, grid, grid_font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
                if (grid_size.x <= grid_space) break;
                /* Tighten only the grid before trying a smaller raster size. */
                grid_letter_space = -1;
                lv_text_get_size(&grid_size, grid, grid_font, -1, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
                if (grid_size.x <= grid_space) break;
            }
            lv_obj_remove_flag(grid_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_text_font(grid_label, grid_font, 0);
            lv_obj_set_style_text_letter_space(grid_label, grid_letter_space, 0);
            lv_obj_set_x(grid_label, 12 + call_width + 4);
            lv_obj_set_width(grid_label, grid_size.x < grid_space ? grid_size.x : grid_space);
            lv_obj_set_height(grid_label, grid_font->line_height);
            center_ink(grid_label, CALLSIGN_TOP, 32, 'H');
        } else lv_obj_add_flag(grid_label, LV_OBJ_FLAG_HIDDEN);
        callsign_layout_valid = true;
        callsign_setup = setup;
    }
    bool history_online = !setup && !error[0] && s->wifi_connected && s->events_connected;
    for (unsigned i = 0; i < FMO_HISTORY_COUNT; ++i) {
        bool present = history_online && i < s->history.count;
        int64_t stamp = present ? s->history.entries[i].timestamp : 0;
        if (present != qso_present[i] || stamp != qso_stamps[i]) {
            char formatted[20];
            if (present) fmo_clock_format_history(stamp, formatted);
            else memcpy(formatted, "---------- --:--:--", sizeof(formatted));
            if (strcmp(qso_times[i], formatted)) {
                memcpy(qso_times[i], formatted, sizeof(formatted));
                lv_obj_invalidate(qso_panel);
            }
            qso_present[i] = present;
            qso_stamps[i] = stamp;
        }
        if (text(qso_labels[i], present ? s->history.entries[i].callsign : "--")) {
            int top = QSO_ROWS_TOP + QSO_ROW_HEIGHT * i;
            center_ink(qso_labels[i], top, QSO_TEXT_HEIGHT, 'H');
        }
        color(qso_labels[i], present ? WHITE : MUTED);
    }
    if (text(hint_label, "确认: 台站列表 长按确认: 配网"))
        center_ink(hint_label, FOOTER_TOP, 18, 'H');
}
