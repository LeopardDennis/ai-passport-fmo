#include "fmo_audio.h"
#include "fmo_pcm.h"
#include "fmo_audio_meter.h"
#include "fmo_provision.h"
#include "bsp_audio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "fmo_audio";
static atomic_bool s_online;
static atomic_uchar s_volume;
static TaskHandle_t s_task;
static portMUX_TYPE s_pcm_lock = portMUX_INITIALIZER_UNLOCKED;
static fmo_pcm_t s_pcm;
static bool s_accepting; /* Protected with the PCM buffer. */
static uint8_t s_level;
static uint64_t s_level_ms;
static uint32_t s_stream_generation; /* All meter fields share s_pcm_lock. */

static uint64_t now_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000);
}

static void clear_level(void)
{
    taskENTER_CRITICAL(&s_pcm_lock);
    s_level = 0;
    ++s_stream_generation;
    taskEXIT_CRITICAL(&s_pcm_lock);
}

static void reset_stream(bool accepting)
{
    taskENTER_CRITICAL(&s_pcm_lock);
    s_accepting = accepting;
    fmo_pcm_reset(&s_pcm);
    s_level = 0;
    s_level_ms = 0;
    ++s_stream_generation;
    taskEXIT_CRITICAL(&s_pcm_lock);
}

static void audio_event(void *arg, esp_event_base_t base, int32_t id, void *event)
{
    (void)arg; (void)base;
    if (id == WEBSOCKET_EVENT_CONNECTED) {
        reset_stream(true);
        ESP_LOGI(TAG, "Audio stream connected (8 kHz PCM16 mono)");
    } else if (id == WEBSOCKET_EVENT_DISCONNECTED || id == WEBSOCKET_EVENT_CLOSED ||
               id == WEBSOCKET_EVENT_ERROR) {
        reset_stream(false);
    } else if (id == WEBSOCKET_EVENT_DATA) {
        esp_websocket_event_data_t *data = event;
        if (!data) return;
        taskENTER_CRITICAL(&s_pcm_lock);
        if (s_accepting && atomic_load(&s_online) && atomic_load(&s_volume)) {
            fmo_pcm_result_t result = FMO_PCM_INVALID;
            if (data->payload_len < 0 || data->payload_offset < 0 || data->data_len < 0)
                fmo_pcm_reset(&s_pcm);
            else result = fmo_pcm_feed(&s_pcm, data->op_code, data->fin,
                              (size_t)data->payload_len, (size_t)data->payload_offset,
                              data->data_ptr, (size_t)data->data_len, now_ms());
            if (result == FMO_PCM_INVALID) {
                s_level = 0;
                ++s_stream_generation;
            }
        }
        taskEXIT_CRITICAL(&s_pcm_lock);
    }
}

static esp_err_t start_stream(esp_websocket_client_handle_t *client)
{
    fmo_endpoint_t endpoint;
    fmo_provision_get_endpoint(&endpoint);
    char uri[160];
    int n = snprintf(uri, sizeof(uri), "ws://%s:%u/audio", endpoint.host, (unsigned)endpoint.port);
    if (n < 0 || n >= (int)sizeof(uri)) return ESP_ERR_INVALID_ARG;
    esp_websocket_client_config_t config = {
        .uri = uri, .buffer_size = 1024, .task_stack = 4096,
        .enable_close_reconnect = true, .reconnect_timeout_ms = 2000,
        .network_timeout_ms = 3000, .ping_interval_sec = 5, .pingpong_timeout_sec = 10,
    };
    *client = esp_websocket_client_init(&config);
    if (!*client) return ESP_ERR_NO_MEM;
    esp_err_t err = esp_websocket_register_events(*client, WEBSOCKET_EVENT_ANY, audio_event, NULL);
    if (err == ESP_OK) err = esp_websocket_client_start(*client);
    if (err != ESP_OK) {
        esp_websocket_client_destroy(*client);
        *client = NULL;
    }
    return err;
}

static void stop_stream(esp_websocket_client_handle_t *client)
{
    reset_stream(false);
    if (!*client) return;
    /* Stop joins the callback task before destroying its handle. Never delete
     * a worker that might be blocked in codec I/O or a WebSocket callback. */
    esp_websocket_client_stop(*client);
    esp_websocket_client_destroy(*client);
    *client = NULL;
    reset_stream(false);
}

