#include "fmo_network.h"
#include "fmo_provision.h"
#include "esp_system.h"
#include <stdatomic.h>
#include "lwip/dns.h"
#include "lwip/tcpip.h"

#include "demo_radio.h"
#include "fmo_ws_rx.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/semphr.h"

#include "cJSON.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_websocket_client.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "fmo_network";
static atomic_bool s_reconnect;
static atomic_bool s_setup_requested;
static atomic_bool s_setup_active;

#define WIFI_READY_BIT BIT0
#define FMO_CHANNEL_REFRESH_MS 1000
#define FMO_CHANNEL_MAX_AGE_MS 5000

typedef enum {
    FMO_SOCKET_EVENTS,
    FMO_SOCKET_CONTROL,
} fmo_socket_kind_t;

typedef struct {
    fmo_socket_kind_t kind;
    esp_websocket_client_handle_t client;
    fmo_ws_rx_t rx;
    uint64_t diagnostics_ms;
} fmo_socket_t;

static QueueHandle_t s_update_queue;
static SemaphoreHandle_t s_state_lock;
static fmo_snapshot_t s_snapshot;
static uint32_t s_speaker_revision;
static uint32_t s_query_revision;
static bool s_query_pending;
static uint64_t s_query_ms;
static unsigned s_events_stack_min = UINT32_MAX;
static unsigned s_control_stack_min = UINT32_MAX;

static uint64_t now_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000);
}
static EventGroupHandle_t s_wifi_bits;
static TaskHandle_t s_network_task;
static fmo_socket_t s_events_socket = { .kind = FMO_SOCKET_EVENTS };
static fmo_socket_t s_control_socket = { .kind = FMO_SOCKET_CONTROL };

/* Serialize producers before publishing a one-slot, overwriteable snapshot.
 * A slow UI cannot drop the final transition or restore an older snapshot. */
static void post_update(const fmo_update_t *update)
{
    bool refresh = false;
    xSemaphoreTake(s_state_lock, portMAX_DELAY);
    fmo_monitor_state_t *state = &s_snapshot.state;
    switch (update->type) {
    case FMO_UPDATE_WIFI:
        fmo_monitor_set_wifi(state, update->connected);
        if (!update->connected) s_query_pending = false;
        break;
    case FMO_UPDATE_EVENTS_LINK:
        fmo_monitor_set_events(state, update->connected && state->wifi_connected);
        fmo_monitor_invalidate_channel(state);
        refresh = update->connected;
        break;
    case FMO_UPDATE_CONTROL_LINK:
        fmo_monitor_set_control(state, update->connected && state->wifi_connected);
        s_query_pending = false;
        refresh = update->connected;
        break;
    case FMO_UPDATE_CHANNEL:
        if (s_query_pending && state->wifi_connected && state->control_connected) {
            fmo_monitor_set_channel(state, update->uid, update->text);
            state->channel_confirmed_ms = now_ms();
            if (s_query_revision != s_speaker_revision) {
                fmo_monitor_invalidate_channel(state);
                refresh = true;
            }
        }
        s_query_pending = false;
        break;
    case FMO_UPDATE_SPEAKER:
        if (state->wifi_connected && state->events_connected) {
            if (update->speaking &&
                (!state->speaking || strcmp(state->speaker, update->callsign) != 0)) {
                ++s_speaker_revision;
                fmo_monitor_invalidate_channel(state);
                refresh = true;
            }
            fmo_monitor_apply_speaker(state, update->callsign, update->grid,
                                      update->speaking, update->is_host, now_ms());
        }
        break;
    case FMO_UPDATE_ERROR:
        snprintf(s_snapshot.error, sizeof(s_snapshot.error), "%s", update->text);
        break;
    }
    xQueueOverwrite(s_update_queue, &s_snapshot);
    xSemaphoreGive(s_state_lock);
    if (refresh) fmo_network_request_refresh();
}

static void post_link(fmo_update_type_t type, bool connected)
{
    fmo_update_t update = { .type = type, .connected = connected };
    post_update(&update);
}

static void post_error(const char *message)
{
    fmo_update_t update = { .type = FMO_UPDATE_ERROR };
    snprintf(update.text, sizeof(update.text), "%s", message ? message : "NETWORK ERROR");
    post_update(&update);
}

static bool json_bool(const cJSON *item)
{
    return cJSON_IsTrue(item) || (cJSON_IsNumber(item) && item->valueint != 0);
}

