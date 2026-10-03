#include "fmo_network.h"
#include "fmo_provision.h"
#include "esp_system.h"
#include <stdatomic.h>
#include "lwip/dns.h"
#include "lwip/tcpip.h"

#include "demo_radio.h"
#include "fmo_ws_rx.h"
#include "fmo_link_policy.h"
#include "fmo_text.h"
#include "fmo_storage.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/semphr.h"

#include "cJSON.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
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
static atomic_bool s_retry_requested;

#define FMO_CHANNEL_REFRESH_MS 1000

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
static bool s_clock_initialized, s_clock_online;
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
static esp_netif_t *s_station;
static bool s_wifi_initialized;
static esp_event_handler_instance_t s_wifi_handler;
static esp_event_handler_instance_t s_ip_handler;
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
        if (!update->connected) {
            s_query_pending = false;
        }
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
            if (s_query_revision == s_speaker_revision) {
                fmo_monitor_set_channel(state, update->uid, update->text);
                state->channel_confirmed_ms = now_ms();
            } else {
                // Discard replies from before the latest talker. Keep an existing
                // confirmed channel visible while a fresh query runs; its age
                // still expires normally, and an unknown channel stays unknown.
                refresh = true;
            }
        }
        s_query_pending = false;
        break;
    case FMO_UPDATE_SPEAKER:
        if (state->wifi_connected && state->events_connected) {
            if (update->speaking) ++s_snapshot.speech_activity;
            if (update->speaking &&
                (!state->speaking || strcmp(state->speaker, update->callsign) != 0)) {
                ++s_speaker_revision;
                // Starting speech requests a background refresh, not a loss of
                // the channel that was already confirmed on this connection.
                refresh = true;
            }
            fmo_monitor_apply_speaker(state, update->callsign, update->grid,
                                      update->speaking, update->is_host, update->cross_server, now_ms());
        }
        break;
    case FMO_UPDATE_HISTORY:
        if (state->wifi_connected && state->events_connected)
            state->history = update->history;
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

static bool copy_ascii(char *destination, size_t capacity, const char *source)
{
    if (!destination || !capacity) return false;
    destination[0] = '\0';
    if (!source || strlen(source) >= capacity) return false;
    for (const unsigned char *p = (const unsigned char *)source; *p; ++p)
        if (*p < 0x20 || *p > 0x7e) return false;
    memcpy(destination, source, strlen(source) + 1);
    return true;
}

/* A lost/invalid PTT release must not leave a permanently live callsign. */
static void invalidate_live_message(void)
{
    xSemaphoreTake(s_state_lock, portMAX_DELAY);
    s_snapshot.state.speaking = false;
    s_snapshot.state.speaker[0] = '\0';
    s_snapshot.state.speaker_is_host = false;
    s_snapshot.state.speaker_cross_server = false;
    ++s_speaker_revision;
    // Keep an outstanding query owned by this connection. Its old speaker
    // revision will reject the reply; timeout will drain the client if needed.
    fmo_monitor_invalidate_channel(&s_snapshot.state);
    xQueueOverwrite(s_update_queue, &s_snapshot);
    xSemaphoreGive(s_state_lock);
    fmo_network_request_refresh();
}

/* History is informational: invalid entries never clear the live talker/channel.
 * Select the newest three by timestamp even if a future server changes ordering. */
static void parse_fmo_history(const cJSON *data)
{
    if (!cJSON_IsArray(data)) return;
    fmo_update_t update = {.type = FMO_UPDATE_HISTORY};
    const cJSON *item;
    cJSON_ArrayForEach(item, data) {
        const cJSON *call = cJSON_GetObjectItemCaseSensitive(item, "callsign");
        const cJSON *time = cJSON_GetObjectItemCaseSensitive(item, "utcTime");
        fmo_history_entry_t entry = {0};
        if (!cJSON_IsString(call) ||
            !copy_ascii(entry.callsign, sizeof(entry.callsign), call->valuestring) ||
            !entry.callsign[0]) continue;
        if (cJSON_IsNumber(time) && time->valuedouble >= 946684800 &&
            time->valuedouble < 4102444800 &&
            time->valuedouble == (int64_t)time->valuedouble)
            entry.timestamp = (int64_t)time->valuedouble;
        unsigned pos = 0;
        while (pos < update.history.count &&
               update.history.entries[pos].timestamp >= entry.timestamp) ++pos;
        if (pos >= FMO_HISTORY_COUNT) continue;
        if (update.history.count < FMO_HISTORY_COUNT) ++update.history.count;
        for (unsigned i = update.history.count - 1; i > pos; --i)
            update.history.entries[i] = update.history.entries[i - 1];
        update.history.entries[pos] = entry;
    }
    if (update.history.count || !cJSON_GetArraySize(data)) post_update(&update);
}

