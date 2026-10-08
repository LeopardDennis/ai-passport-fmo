#include "fmo_audio.h"
#include "fmo_pcm.h"
#include "fmo_audio_meter.h"
#include "fmo_provision.h"
#include "fmo_link_policy.h"
#include "bsp_audio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "fmo_audio";
static atomic_bool s_online;
static atomic_bool s_suspended, s_suspend_ack;
static atomic_uchar s_volume;
static TaskHandle_t s_task;
static portMUX_TYPE s_pcm_lock = portMUX_INITIALIZER_UNLOCKED;
static fmo_pcm_t s_pcm;
static bool s_accepting; /* Protected with the PCM buffer. */
static uint8_t s_level;
static uint64_t s_level_ms;
static uint32_t s_received_bytes, s_dropped_samples, s_invalid_messages;
static uint64_t s_activity_ms;
static uint32_t s_buffer_wait_ms;
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
    s_activity_ms = now_ms();
    fmo_pcm_reset(&s_pcm);
    s_level = 0;
    s_level_ms = 0;
    ++s_stream_generation;
    taskEXIT_CRITICAL(&s_pcm_lock);
}

/* Backpressure runs in the socket task, outside the PCM critical section.
 * Reserve the entire message, so a partial message cannot fill the ring and
 * prevent the playback worker from freeing space. A wedged codec still falls
 * back to bounded overflow after two seconds; mute/offline interrupts promptly. */