static void copy_ascii(char *destination, size_t destination_size,
                       const char *source)
{
    if (!destination || destination_size == 0) return;
    destination[0] = '\0';
    if (!source) return;

    size_t output = 0;
    for (size_t i = 0; source[i] != '\0' && output + 1 < destination_size; i++) {
        unsigned char value = (unsigned char)source[i];
        if (value < 0x20 || value > 0x7e) {
            destination[0] = '\0';
            return;
        }
        destination[output++] = (char)value;
    }
    destination[output] = '\0';
}

static void parse_fmo_message(fmo_socket_kind_t kind, const char *payload)
{
    cJSON *root = cJSON_Parse(payload);
    if (!root) {
        ESP_LOGW(TAG, "Ignored invalid JSON from %s socket",
                 kind == FMO_SOCKET_EVENTS ? "events" : "control");
        return;
    }

    const cJSON *type = cJSON_GetObjectItemCaseSensitive(root, "type");
    const cJSON *sub_type = cJSON_GetObjectItemCaseSensitive(root, "subType");
    const cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!cJSON_IsString(type) || !cJSON_IsString(sub_type) || !cJSON_IsObject(data)) {
        cJSON_Delete(root);
        return;
    }

    if (kind == FMO_SOCKET_EVENTS && strcmp(type->valuestring, "qso") == 0 &&
        strcmp(sub_type->valuestring, "callsign") == 0) {
        const cJSON *callsign = cJSON_GetObjectItemCaseSensitive(data, "callsign");
        const cJSON *grid = cJSON_GetObjectItemCaseSensitive(data, "grid");
        const cJSON *speaking = cJSON_GetObjectItemCaseSensitive(data, "isSpeaking");
        const cJSON *is_host = cJSON_GetObjectItemCaseSensitive(data, "isHost");
        if (cJSON_IsString(callsign) &&
            (cJSON_IsBool(speaking) || (cJSON_IsNumber(speaking) &&
             (speaking->valuedouble == 0 || speaking->valuedouble == 1)))) {
            fmo_update_t update = {
                .type = FMO_UPDATE_SPEAKER,
                .speaking = json_bool(speaking),
                .is_host = json_bool(is_host),
            };
            copy_ascii(update.callsign, sizeof(update.callsign), callsign->valuestring);
            if (cJSON_IsString(grid)) {
                copy_ascii(update.grid, sizeof(update.grid), grid->valuestring);
            }
            if (update.callsign[0] != '\0') post_update(&update);
        }
    } else if (kind == FMO_SOCKET_CONTROL && strcmp(type->valuestring, "station") == 0 &&
               strcmp(sub_type->valuestring, "getCurrentResponse") == 0) {
        const cJSON *uid = cJSON_GetObjectItemCaseSensitive(data, "uid");
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(data, "name");
        fmo_update_t update = { .type = FMO_UPDATE_CHANNEL };
        if (cJSON_IsNumber(uid) && uid->valuedouble > 0 &&
            uid->valuedouble <= UINT32_MAX &&
            uid->valuedouble == (uint32_t)uid->valuedouble) {
            update.uid = (uint32_t)uid->valuedouble;
        }
        if (cJSON_IsString(name)) copy_ascii(update.text, sizeof(update.text), name->valuestring);
        if (update.uid) post_update(&update);
    }

    cJSON_Delete(root);
}

static void reset_rx(fmo_socket_t *socket)
{
    fmo_ws_rx_reset(&socket->rx);
}

static void receive_fragment(fmo_socket_t *socket,
                             const esp_websocket_event_data_t *data)
{
    if (!socket || !data) return;
    fmo_rx_result_t result = FMO_RX_INVALID;
    if (data->data_len >= 0 && data->payload_len >= 0 && data->payload_offset >= 0) {
        result = fmo_ws_rx_feed(&socket->rx, data->op_code, data->fin,
                                data->payload_len, data->payload_offset,
                                data->data_ptr, data->data_len);
    }
    if (result == FMO_RX_COMPLETE) {
        parse_fmo_message(socket->kind, socket->rx.text);
    } else if (result == FMO_RX_INVALID) {
        reset_rx(socket);
        /* Discarding a message can lose a PTT release: stop claiming live state. */
        xSemaphoreTake(s_state_lock, portMAX_DELAY);
        s_snapshot.state.speaking = false;
        s_snapshot.state.speaker[0] = '\0';
        fmo_monitor_invalidate_channel(&s_snapshot.state);
        xQueueOverwrite(s_update_queue, &s_snapshot);
        xSemaphoreGive(s_state_lock);
        fmo_network_request_refresh();
        ESP_LOGW(TAG, "Dropped invalid or oversized WebSocket message");
    }
}

