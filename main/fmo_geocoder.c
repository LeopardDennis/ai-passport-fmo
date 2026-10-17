#include "fmo_geocoder.h"
#include "fmo_location.h"
#include "fmo_network.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "nvs.h"
#include "sdkconfig.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define RESPONSE_LIMIT 4096
static const char *TAG = "fmo_location";
static fmo_location_cache_t cache;
static TaskHandle_t worker;

static uint64_t milliseconds(void) { return esp_timer_get_time() / 1000; }

static void cache_load(void)
{
    nvs_handle_t handle;
    if (nvs_open("fmo_places_v1", NVS_READONLY, &handle) != ESP_OK) return;
    for (unsigned i = 0; i < FMO_LOCATION_CACHE_SIZE; ++i) {
        char key[8];
        snprintf(key, sizeof(key), "p%02u", i);
        size_t size = sizeof(cache.entries[i]);
        if (nvs_get_blob(handle, key, &cache.entries[i], &size) == ESP_OK &&
            size == sizeof(cache.entries[i]) && fmo_location_entry_valid(&cache.entries[i]))
            cache.occupied[i] = true;
        else memset(&cache.entries[i], 0, sizeof(cache.entries[i]));
    }
    nvs_close(handle);
}

static void cache_save(unsigned slot)
{
    nvs_handle_t handle;
    if (nvs_open("fmo_places_v1", NVS_READWRITE, &handle) != ESP_OK) return;
    char key[8];
    snprintf(key, sizeof(key), "p%02u", slot);
    esp_err_t err = nvs_set_blob(handle, key, &cache.entries[slot], sizeof(cache.entries[slot]));
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    if (err != ESP_OK) ESP_LOGW(TAG, "Cache write failed; retaining RAM cache");
}

static bool lookup(fmo_position_t position, char *name, size_t capacity)
{
    char url[256];
    int n = snprintf(url, sizeof(url), "%s?lat=%.6f&lon=%.6f&limit=1",
                     CONFIG_FMO_GEOCODER_URL, position.lat_e6 / 1000000.0, position.lon_e6 / 1000000.0);
    if (n < 0 || (size_t)n >= sizeof(url)) return false;
    char *body = malloc(RESPONSE_LIMIT + 1);
    if (!body) return false;
    esp_http_client_config_t config = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 3000,
        .buffer_size = 512,
        .buffer_size_tx = 512,
        .user_agent = "AI-Passport-FMO/1.0 (+https://github.com/LeopardDennis/ai-passport-fmo)",
        .disable_auto_redirect = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    bool ok = false;
    uint64_t deadline = milliseconds() + 8000;
    if (!client) goto done;
    if (esp_http_client_open(client, 0) != ESP_OK) goto done;
    int64_t length = esp_http_client_fetch_headers(client);
    if (length < 0 || length > RESPONSE_LIMIT || esp_http_client_get_status_code(client) != 200) goto done;
    size_t used = 0;
    /* fetch_headers may already have buffered the complete body. Always drain
     * read(), even when is_complete_data_received() is already true. */
    while (used < RESPONSE_LIMIT && milliseconds() < deadline) {
        size_t remaining = RESPONSE_LIMIT - used;
        int got = esp_http_client_read(client, body + used, remaining < 512 ? remaining : 512);
        if (got <= 0) break;
        used += got;
    }
    body[used] = 0;
    ok = used && strlen(body) == used && esp_http_client_is_complete_data_received(client) && milliseconds() < deadline;
done:
    /* Release TLS buffers before allocating the JSON tree. */
    if (client) esp_http_client_cleanup(client);
    if (ok) ok = fmo_location_parse_response(body, name, capacity);
    free(body);
    return ok;
}

static void geocoder_task(void *argument)
{
    (void)argument;
    cache_load();
    uint64_t next_request = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(200));
        fmo_monitor_state_t request;
        if (!fmo_network_location_snapshot(&request)) continue;
        fmo_position_t position = request.position;
        if (!request.position_valid && !fmo_location_grid(request.grid, &position)) continue;
        uint64_t now = milliseconds();
        const fmo_location_entry_t *hit = fmo_location_find(&cache, position, now);
        if (hit) {
            if (hit->name[0]) fmo_network_location_result(&request, hit->name);
            continue;
        }
        /* Wait for a valid TLS clock and protect streaming audio's heap budget.
         * Cache hits above remain available without Internet or a synced clock. */
        if (now < next_request || time(NULL) < 1704067200 ||
            heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) < 65536 ||
            heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) < 32768) continue;
        char name[FMO_LOCATION_MAX + 1] = "";
        bool ok = lookup(position, name, sizeof(name));
        now = milliseconds();
        next_request = now + (ok ? 15000 : 60000);
        unsigned slot = fmo_location_store(&cache, position, ok ? name : "", now);
        if (ok) {
            /* Result is rechecked against current identity under the state lock.
             * A stale result can be cached, but cannot change the new speaker. */
            fmo_network_location_result(&request, name);
            cache_save(slot);
        }
    }
}

void fmo_geocoder_start(void)
{
    if (worker || !CONFIG_FMO_GEOCODER_URL[0]) return;
    if (strncmp(CONFIG_FMO_GEOCODER_URL, "https://", 8)) {
        ESP_LOGW(TAG, "Location lookup requires an HTTPS endpoint");
        return;
    }
    if (xTaskCreate(geocoder_task, "fmo_location", 6144, NULL, 2, &worker) != pdPASS)
        ESP_LOGW(TAG, "Location worker unavailable; retaining grids");
}
