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
#include "fmo_ui.h"

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
static char s_setup_ssid[33];
static char s_setup_password[17];
static int s_battery_soc = -1;
static int s_brightness = CONFIG_FMO_BACKLIGHT;

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
    }
}

static void ui_tick(lv_timer_t *timer)
{
    (void)timer;
    bool dirty = false;
    fmo_snapshot_t snapshot;
    if (xQueueReceive(s_fmo_queue, &snapshot, 0) == pdTRUE) {
        s_state = snapshot.state;
        snprintf(s_setup_ssid, sizeof(s_setup_ssid), "%s", snapshot.setup_ssid);
        snprintf(s_setup_password, sizeof(s_setup_password), "%s", snapshot.setup_password);
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
        fmo_ui_render(&s_state, s_error, s_setup_ssid, s_setup_password,
                      s_battery_soc, now_ms, now_ms < s_hint_until_ms);
        s_last_render_second = current_second;
    }
}

static void build_ui(void)
{
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
    if (button == BSP_BTN_OK && event == BSP_BTN_LONG) {
        fmo_network_request_setup();
        return;
    }
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
        vTaskDelay(pdMS_TO_TICKS(120000));
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