/* Only the coordinator sends queries. Socket callbacks merely notify it. */
static void request_current_channel(void)
{
    if (!s_control_socket.client ||
        !esp_websocket_client_is_connected(s_control_socket.client)) return;
    static const char request[] =
        "{\"type\":\"station\",\"subType\":\"getCurrent\",\"data\":{}}";
    xSemaphoreTake(s_state_lock, portMAX_DELAY);
    if (s_query_pending && now_ms() - s_query_ms < 2000) {
        xSemaphoreGive(s_state_lock);
        return;
    }
    s_query_pending = true;
    s_query_ms = now_ms();
    s_query_revision = s_speaker_revision;
    xSemaphoreGive(s_state_lock);
    int sent = esp_websocket_client_send_text(s_control_socket.client, request,
                                               sizeof(request) - 1, pdMS_TO_TICKS(250));
    if (sent != sizeof(request) - 1) {
        xSemaphoreTake(s_state_lock, portMAX_DELAY);
        s_query_pending = false;
        fmo_monitor_invalidate_channel(&s_snapshot.state);
        xQueueOverwrite(s_update_queue, &s_snapshot);
        xSemaphoreGive(s_state_lock);
    }
}

static void websocket_event(void *handler_args, esp_event_base_t event_base,
                            int32_t event_id, void *event_data)
{
    (void)event_base;
    fmo_socket_t *socket = (fmo_socket_t *)handler_args;
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    if (!socket) return;

    fmo_update_type_t link_type = socket->kind == FMO_SOCKET_EVENTS
                                      ? FMO_UPDATE_EVENTS_LINK
                                      : FMO_UPDATE_CONTROL_LINK;
    if (socket->diagnostics_ms == 0 || now_ms() - socket->diagnostics_ms >= 60000) {
        socket->diagnostics_ms = now_ms();
        unsigned remaining = uxTaskGetStackHighWaterMark(NULL);
        xSemaphoreTake(s_state_lock, portMAX_DELAY);
        unsigned *minimum = socket->kind == FMO_SOCKET_EVENTS ? &s_events_stack_min : &s_control_stack_min;
        if (remaining < *minimum) *minimum = remaining;
        xSemaphoreGive(s_state_lock);
    }
    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        post_link(link_type, true);
        fmo_network_request_refresh();
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
    case WEBSOCKET_EVENT_CLOSED:
        reset_rx(socket);
        post_link(link_type, false);
        break;
    case WEBSOCKET_EVENT_DATA:
        receive_fragment(socket, data);
        break;
    case WEBSOCKET_EVENT_ERROR:
        reset_rx(socket);
        post_link(link_type, false);
        break;
    default:
        break;
    }
}

static esp_err_t start_socket(fmo_socket_t *socket, const char *uri)
{
    esp_websocket_client_config_t config = {
        .uri = uri,
        .buffer_size = 1024,
        .task_stack = 4096,
        .enable_close_reconnect = true,
        .reconnect_timeout_ms = 2000,
        .network_timeout_ms = 3000,
        .ping_interval_sec = 5,
        .pingpong_timeout_sec = 10,
    };
    socket->client = esp_websocket_client_init(&config);
    if (!socket->client) return ESP_ERR_NO_MEM;

    esp_err_t err = esp_websocket_register_events(socket->client,
                                                   WEBSOCKET_EVENT_ANY,
                                                   websocket_event, socket);
    if (err == ESP_OK) err = esp_websocket_client_start(socket->client);
    if (err != ESP_OK) {
        esp_websocket_client_destroy(socket->client);
        socket->client = NULL;
    }
    return err;
}

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        if (atomic_load(&s_reconnect)) esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_wifi_bits, WIFI_READY_BIT);
        xEventGroupSetBits(s_wifi_bits, WIFI_READY_BIT << 1);
        post_link(FMO_UPDATE_WIFI, false);
        /* The network worker owns retries and profile switching. */
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_bits, WIFI_READY_BIT);
        post_link(FMO_UPDATE_WIFI, true);
    }
}