static void audio_task(void *argument)
{
    (void)argument;
    esp_websocket_client_handle_t client = NULL;
    bool codec_attempted = false, codec_ready = false;
    int applied_volume = -1;
    uint64_t retry_ms = 0;
    int16_t output[FMO_PCM_CHUNK_SAMPLES];
    for (;;) {
        unsigned volume = atomic_load(&s_volume);
        bool play = atomic_load(&s_online) && volume;
        if (!play || !codec_ready) {
            if (codec_ready && applied_volume != 0) {
                bsp_audio_set_volume(0);
                applied_volume = 0;
            }
            if (!play) {
                stop_stream(&client);
                retry_ms = 0;
                ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
                continue;
            }
            if (!codec_attempted) {
                /* The BSP has no deinit API. Try initialization once to avoid
                 * leaking partially initialized I2S/codec resources on retries. */
                codec_attempted = true;
                esp_err_t err = bsp_audio_init();
                if (err == ESP_OK) err = bsp_audio_set_format(FMO_PCM_RATE, 16, 1);
                codec_ready = err == ESP_OK;
                if (!codec_ready) ESP_LOGW(TAG, "Audio unavailable: %s", esp_err_to_name(err));
            }
            if (!codec_ready) {
                ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
                continue;
            }
        }
        if (applied_volume <= 0) {
            bsp_audio_set_volume(0);
            applied_volume = 0;
            memset(output, 0, sizeof(output));
            /* Queue 320 ms of silence before unmuting. This exceeds the BSP's
             * six 240-frame DMA buffers (180 ms at 8 kHz), replacing any audio
             * left in hardware even after a quick off/on transition. */
            for (unsigned i = 0; i < 16; ++i) {
                if (!atomic_load(&s_online) || !atomic_load(&s_volume)) break;
                if (bsp_audio_write(output, sizeof(output)) != ESP_OK) {
                    codec_ready = false;
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(1));
            }
            if (!codec_ready) {
                stop_stream(&client);
                ESP_LOGW(TAG, "Audio priming failed; disabled until reboot");
                continue;
            }
            if (!atomic_load(&s_online) || !atomic_load(&s_volume)) continue;
            volume = atomic_load(&s_volume);
        }
        if (!client && now_ms() >= retry_ms) {
            esp_err_t err = start_stream(&client);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Audio connection failed; retrying: %s", esp_err_to_name(err));
                retry_ms = now_ms() + 5000;
            }
        }
        if (applied_volume != (int)volume) {
            bsp_audio_set_volume((uint8_t)volume);
            applied_volume = (int)volume;
        }
        taskENTER_CRITICAL(&s_pcm_lock);
        fmo_pcm_read(&s_pcm, output, FMO_PCM_CHUNK_SAMPLES, now_ms());
        uint32_t generation = s_stream_generation;
        taskEXIT_CRITICAL(&s_pcm_lock);
        uint8_t level = fmo_audio_meter_measure(output, FMO_PCM_CHUNK_SAMPLES);
        /* Check again after receiving PCM so a UI mute/disconnect cannot leave
         * an old chunk queued after the requested transition. */
        if (!atomic_load(&s_online) || !atomic_load(&s_volume)) {
            bsp_audio_set_volume(0);
            applied_volume = 0;
            continue;
        }
        if (bsp_audio_write(output, sizeof(output)) != ESP_OK) {
            bsp_audio_set_volume(0);
            applied_volume = 0;
            stop_stream(&client);
            codec_ready = false;
            ESP_LOGW(TAG, "Playback failed; audio disabled until reboot");
        } else {
            taskENTER_CRITICAL(&s_pcm_lock);
            if (generation == s_stream_generation && s_accepting &&
                atomic_load(&s_online) && atomic_load(&s_volume)) {
                s_level = level;
                s_level_ms = now_ms();
            }
            taskEXIT_CRITICAL(&s_pcm_lock);
        }
        /* I2S normally paces the loop; yield while its DMA initially fills. */
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

esp_err_t fmo_audio_start(void)
{
    if (s_task) return ESP_ERR_INVALID_STATE;
    if (xTaskCreate(audio_task, "fmo_audio", 4096, NULL, 3, &s_task) != pdPASS) {
        s_task = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void fmo_audio_set_online(bool online)
{
    if (atomic_exchange(&s_online, online) != online) {
        if (!online) clear_level();
        if (s_task) xTaskNotifyGive(s_task);
    }
}

void fmo_audio_set_volume(uint8_t percent)
{
    if (percent > 100) percent = 100;
    if (atomic_exchange(&s_volume, percent) != percent) {
        if (!percent) clear_level();
        if (s_task) xTaskNotifyGive(s_task);
    }
}

uint8_t fmo_audio_get_level(void)
{
    uint64_t now = now_ms();
    taskENTER_CRITICAL(&s_pcm_lock);
    uint8_t level = s_accepting && atomic_load(&s_online) && atomic_load(&s_volume) &&
                    now >= s_level_ms && now - s_level_ms <= 120 ? s_level : 0;
    taskEXIT_CRITICAL(&s_pcm_lock);
    return level;
}
