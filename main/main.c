// FMO live monitor for FoloToy AI Passport.
//
// The application owns one screen for its lifetime. Network and battery tasks
// communicate with the LVGL timer through fixed-size queues; they never access
// LVGL objects directly.
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "fmo_monitor_state.h"
#include "fmo_network.h"
#include "ui_pixel.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lvgl.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "fmo_monitor";

typedef enum {
    APP_INPUT_BUTTON,
    APP_INPUT_BATTERY,
} app_input_type_t;

typedef struct {
    app_input_type_t type;
    int value;
} app_input_t;

static QueueHandle_t s_fmo_queue;
static QueueHandle_t s_input_queue;
static fmo_monitor_state_t s_state;
static char s_error[48];
static int s_battery_soc = -1;
static int s_brightness = CONFIG_FMO_BACKLIGHT;

static lv_obj_t *s_link_label;
static lv_obj_t *s_battery_label;
static lv_obj_t *s_channel_label;
static lv_obj_t *s_air_label;
static lv_obj_t *s_callsign_label;
static lv_obj_t *s_detail_label;
static lv_obj_t *s_hint_label;
static uint64_t s_last_render_second = UINT64_MAX;
static uint64_t s_hint_until_ms;

/* LVGL copies text on every set, even when equal. Avoid needless allocations. */
static void label_text(lv_obj_t *label, const char *text)
{
    if (strcmp(lv_label_get_text(label), text) != 0) lv_label_set_text(label, text);
}

static void label_color(lv_obj_t *label, uint32_t color)
{
    if (!lv_color_eq(lv_obj_get_style_text_color(label, 0), lv_color_hex(color)))
        lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
}

static uint64_t monotonic_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000);
}

static void render_link(void)
{
    const char *text;
    uint32_t color;
    if (s_error[0] != '\0') {
        text = s_error;
        color = UI_RED;
    } else if (!s_state.wifi_connected) {
        text = "CONNECTING WIFI";
        color = UI_ORANGE;
    } else if (!s_state.events_connected) {
        text = "CONNECTING FMO";
        color = UI_ORANGE;
    } else if (!s_state.control_connected || !s_state.channel_valid) {
        text = "SYNCING CHANNEL";
        color = UI_ORANGE;
    } else {
        text = "LIVE";
        color = UI_GRASS_DARK;
    }
    label_text(s_link_label, text);
    label_color(s_link_label, color);
}

static void render_channel(void)
{
    char text[64];
    if (!s_state.channel_valid) {
        snprintf(text, sizeof(text), "--");
    } else if (s_state.channel_name[0] != '\0') {
        snprintf(text, sizeof(text), "%s", s_state.channel_name);
    } else if (s_state.channel_uid != 0) {
        snprintf(text, sizeof(text), "CHANNEL #%" PRIu32, s_state.channel_uid);
    } else {
        snprintf(text, sizeof(text), "--");
    }
    label_text(s_channel_label, text);
}

static void render_speaker(uint64_t now_ms)
{
    char detail[64];
    if (!s_state.channel_valid || !s_state.events_connected) {
        label_text(s_air_label, "WAITING FOR SYNC");
        label_color(s_air_label, UI_ORANGE);
        label_text(s_callsign_label, "--");
        detail[0] = '\0';
    } else if (s_state.speaking) {
        label_text(s_air_label, "ON AIR");
        label_color(s_air_label, UI_RED);
        label_text(s_callsign_label, s_state.speaker);
        if (s_state.grid[0]) {
            snprintf(detail, sizeof(detail), "%s | %s",
                     s_state.speaker_is_host ? "HOST" : "REMOTE", s_state.grid);
        } else {
            snprintf(detail, sizeof(detail), "%s",
                     s_state.speaker_is_host ? "HOST" : "REMOTE");
        }
    } else if (s_state.last_speaker[0] != '\0') {
        uint64_t age_seconds = now_ms >= s_state.last_speaker_ms
                                   ? (now_ms - s_state.last_speaker_ms) / 1000
                                   : 0;
        label_text(s_air_label, "LAST HEARD");
        label_color(s_air_label, UI_SKY_DARK);
        label_text(s_callsign_label, s_state.last_speaker);
        if (s_state.grid[0]) {
            snprintf(detail, sizeof(detail), "%s | %" PRIu64 "s AGO",
                     s_state.grid, age_seconds);
        } else {
            snprintf(detail, sizeof(detail), "%" PRIu64 "s AGO", age_seconds);
        }
    } else {
        label_text(s_air_label, "LISTENING");
        label_color(s_air_label, UI_GRASS_DARK);
        label_text(s_callsign_label, "--");
        snprintf(detail, sizeof(detail), "WAITING FOR A CALLSIGN");
    }
    label_text(s_detail_label, detail);
}

static void render_battery(void)
{
    if (s_battery_soc >= 0) {
        char text[16];
        snprintf(text, sizeof(text), "BAT %d%%", s_battery_soc);
        label_text(s_battery_label, text);
    } else {
        label_text(s_battery_label, "BAT --");
    }
}

static void apply_input(const app_input_t *input)
{
    if (!input) return;
    if (input->type == APP_INPUT_BATTERY) {
        s_battery_soc = input->value;
        return;
    }

    bsp_btn_t button = (bsp_btn_t)input->value;
    if (button == BSP_BTN_UP) {
        s_brightness += 10;
        if (s_brightness > 100) s_brightness = 100;
        bsp_display_backlight((uint8_t)s_brightness);
    } else if (button == BSP_BTN_DOWN) {
        s_brightness -= 10;
        if (s_brightness < 20) s_brightness = 20;
        bsp_display_backlight((uint8_t)s_brightness);
    } else if (button == BSP_BTN_OK) {
        fmo_network_request_refresh();
        s_hint_until_ms = monotonic_ms() + 3000;
        label_text(s_hint_label, "SYNC REQUESTED");
    }
}