static void setup_display(const char *ssid, const char *password)
{
    xSemaphoreTake(s_state_lock, portMAX_DELAY);
    snprintf(s_snapshot.setup_ssid, sizeof(s_snapshot.setup_ssid), "%s", ssid);
    snprintf(s_snapshot.setup_password, sizeof(s_snapshot.setup_password), "%s", password);
    xQueueOverwrite(s_update_queue, &s_snapshot);
    xSemaphoreGive(s_state_lock);
}

static esp_err_t start_wifi(void)
{
    esp_err_t err = demo_radio_nvs_prepare();
    if (err != ESP_OK) return err;
    err = demo_radio_network_prepare();
    if (err != ESP_OK) return err;

    if (!esp_netif_create_default_wifi_sta()) return ESP_ERR_NO_MEM;

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&init_config);
    if (err != ESP_OK) return err;
    err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                     wifi_event, NULL);
    if (err != ESP_OK) return err;
    err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                     wifi_event, NULL);
    if (err != ESP_OK) return err;

    wifi_config_t wifi_config = { 0 };
    if (strlen(CONFIG_FMO_WIFI_SSID) > sizeof(wifi_config.sta.ssid) ||
        strlen(CONFIG_FMO_WIFI_PASSWORD) >= sizeof(wifi_config.sta.password))
        return ESP_ERR_INVALID_ARG;
    memcpy(wifi_config.sta.ssid, CONFIG_FMO_WIFI_SSID, strlen(CONFIG_FMO_WIFI_SSID));
    snprintf((char *)wifi_config.sta.password, sizeof(wifi_config.sta.password), "%s",
             CONFIG_FMO_WIFI_PASSWORD);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    bool force = false;
    bool saved = fmo_provision_load(&wifi_config, &force);
    wifi_config.sta.threshold.authmode = wifi_config.sta.password[0]
        ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    bool setup = force || (!saved && CONFIG_FMO_WIFI_SSID[0] == '\0');
    atomic_store(&s_reconnect, !setup);

    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err == ESP_OK) err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err == ESP_OK) err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err == ESP_OK) err = esp_wifi_start();
    if (err == ESP_OK && setup) {
        atomic_store(&s_setup_active, true);
        atomic_store(&s_setup_requested, false);
        err = fmo_provision_run(s_wifi_bits, WIFI_READY_BIT, setup_display);
        atomic_store(&s_setup_active, false);
        setup_display("", "");
        atomic_store(&s_reconnect, true);
        if (err == ESP_OK && !(xEventGroupGetBits(s_wifi_bits) & WIFI_READY_BIT)) esp_wifi_connect();
    }
    return err;
}

static void check_setup_request(void)
{
    if (atomic_exchange(&s_setup_requested, false)) {
        if (fmo_provision_force() == ESP_OK) esp_restart();
        else post_error("SETUP SAVE FAILED");
    }
}

static uint8_t s_tried_profiles;
static uint64_t s_next_wifi_attempt;
static bool s_was_online;

static void retry_wifi(void)
{
    if (xEventGroupGetBits(s_wifi_bits) & WIFI_READY_BIT) {
        if (!s_was_online) fmo_provision_remember_connected();
        s_was_online = true;
        s_tried_profiles = 0;
        s_next_wifi_attempt = 0;
        return;
    }
    s_was_online = false;
    if (now_ms() < s_next_wifi_attempt) return;
    esp_wifi_disconnect();
    wifi_config_t config;
    if (fmo_provision_next(&config, &s_tried_profiles)) {
        if (esp_wifi_set_config(WIFI_IF_STA, &config) == ESP_OK) esp_wifi_connect();
        memset(&config, 0, sizeof(config));
        s_next_wifi_attempt = now_ms() + 25000;
    } else {
        /* A complete failed round backs off. Build-time-only configuration
         * still reconnects without adding unverified credentials to NVS. */
        if (!fmo_provision_preferred_mask()) esp_wifi_connect();
        s_tried_profiles = 0;
        s_next_wifi_attempt = now_ms() + 30000;
    }
}

static void stop_socket(fmo_socket_t *socket)
{
    if (!socket->client) return;
    esp_websocket_client_stop(socket->client);
    esp_websocket_client_destroy(socket->client);
    socket->client = NULL;
    memset(&socket->rx, 0, sizeof(socket->rx));
}

static void clear_dns_cache(void *unused)
{
    (void)unused;
    dns_clear_cache();
}

