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
#include "fmo_display_policy.h"
#include "fmo_audio.h"
#include "fmo_controls.h"
#include "fmo_monitor_state.h"
#include "fmo_network.h"
#include "fmo_ui.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lvgl.h"

#include <inttypes.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *TAG = "fmo_monitor";

typedef enum {
    APP_INPUT_BUTTON,
    APP_INPUT_BATTERY,
} app_input_type_t;

typedef struct {
    app_input_type_t type;
    int value;
    bsp_btn_ev_t event;
} app_input_t;

static QueueHandle_t s_fmo_queue;
static QueueHandle_t s_input_queue;
static fmo_monitor_state_t s_state;
static fmo_controls_t s_controls = {.audio_enabled = true, .volume = CONFIG_FMO_VOLUME};
static uint32_t s_speech_activity;
static char s_error[48];
static char s_setup_ssid[33];
static char s_setup_password[17];
static int s_battery_soc = -1;
static int s_brightness = CONFIG_FMO_BACKLIGHT;
static fmo_display_policy_t s_display_policy;
static int s_applied_brightness = -1;
static atomic_bool s_display_dark;
static bool s_ignore_gesture[BSP_BTN_COUNT];
static uint64_t s_display_retry_ms;
static TaskHandle_t s_battery_task;

static uint64_t s_last_render_second = UINT64_MAX;
static uint64_t s_hint_until_ms;

static uint64_t monotonic_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000);
}

static void apply_input(const app_input_t *input)
{
    if (!input) return;
    if (input->type == APP_INPUT_BATTERY) {
        s_battery_soc = input->value;
        return;
    }

    bsp_btn_t button = (bsp_btn_t)input->value;
    if ((unsigned)button >= BSP_BTN_COUNT) return;
    fmo_display_policy_touch(&s_display_policy, monotonic_ms());
    if (input->event == BSP_BTN_PRESS) {
        // Consume the complete wake gesture, including a subsequent long press.
        if (atomic_load(&s_display_dark)) s_ignore_gesture[button] = true;
        return;
    }
    if (s_ignore_gesture[button]) {
        s_ignore_gesture[button] = false;
        return;
    }
    fmo_key_t key;
    if (input->event == BSP_BTN_LONG && button == BSP_BTN_UP) key = FMO_KEY_REFRESH;
    else if (input->event == BSP_BTN_LONG && button == BSP_BTN_OK) key = FMO_KEY_BACK;
    else if (input->event == BSP_BTN_CLICK) key = button == BSP_BTN_UP ? FMO_KEY_UP :
        button == BSP_BTN_DOWN ? FMO_KEY_DOWN : FMO_KEY_OK;
    else return;
    switch (fmo_controls_key(&s_controls, key)) {
    case FMO_ACTION_VOLUME:
    case FMO_ACTION_AUDIO:
        fmo_audio_set_volume(s_controls.audio_enabled ? s_controls.volume : 0);
        break;
    case FMO_ACTION_REFRESH:
        fmo_network_request_refresh();
        s_hint_until_ms = monotonic_ms() + 3000;
        break;
    case FMO_ACTION_SETUP: fmo_network_request_setup(); break;
    case FMO_ACTION_RETRY: fmo_network_request_retry(); break;
    case FMO_ACTION_CANCEL: fmo_network_cancel_setup(); break;
    case FMO_ACTION_NONE: break;
    }
}

// Called only by the LVGL timer. Stop refresh before panel sleep, and keep the
// backlight dark until a full redraw has completed on wake. Network tasks stay on.
static bool update_display(uint64_t now)
{
    bool keep_awake = s_setup_ssid[0] || s_controls.view == FMO_VIEW_NETWORK ||
                      (s_state.wifi_connected && s_state.events_connected && s_state.speaking);
    fmo_display_level_t target = fmo_display_policy_step(&s_display_policy, now, keep_awake);
    bool dark = atomic_load(&s_display_dark);
    if ((target == FMO_DISPLAY_DARK) != dark) {
        if (now < s_display_retry_ms) return !dark;
        lv_display_t *display = lv_display_get_default();
        lv_timer_t *refresh = lv_display_get_refr_timer(display);
        if (target == FMO_DISPLAY_DARK) lv_timer_pause(refresh);
        esp_err_t err = bsp_display_sleep(target == FMO_DISPLAY_DARK);
        if (err != ESP_OK) {
            if (!dark) {
                lv_timer_resume(refresh);
                bsp_display_backlight((uint8_t)s_brightness);
            }
            s_applied_brightness = -1;
            s_display_retry_ms = now + 1000;
            return !dark;
        }
        s_display_retry_ms = 0;
        s_applied_brightness = -1;
        atomic_store(&s_display_dark, target == FMO_DISPLAY_DARK);
        if (target != FMO_DISPLAY_DARK) {
            lv_timer_resume(refresh);
            lv_obj_invalidate(lv_screen_active());
            fmo_ui_set_clock((int64_t)time(NULL));
            fmo_ui_render(&s_state, s_error, s_setup_ssid, s_setup_password,
                          s_battery_soc, now, now < s_hint_until_ms, &s_controls);
            lv_refr_now(display);
            s_last_render_second = now / 1000;
            if (s_battery_task) xTaskNotifyGive(s_battery_task);
        }
    }
    if (target == FMO_DISPLAY_DARK) return false;
    uint8_t brightness = (uint8_t)s_brightness;
    if (target == FMO_DISPLAY_DIM && brightness > 20) brightness = 20;
    if (s_applied_brightness != brightness) {
        bsp_display_backlight(brightness);
        s_applied_brightness = brightness;
    }
    return true;
}