static bool wait_for_pcm_space(const esp_websocket_event_data_t *data)
{
    if (data->payload_len < 0 || data->payload_offset < 0) return true;
    size_t reserve = fmo_pcm_message_reserve(data->op_code, data->fin,
                        (size_t)data->payload_len, (size_t)data->payload_offset);
    if (!reserve) return true;
    uint64_t start = now_ms();
    taskENTER_CRITICAL(&s_pcm_lock);
    uint32_t generation = s_stream_generation;
    taskEXIT_CRITICAL(&s_pcm_lock);
    for (;;) {
        taskENTER_CRITICAL(&s_pcm_lock);
        bool accepting = s_accepting && generation == s_stream_generation &&
                         atomic_load(&s_online) && !atomic_load(&s_suspended) && atomic_load(&s_volume);
        bool ready = s_pcm.active ||
                     s_pcm.count + s_pcm.pending_count + reserve <= FMO_PCM_CAPACITY;
        uint64_t elapsed = now_ms() - start;
        if (!accepting || ready || elapsed >= FMO_AUDIO_BUFFER_WAIT_MS) {
            s_buffer_wait_ms += (uint32_t)elapsed;
            taskEXIT_CRITICAL(&s_pcm_lock);
            return accepting;
        }
        taskEXIT_CRITICAL(&s_pcm_lock);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

static bool stream_stalled(void)
{
    taskENTER_CRITICAL(&s_pcm_lock);
    bool stalled = s_accepting && now_ms() - s_activity_ms >= FMO_AUDIO_ACTIVITY_TIMEOUT_MS;
    taskEXIT_CRITICAL(&s_pcm_lock);
    return stalled;
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
        ESP_LOGW(TAG, "Audio stream disconnected (event=%ld)", (long)id);
    } else if (id == WEBSOCKET_EVENT_DATA) {
        esp_websocket_event_data_t *data = event;
        if (!data || !wait_for_pcm_space(data)) return;
        taskENTER_CRITICAL(&s_pcm_lock);
        if (s_accepting && atomic_load(&s_online) && !atomic_load(&s_suspended) && atomic_load(&s_volume)) {
            uint32_t dropped_before = s_pcm.dropped_samples;
            fmo_pcm_result_t result = FMO_PCM_INVALID;
            if (data->payload_len < 0 || data->payload_offset < 0 || data->data_len < 0)
                fmo_pcm_reset(&s_pcm);
            else result = fmo_pcm_feed(&s_pcm, data->op_code, data->fin,
                              (size_t)data->payload_len, (size_t)data->payload_offset,
                              data->data_ptr, (size_t)data->data_len, now_ms());
            if ((result == FMO_PCM_MORE || result == FMO_PCM_COMPLETE) && data->data_len > 0)
                s_activity_ms = now_ms();
            else if (result == FMO_PCM_IGNORED && (data->op_code == 9 || data->op_code == 10))
                s_activity_ms = now_ms();
            if (result == FMO_PCM_INVALID) {
                ++s_invalid_messages;
                s_level = 0;
                ++s_stream_generation;
            } else if (result == FMO_PCM_MORE || result == FMO_PCM_COMPLETE) {
                s_received_bytes += (uint32_t)data->data_len;
                s_dropped_samples += s_pcm.dropped_samples - dropped_before;
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
        .network_timeout_ms = FMO_AUDIO_IO_TIMEOUT_MS,
        .ping_interval_sec = FMO_LINK_PING_INTERVAL_SEC,
        /* A valid PCM stream is also proof of liveness. The library's PONG-only
         * deadline disconnected a receiving stream on the measured device. */
        .disable_pingpong_discon = true,
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
    uint64_t retry_ms = 0, diagnostics_ms = 0, wifi_retry_ms = 0;
    wifi_ps_type_t saved_ps = WIFI_PS_MIN_MODEM;
    bool wifi_awake = false;
    uint32_t written_samples = 0, played_samples = 0, write_max_ms = 0;
    int16_t output[FMO_PCM_CHUNK_SAMPLES];
    for (;;) {
        if (now_ms() - diagnostics_ms >= 30000) {
            diagnostics_ms = now_ms();
            taskENTER_CRITICAL(&s_pcm_lock);
            uint32_t received = s_received_bytes, dropped = s_dropped_samples;
            uint32_t invalid = s_invalid_messages;
            unsigned queued = (unsigned)s_pcm.count, pending = (unsigned)s_pcm.pending_count;
            uint32_t buffer_wait = s_buffer_wait_ms;
            s_buffer_wait_ms = 0;
            s_received_bytes = s_dropped_samples = s_invalid_messages = 0;
            taskEXIT_CRITICAL(&s_pcm_lock);
            /* Aggregate sizes only: never log PCM, callsigns or endpoints. */
            ESP_LOGI(TAG, "Audio stats: rx=%lu bytes dropped=%lu samples invalid=%lu queued=%u pending=%u out=%lu pcm=%lu write_max_ms=%lu buffer_wait_ms=%lu",
                     (unsigned long)received, (unsigned long)dropped,
                     (unsigned long)invalid, queued, pending, (unsigned long)written_samples,
                     (unsigned long)played_samples, (unsigned long)write_max_ms, (unsigned long)buffer_wait);
            written_samples = played_samples = write_max_ms = 0;
        }
        unsigned volume = atomic_load(&s_volume);
        bool play = atomic_load(&s_online) && !atomic_load(&s_suspended) && volume;
        if (wifi_awake && (!play || !codec_ready)) {
            esp_err_t err = esp_wifi_set_ps(saved_ps);
            wifi_awake = false;
            if (err != ESP_OK) ESP_LOGW(TAG, "Wi-Fi power restore failed: %s", esp_err_to_name(err));
        }
        if (!play || !codec_ready) {
            if (codec_ready && applied_volume != 0) {
                bsp_audio_set_volume(0);
                applied_volume = 0;
            }
            if (!play) {
                stop_stream(&client);
                atomic_store(&s_suspend_ack, atomic_load(&s_suspended));
                retry_ms = 0;
                ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
                continue;
            }
            if (!codec_attempted) {
                /* The BSP has no deinit API. Try initialization once to avoid
                 * leaking partially initialized I2S/codec resources on retries. */
                codec_attempted = true;
                esp_err_t err = bsp_audio_init_playback();
                if (err == ESP_OK) err = bsp_audio_set_format(FMO_PCM_RATE, 16, 1);
                codec_ready = err == ESP_OK;
                if (!codec_ready) ESP_LOGW(TAG, "Audio unavailable: %s", esp_err_to_name(err));
            }
            if (!codec_ready) {
                ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
                continue;
            }
        }
        if (!wifi_awake && now_ms() >= wifi_retry_ms) {
            esp_err_t err = esp_wifi_get_ps(&saved_ps);
            if (err == ESP_OK) err = esp_wifi_set_ps(WIFI_PS_NONE);
            wifi_awake = err == ESP_OK;
            if (!wifi_awake) {
                wifi_retry_ms = now_ms() + 5000;
                ESP_LOGW(TAG, "Wi-Fi audio power policy failed: %s", esp_err_to_name(err));
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
                if (!atomic_load(&s_online) || atomic_load(&s_suspended) || !atomic_load(&s_volume)) break;
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
            if (!atomic_load(&s_online) || atomic_load(&s_suspended) || !atomic_load(&s_volume)) continue;
            volume = atomic_load(&s_volume);
        }
        if (client && stream_stalled()) {
            ESP_LOGW(TAG, "Audio reconnect: no PCM or heartbeat activity for %u ms",
                     (unsigned)FMO_AUDIO_ACTIVITY_TIMEOUT_MS);
            bsp_audio_set_volume(0);
            applied_volume = 0;
            stop_stream(&client);
            retry_ms = now_ms() + 2000;
            continue;
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
        size_t available = fmo_pcm_read(&s_pcm, output, FMO_PCM_CHUNK_SAMPLES, now_ms());
        uint32_t generation = s_stream_generation;
        taskEXIT_CRITICAL(&s_pcm_lock);
        uint8_t level = fmo_audio_meter_measure(output, FMO_PCM_CHUNK_SAMPLES);
        /* Check again after receiving PCM so a UI mute/disconnect cannot leave
         * an old chunk queued after the requested transition. */
        if (!atomic_load(&s_online) || atomic_load(&s_suspended) || !atomic_load(&s_volume)) {
            bsp_audio_set_volume(0);
            applied_volume = 0;
            continue;
        }
        uint64_t write_start = now_ms();
        if (bsp_audio_write(output, sizeof(output)) != ESP_OK) {
            bsp_audio_set_volume(0);
            applied_volume = 0;
            stop_stream(&client);
            codec_ready = false;
            ESP_LOGW(TAG, "Playback failed; audio disabled until reboot");
        } else {
            uint32_t write_ms = (uint32_t)(now_ms() - write_start);
            if (write_ms > write_max_ms) write_max_ms = write_ms;
            written_samples += FMO_PCM_CHUNK_SAMPLES;
            played_samples += (uint32_t)available;
            taskENTER_CRITICAL(&s_pcm_lock);
            if (generation == s_stream_generation && s_accepting &&
                atomic_load(&s_online) && !atomic_load(&s_suspended) && atomic_load(&s_volume)) {
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
        ESP_LOGI(TAG, "Audio network ready=%d", online);
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
    uint8_t level = s_accepting && atomic_load(&s_online) && !atomic_load(&s_suspended) && atomic_load(&s_volume) &&
                    now >= s_level_ms && now - s_level_ms <= 120 ? s_level : 0;
    taskEXIT_CRITICAL(&s_pcm_lock);
    return level;
}

void fmo_audio_suspend(bool suspend)
{
    if (atomic_exchange(&s_suspended, suspend) != suspend) {
        if (suspend) clear_level();
        else atomic_store(&s_suspend_ack, false);
        if (s_task) xTaskNotifyGive(s_task);
    }
}

bool fmo_audio_is_suspended(void)
{
    return !s_task || (atomic_load(&s_suspended) && atomic_load(&s_suspend_ack));
}