static void ui_tick(lv_timer_t *timer)
{
    (void)timer;
    bool dirty = false;
    fmo_snapshot_t snapshot;
    if (xQueueReceive(s_fmo_queue, &snapshot, 0) == pdTRUE) {
        s_state = snapshot.state;
        snprintf(s_error, sizeof(s_error), "%s", snapshot.error);
        dirty = true;
    }

    app_input_t input;
    for (unsigned i = 0; i < 4 && xQueueReceive(s_input_queue, &input, 0) == pdTRUE; ++i) {
        apply_input(&input);
        dirty = true;
    }

    uint64_t now_ms = monotonic_ms();
    uint64_t current_second = now_ms / 1000;
    if (dirty || current_second != s_last_render_second) {
        render_link();
        render_channel();
        render_speaker(now_ms);
        render_battery();
        if (now_ms >= s_hint_until_ms) {
            label_text(s_hint_label, "UP/DN LIGHT  OK SYNC");
        }
        s_last_render_second = current_second;
    }
}

static lv_obj_t *create_centered_label(lv_obj_t *parent, const char *text,
                                       const lv_font_t *font, int y)
{
    lv_obj_t *label = ui_pixel_label(parent, text, font, UI_INK);
    lv_obj_set_width(label, 188);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
    return label;
}

static void build_ui(void)
{
    lv_obj_t *screen = ui_pixel_screen_create("FMO LIVE");

    s_battery_label = ui_pixel_label(screen, "BAT --", &lv_font_montserrat_14,
                                     0xFFFFFF);
    lv_obj_set_pos(s_battery_label, 168, 29);

    lv_obj_t *panel = ui_pixel_panel_create(screen, 12, 55, 216, 174, UI_PAPER);
    s_link_label = create_centered_label(panel, "CONNECTING WIFI",
                                         &lv_font_montserrat_14, 0);

    lv_obj_t *caption = create_centered_label(panel, "CHANNEL",
                                               &lv_font_montserrat_14, 25);
    lv_obj_set_style_text_color(caption, lv_color_hex(UI_SKY_DARK), 0);
    s_channel_label = create_centered_label(panel, "--",
                                             &lv_font_montserrat_20, 44);
    lv_label_set_long_mode(s_channel_label, LV_LABEL_LONG_SCROLL_CIRCULAR);

    s_air_label = create_centered_label(panel, "LISTENING",
                                         &lv_font_montserrat_14, 81);
    s_callsign_label = create_centered_label(panel, "--",
                                              &lv_font_montserrat_20, 105);
    s_detail_label = create_centered_label(panel, "WAITING FOR A CALLSIGN",
                                            &lv_font_montserrat_14, 134);

    lv_obj_t *hint_panel = ui_pixel_panel_create(screen, 12, 239, 168, 36, UI_MUTED);
    s_hint_label = ui_pixel_label(hint_panel, "UP/DN LIGHT  OK SYNC",
                                  &lv_font_montserrat_14, UI_INK);
    lv_obj_set_width(s_hint_label, 145);
    lv_obj_set_style_text_align(s_hint_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_hint_label);
    ui_pixel_mascot_create(screen, 190, 238);

    lv_screen_load(screen);
    lv_timer_create(ui_tick, 200, NULL);
}

static void button_event(bsp_btn_t button, bsp_btn_ev_t event, void *user)
{
    (void)user;
    if (event != BSP_BTN_CLICK || !s_input_queue) return;
    app_input_t input = { .type = APP_INPUT_BUTTON, .value = button };
    xQueueSend(s_input_queue, &input, 0);
}

static void battery_task(void *argument)
{
    (void)argument;
    for (;;) {
        app_input_t input = {
            .type = APP_INPUT_BATTERY,
            .value = bsp_battery_soc(),
        };
        xQueueSend(s_input_queue, &input, 0);
        ESP_LOGD(TAG, "Battery stack free=%u", (unsigned)uxTaskGetStackHighWaterMark(NULL));
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting FMO live monitor");
    fmo_monitor_state_init(&s_state);

    if (bsp_i2c_init() != ESP_OK) {
        ESP_LOGW(TAG, "Shared I2C initialization failed; battery will be unavailable");
    }
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "Display/LVGL initialization failed (MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight((uint8_t)s_brightness);

    s_fmo_queue = xQueueCreate(1, sizeof(fmo_snapshot_t));
    s_input_queue = xQueueCreate(4, sizeof(app_input_t));
    if (!s_fmo_queue || !s_input_queue) {
        ESP_LOGE(TAG, "Unable to allocate application queues");
        return;
    }

    if (bsp_lvgl_lock(1000)) {
        build_ui();
        bsp_lvgl_unlock();
    } else {
        ESP_LOGE(TAG, "Unable to acquire LVGL during startup");
        return;
    }

    if (bsp_button_init(button_event, NULL) != ESP_OK) {
        ESP_LOGW(TAG, "Buttons unavailable; monitor will continue automatically");
    }

    if (bsp_battery_init() == ESP_OK) {
        if (xTaskCreate(battery_task, "battery_monitor", 2048, NULL, 2, NULL) != pdPASS)
            ESP_LOGW(TAG, "Battery worker unavailable");
    }

    esp_err_t err = fmo_network_start(s_fmo_queue);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "FMO network task failed: %s", esp_err_to_name(err));
        fmo_snapshot_t snapshot = { 0 };
        snprintf(snapshot.error, sizeof(snapshot.error), "NETWORK START FAILED");
        xQueueOverwrite(s_fmo_queue, &snapshot);
    }
}