static void network_task(void *argument)
{
    (void)argument;
    esp_err_t err = start_wifi();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi start failed: %s", esp_err_to_name(err));
        post_error("WIFI START FAILED");
        s_network_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    s_tried_profiles = fmo_provision_preferred_mask();
    s_next_wifi_attempt = now_ms() + 25000;
    while (!(xEventGroupWaitBits(s_wifi_bits, WIFI_READY_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(1000)) & WIFI_READY_BIT)) {
        check_setup_request();
        retry_wifi();
    }

    char events_uri[160];
    char control_uri[160];
    int events_length = snprintf(events_uri, sizeof(events_uri), "ws://%s:%d/events",
                                 CONFIG_FMO_HOST[0] ? CONFIG_FMO_HOST : "fmo.local", CONFIG_FMO_PORT);
    int control_length = snprintf(control_uri, sizeof(control_uri), "ws://%s:%d/ws",
                                  CONFIG_FMO_HOST[0] ? CONFIG_FMO_HOST : "fmo.local", CONFIG_FMO_PORT);
    if (events_length < 0 || events_length >= (int)sizeof(events_uri) ||
        control_length < 0 || control_length >= (int)sizeof(control_uri)) {
        post_error("FMO HOST TOO LONG");
        s_network_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    unsigned diagnostics_tick = 0;
    for (;;) {
        check_setup_request();
        EventBits_t old_bits = xEventGroupClearBits(s_wifi_bits, WIFI_READY_BIT << 1);
        if (!(old_bits & WIFI_READY_BIT) || (old_bits & (WIFI_READY_BIT << 1))) {
            stop_socket(&s_events_socket);
            stop_socket(&s_control_socket);
            if (old_bits & (WIFI_READY_BIT << 1))
                tcpip_callback_wait(clear_dns_cache, NULL);
        }
        retry_wifi();
        if (xEventGroupGetBits(s_wifi_bits) & WIFI_READY_BIT) {
            if (!s_events_socket.client) {
                err = start_socket(&s_events_socket, events_uri);
                if (err != ESP_OK) ESP_LOGW(TAG, "Events client creation failed; retrying");
            }
            if (!s_control_socket.client) {
                err = start_socket(&s_control_socket, control_uri);
                if (err != ESP_OK) ESP_LOGW(TAG, "Control client creation failed; retrying");
            }
            request_current_channel();
        }
        xSemaphoreTake(s_state_lock, portMAX_DELAY);
        if (s_snapshot.state.channel_valid &&
            now_ms() - s_snapshot.state.channel_confirmed_ms > FMO_CHANNEL_MAX_AGE_MS) {
            fmo_monitor_invalidate_channel(&s_snapshot.state);
            xQueueOverwrite(s_update_queue, &s_snapshot);
        }
        unsigned events_min = s_events_stack_min;
        unsigned control_min = s_control_stack_min;
        xSemaphoreGive(s_state_lock);
        if (++diagnostics_tick >= 60) {
            diagnostics_tick = 0;
            ESP_LOGI(TAG, "Internal heap: min=%u largest=%u; stack free: coordinator=%u events=%u control=%u",
                     (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                     (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                     (unsigned)uxTaskGetStackHighWaterMark(NULL), events_min, control_min);
        }
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(FMO_CHANNEL_REFRESH_MS));
    }
}

esp_err_t fmo_network_start(QueueHandle_t update_queue)
{
    if (!update_queue) return ESP_ERR_INVALID_ARG;
    if (s_network_task) return ESP_ERR_INVALID_STATE;

    s_state_lock = xSemaphoreCreateMutex();
    if (!s_state_lock) return ESP_ERR_NO_MEM;
    s_update_queue = update_queue;
    s_wifi_bits = xEventGroupCreate();
    if (!s_wifi_bits) {
        vSemaphoreDelete(s_state_lock);
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreate(network_task, "fmo_network", 6144, NULL, 5,
                    &s_network_task) != pdPASS) {
        vSemaphoreDelete(s_state_lock);
        vEventGroupDelete(s_wifi_bits);
        s_wifi_bits = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void fmo_network_request_refresh(void)
{
    if (s_network_task) xTaskNotifyGive(s_network_task);
}

void fmo_network_request_setup(void)
{
    if (atomic_load(&s_setup_active)) return;
    atomic_store(&s_setup_requested, true);
    if (s_network_task) xTaskNotifyGive(s_network_task);
}