static void ui_tick(lv_timer_t *timer)
{
    (void)timer;
    bool dirty = false;
    fmo_snapshot_t snapshot;
    if (xQueueReceive(s_fmo_queue, &snapshot, 0) == pdTRUE) {
        if (snapshot.speech_activity != s_speech_activity) {
            fmo_display_policy_touch(&s_display_policy, monotonic_ms());
            s_speech_activity = snapshot.speech_activity;
        }
        s_state = snapshot.state;
        snprintf(s_setup_ssid, sizeof(s_setup_ssid), "%s", snapshot.setup_ssid);
        snprintf(s_setup_password, sizeof(s_setup_password), "%s", snapshot.setup_password);
        snprintf(s_error, sizeof(s_error), "%s", snapshot.error);
        fmo_controls_observe_setup(&s_controls, s_setup_ssid[0] != 0);
        dirty = true;
    }

    app_input_t input;
    for (unsigned i = 0; i < 8 && xQueueReceive(s_input_queue, &input, 0) == pdTRUE; ++i) {
        apply_input(&input);
        dirty = true;
    }

    fmo_audio_set_online(s_state.wifi_connected && s_state.events_connected &&
                         s_state.control_connected && !s_setup_ssid[0] && !s_error[0]);
    uint64_t now_ms = monotonic_ms();
    if (!update_display(now_ms)) return;
    uint64_t current_second = now_ms / 1000;
    if (dirty || current_second != s_last_render_second) {
        fmo_ui_set_clock((int64_t)time(NULL));
        fmo_ui_render(&s_state, s_error, s_setup_ssid, s_setup_password,
                      s_battery_soc, now_ms, now_ms < s_hint_until_ms, &s_controls);
        s_last_render_second = current_second;
    }
}

static void build_ui(void)
{
    fmo_display_policy_touch(&s_display_policy, monotonic_ms());
    fmo_ui_create();
    lv_mem_monitor_t memory;
    lv_mem_monitor(&memory);
    ESP_LOGI(TAG, "UI ready; LVGL peak=%u free=%u largest=%u",
             (unsigned)memory.max_used, (unsigned)memory.free_size,
             (unsigned)memory.free_biggest_size);
    lv_timer_create(ui_tick, 200, NULL);
}

static void button_event(bsp_btn_t button, bsp_btn_ev_t event, void *user)
{
    (void)user;
    if (!s_input_queue) return;
    app_input_t input = { .type = APP_INPUT_BUTTON, .value = button, .event = event };
    xQueueSend(s_input_queue, &input, 0);
}

static void battery_task(void *argument)
{
    (void)argument;
    for (;;) {
        if (!atomic_load(&s_display_dark)) {
            app_input_t input = { .type = APP_INPUT_BATTERY, .value = bsp_battery_soc() };
            xQueueSend(s_input_queue, &input, 0);
        }
        ESP_LOGD(TAG, "Battery stack free=%u", (unsigned)uxTaskGetStackHighWaterMark(NULL));
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(120000));
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
    s_input_queue = xQueueCreate(8, sizeof(app_input_t));
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
        if (xTaskCreate(battery_task, "battery_monitor", 2048, NULL, 2, &s_battery_task) != pdPASS)
            ESP_LOGW(TAG, "Battery worker unavailable");
    }

    fmo_audio_set_volume(s_controls.audio_enabled ? s_controls.volume : 0);
    esp_err_t audio_err = fmo_audio_start();
    if (audio_err != ESP_OK)
        ESP_LOGW(TAG, "Audio worker unavailable: %s", esp_err_to_name(audio_err));

    esp_err_t err = fmo_network_start(s_fmo_queue);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "FMO network task failed: %s", esp_err_to_name(err));
        fmo_snapshot_t snapshot = { 0 };
        snprintf(snapshot.error, sizeof(snapshot.error), "NETWORK START FAILED");
        xQueueOverwrite(s_fmo_queue, &snapshot);
    }
}