static void parse_fmo_message(fmo_socket_kind_t kind, const char *payload)
{
    cJSON *root = cJSON_ParseWithOpts(payload, NULL, true);
    if (!root) {
        ESP_LOGW(TAG, "Ignored invalid JSON from %s socket",
                 kind == FMO_SOCKET_EVENTS ? "events" : "control");
        invalidate_live_message();
        return;
    }

    const cJSON *type = cJSON_GetObjectItemCaseSensitive(root, "type");
    const cJSON *sub_type = cJSON_GetObjectItemCaseSensitive(root, "subType");
    const cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!cJSON_IsString(type) || !cJSON_IsString(sub_type)) {
        cJSON_Delete(root);
        return;
    }
    if (kind == FMO_SOCKET_EVENTS && !strcmp(type->valuestring, "qso") &&
        !strcmp(sub_type->valuestring, "history")) {
        if (!fmo_text_json_has_nul(payload)) parse_fmo_history(data);
        cJSON_Delete(root);
        return;
    }
    if (fmo_text_json_has_nul(payload)) {
        cJSON_Delete(root);
        invalidate_live_message();
        return;
    }
    if (!cJSON_IsObject(data)) {
        if ((kind == FMO_SOCKET_EVENTS && strcmp(type->valuestring, "qso") == 0 &&
             strcmp(sub_type->valuestring, "callsign") == 0) ||
            (kind == FMO_SOCKET_CONTROL && strcmp(type->valuestring, "station") == 0 &&
             strcmp(sub_type->valuestring, "getCurrentResponse") == 0))
            invalidate_live_message();
        cJSON_Delete(root);
        return;
    }

    if (kind == FMO_SOCKET_EVENTS && strcmp(type->valuestring, "qso") == 0 &&
        strcmp(sub_type->valuestring, "callsign") == 0) {
        const cJSON *callsign = cJSON_GetObjectItemCaseSensitive(data, "callsign");
        const cJSON *grid = cJSON_GetObjectItemCaseSensitive(data, "grid");
        const cJSON *speaking = cJSON_GetObjectItemCaseSensitive(data, "isSpeaking");
        const cJSON *is_host = cJSON_GetObjectItemCaseSensitive(data, "isHost");
        const cJSON *cross_server = cJSON_GetObjectItemCaseSensitive(data, "crossServer");
        if (cJSON_IsBool(speaking) || (cJSON_IsNumber(speaking) &&
            (speaking->valuedouble == 0 || speaking->valuedouble == 1))) {
            fmo_update_t update = {
                .type = FMO_UPDATE_SPEAKER,
                .speaking = json_bool(speaking),
            };
            /* An explicit idle event may omit the callsign or send null/"".
             * Release metadata is not used; only starts need grid/host fields.
             * Keep named releases matched so an old talker cannot stop a new one. */
            bool valid = cJSON_IsString(callsign) ?
                copy_ascii(update.callsign, sizeof(update.callsign), callsign->valuestring) :
                !update.speaking && (!callsign || cJSON_IsNull(callsign));
            if (update.speaking) {
                valid = valid && update.callsign[0];
                if (grid && !cJSON_IsNull(grid)) valid = valid && cJSON_IsString(grid) &&
                    copy_ascii(update.grid, sizeof(update.grid), grid->valuestring);
                if (is_host) valid = valid && (cJSON_IsBool(is_host) || (cJSON_IsNumber(is_host) &&
                    (is_host->valuedouble == 0 || is_host->valuedouble == 1)));
                update.is_host = json_bool(is_host);
                /* Decorative metadata must never blank a valid PTT/channel.
                 * Missing/unknown flag values use the ordinary speaker color. */
                update.cross_server = cJSON_IsTrue(cross_server) ||
                    (cJSON_IsNumber(cross_server) && cross_server->valuedouble == 1);
            }
            if (valid) post_update(&update);
            else invalidate_live_message();
        } else invalidate_live_message();
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
        if (cJSON_IsString(name)) fmo_text_copy_utf8(update.text, sizeof(update.text), name->valuestring);
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
        invalidate_live_message();
        ESP_LOGW(TAG, "Dropped invalid or oversized %s WebSocket message "
                 "(frame=%d offset=%d chunk=%d opcode=%u capacity=%u)",
                 socket->kind == FMO_SOCKET_EVENTS ? "events" : "control",
                 data->payload_len, data->payload_offset, data->data_len,
                 (unsigned)data->op_code, (unsigned)FMO_WS_MESSAGE_CAPACITY);
    }
}

/* Only the coordinator sends queries. Socket callbacks merely notify it. */
static bool request_current_channel(void)
{
    if (!s_control_socket.client ||
        !esp_websocket_client_is_connected(s_control_socket.client)) return false;
    static const char request[] =
        "{\"type\":\"station\",\"subType\":\"getCurrent\",\"data\":{}}";
    xSemaphoreTake(s_state_lock, portMAX_DELAY);
    if (s_query_pending) {
        bool expired = now_ms() - s_query_ms >= FMO_CHANNEL_QUERY_TIMEOUT_MS;
        if (expired) {
            // The protocol has no request ID. Drain this client before sending
            // another query so a delayed reply cannot inherit new metadata.
            ESP_LOGW(TAG, "Control reconnect: channel query exceeded %u ms",
                     (unsigned)FMO_CHANNEL_QUERY_TIMEOUT_MS);
            s_query_pending = false;
            fmo_monitor_set_control(&s_snapshot.state, false);
            xQueueOverwrite(s_update_queue, &s_snapshot);
        }
        xSemaphoreGive(s_state_lock);
        return expired;
    }
    s_query_pending = true;
    s_query_ms = now_ms();
    s_query_revision = s_speaker_revision;
    xSemaphoreGive(s_state_lock);
    int sent = esp_websocket_client_send_text(s_control_socket.client, request,
                                               sizeof(request) - 1, pdMS_TO_TICKS(FMO_LINK_IO_TIMEOUT_MS));
    if (sent != sizeof(request) - 1) {
        ESP_LOGW(TAG, "Control reconnect: channel query send failed (%d)", sent);
        xSemaphoreTake(s_state_lock, portMAX_DELAY);
        s_query_pending = false;
        fmo_monitor_invalidate_channel(&s_snapshot.state);
        xQueueOverwrite(s_update_queue, &s_snapshot);
        xSemaphoreGive(s_state_lock);
        return true; // A partially sent query also has ambiguous reply ownership.
    }
    return false;
}

/* The coordinator owns one lifetime SNTP service. No wait for synchronization:
 * the display stays usable with --:-- until the system clock becomes valid. */
static void update_clock_service(bool online)
{
    if (!online) { s_clock_online = false; return; }
    if (s_clock_online) return;
    esp_err_t err;
    if (!s_clock_initialized) {
        esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
        config.wait_for_sync = false;
        err = esp_netif_sntp_init(&config);
        if (err == ESP_OK) s_clock_initialized = true;
    } else err = esp_netif_sntp_start();
    s_clock_online = err == ESP_OK;
    if (err != ESP_OK) ESP_LOGW(TAG, "Clock service unavailable; retrying");
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
    const char *kind = socket->kind == FMO_SOCKET_EVENTS ? "Events" : "Control";
    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "%s socket connected", kind);
        post_link(link_type, true);
        fmo_network_request_refresh();
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
    case WEBSOCKET_EVENT_CLOSED:
        ESP_LOGW(TAG, "%s socket disconnected (event=%ld)", kind, (long)event_id);
        reset_rx(socket);
        post_link(link_type, false);
        break;
    case WEBSOCKET_EVENT_DATA:
        receive_fragment(socket, data);
        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGW(TAG, "%s socket error (type=%d)", kind,
                 data ? (int)data->error_handle.error_type : -1);
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
        .network_timeout_ms = FMO_LINK_IO_TIMEOUT_MS,
        .ping_interval_sec = FMO_LINK_PING_INTERVAL_SEC,
        .pingpong_timeout_sec = FMO_LINK_PONG_TIMEOUT_SEC,
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
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        if (atomic_load(&s_reconnect)) esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *disconnected = data;
        if (disconnected) {
            ESP_LOGW(TAG, "Station disconnected: reason=%u", disconnected->reason);
            if (atomic_load(&s_setup_active))
                fmo_provision_note_disconnect_reason(disconnected->reason);
        }
        xEventGroupClearBits(s_wifi_bits, FMO_WIFI_READY_BIT);
        xEventGroupSetBits(s_wifi_bits, FMO_WIFI_DISCONNECTED_BIT);
        post_link(FMO_UPDATE_WIFI, false);
        /* The network worker owns retries and profile switching. */
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_STOP) {
        xEventGroupSetBits(s_wifi_bits, FMO_WIFI_STOPPED_BIT);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_bits, FMO_WIFI_READY_BIT);
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

    s_station = esp_netif_create_default_wifi_sta();
    if (!s_station) return ESP_ERR_NO_MEM;

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&init_config);
    if (err != ESP_OK) return err;
    s_wifi_initialized = true;
    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               wifi_event, NULL, &s_wifi_handler);
    if (err != ESP_OK) return err;
    err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               wifi_event, NULL, &s_ip_handler);
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
        err = fmo_provision_run(s_wifi_bits, FMO_WIFI_READY_BIT, setup_display);
        atomic_store(&s_setup_active, false);
        setup_display("", "");
        atomic_store(&s_reconnect, true);
        if (err == ESP_OK && !(xEventGroupGetBits(s_wifi_bits) & FMO_WIFI_READY_BIT)) esp_wifi_connect();
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
    if (atomic_exchange(&s_retry_requested, false)) {
        s_tried_profiles = 0;
        s_next_wifi_attempt = 0;
    }
    if (xEventGroupGetBits(s_wifi_bits) & FMO_WIFI_READY_BIT) {
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

static void cleanup_wifi(void)
{
    atomic_store(&s_reconnect, false);
    atomic_store(&s_setup_active, false);
    if (s_wifi_handler) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, s_wifi_handler);
        s_wifi_handler = NULL;
    }
    if (s_ip_handler) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, s_ip_handler);
        s_ip_handler = NULL;
    }
    if (s_wifi_initialized) {
        esp_wifi_stop();
        esp_wifi_deinit();
        s_wifi_initialized = false;
    }
    if (s_station) {
        esp_netif_destroy_default_wifi(s_station);
        s_station = NULL;
    }
    xEventGroupClearBits(s_wifi_bits, FMO_WIFI_READY_BIT | FMO_WIFI_DISCONNECTED_BIT |
                         FMO_WIFI_STOPPED_BIT | FMO_WIFI_CANCEL_BIT);
    post_link(FMO_UPDATE_WIFI, false);
}

/* Keep the worker and menu requests alive through partial startup failures.
 * Storage remains a fail-closed prerequisite for every network attempt. */
static void prepare_network(void)
{
    for (;;) {
        esp_err_t err = fmo_storage_prepare();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Installation cleanup failed: %s", esp_err_to_name(err));
            post_error("DATA RESET FAILED");
        } else {
            check_setup_request();
            err = start_wifi();
            if (err == ESP_OK) {
                post_error("");
                atomic_store(&s_retry_requested, false);
                return;
            }
            ESP_LOGE(TAG, "Wi-Fi/setup start failed: %s", esp_err_to_name(err));
            cleanup_wifi();
            post_error("WIFI START FAILED");
        }
        // A retry/setup notification interrupts the backoff immediately.
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5000));
    }
}

static void network_task(void *argument)
{
    (void)argument;
    prepare_network();
    esp_err_t err;

    s_tried_profiles = fmo_provision_preferred_mask();
    s_next_wifi_attempt = now_ms() + 25000;
    while (!(xEventGroupWaitBits(s_wifi_bits, FMO_WIFI_READY_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(1000)) & FMO_WIFI_READY_BIT)) {
        check_setup_request();
        retry_wifi();
    }

    fmo_endpoint_t endpoint;
    fmo_provision_get_endpoint(&endpoint);
    char events_uri[160];
    char control_uri[160];
    int events_length = snprintf(events_uri, sizeof(events_uri), "ws://%s:%d/events",
                                 endpoint.host, endpoint.port);
    int control_length = snprintf(control_uri, sizeof(control_uri), "ws://%s:%d/ws",
                                  endpoint.host, endpoint.port);
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
        EventBits_t old_bits = xEventGroupClearBits(s_wifi_bits, FMO_WIFI_DISCONNECTED_BIT);
        if (!(old_bits & FMO_WIFI_READY_BIT) || (old_bits & FMO_WIFI_DISCONNECTED_BIT)) {
            update_clock_service(false);
            stop_socket(&s_events_socket);
            stop_socket(&s_control_socket);
            if (old_bits & FMO_WIFI_DISCONNECTED_BIT)
                tcpip_callback_wait(clear_dns_cache, NULL);
        }
        retry_wifi();
        if (xEventGroupGetBits(s_wifi_bits) & FMO_WIFI_READY_BIT) {
            update_clock_service(true);
            if (!s_events_socket.client) {
                err = start_socket(&s_events_socket, events_uri);
                if (err != ESP_OK) ESP_LOGW(TAG, "Events client creation failed; retrying");
            }
            if (!s_control_socket.client) {
                err = start_socket(&s_control_socket, control_uri);
                if (err != ESP_OK) ESP_LOGW(TAG, "Control client creation failed; retrying");
            }
            if (request_current_channel())
                stop_socket(&s_control_socket);
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

void fmo_network_request_retry(void)
{
    atomic_store(&s_retry_requested, true);
    if (s_network_task) xTaskNotifyGive(s_network_task);
}

void fmo_network_cancel_setup(void)
{
    if (s_wifi_bits && atomic_load(&s_setup_active))
        xEventGroupSetBits(s_wifi_bits, FMO_WIFI_CANCEL_BIT);
}
