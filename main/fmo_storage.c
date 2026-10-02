#include "fmo_storage.h"
#include "fmo_storage_policy.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "fmo_storage";

static bool is_reset_target(const esp_partition_t *partition)
{
    return strcmp(partition->label, "cardid") != 0 &&
           strcmp(partition->label, FMO_INSTALL_PARTITION) != 0;
}

/* Validate the entire layout before erasing anything, including on app-only
 * development updates. Reject shifted identity or app partitions. */
static esp_err_t validate_reset_layout(const esp_partition_t *marker)
{
    const esp_partition_t *identity = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, "cardid");
    const esp_partition_t *app = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, "factory");
    const esp_partition_t *recovery = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_TEST, "recovery");
    const esp_partition_t *legacy = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, "pdk_cache");
    const esp_partition_t *nvs = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, "nvs");
    if (!identity || identity->address != 0x356000 || identity->size != 0x4000 ||
        !app || app->address != 0x10000 || app->size != 0x300000 ||
        !recovery || recovery->address != 0x700000 || recovery->size != 0x100000 ||
        !legacy || legacy->address != 0x310000 || legacy->size != 0x10000 ||
        !nvs || nvs->address != 0x9000 || nvs->size != 0x6000 ||
        marker->address != 0x320000 || marker->size != 0x1000)
        return ESP_ERR_INVALID_STATE;

    esp_partition_iterator_t it = esp_partition_find(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, NULL);
    while (it) {
        const esp_partition_t *part = esp_partition_get(it);
        if (is_reset_target(part) && !fmo_storage_erasure_allowed(part->address, part->size)) {
            esp_partition_iterator_release(it);
            return ESP_ERR_INVALID_STATE;
        }
        it = esp_partition_next(it);
    }
    return ESP_OK;
}

static esp_err_t reset_application_data(void)
{
    esp_partition_iterator_t it = esp_partition_find(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, NULL);
    while (it) {
        const esp_partition_t *part = esp_partition_get(it);
        if (is_reset_target(part)) {
            esp_err_t err = part->subtype == ESP_PARTITION_SUBTYPE_DATA_NVS
                ? nvs_flash_erase_partition(part->label)
                : esp_partition_erase_range(part, 0, part->size);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Application data reset failed (%s): %s", part->label, esp_err_to_name(err));
                esp_partition_iterator_release(it);
                return err;
            }
        }
        it = esp_partition_next(it);
    }
    return ESP_OK;
}

esp_err_t fmo_storage_prepare(void)
{
    const esp_partition_t *marker = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, FMO_INSTALL_PARTITION);
    if (!marker) return ESP_ERR_NOT_FOUND;
    esp_err_t err = validate_reset_layout(marker);
    if (err != ESP_OK) return err;
    const esp_app_desc_t *app = esp_app_get_description();
    if (!app) return ESP_ERR_INVALID_STATE;
    uint8_t nonzero = 0;
    for (unsigned i = 0; i < FMO_STORAGE_HASH_SIZE; ++i) nonzero |= app->app_elf_sha256[i];
    if (!nonzero) return ESP_ERR_INVALID_STATE;

    fmo_install_record_t record;
    err = esp_partition_read(marker, 0, &record, sizeof(record));
    if (err != ESP_OK) return err;
    if (!fmo_storage_reset_required(&record, app->app_elf_sha256)) return ESP_OK;

    ESP_LOGI(TAG, "Fresh firmware install: resetting all application data");
    err = reset_application_data();
    if (err != ESP_OK) return err;
    // Commit last. Interrupted erases/writes leave a missing or incomplete
    // marker, so boot retries cleanup instead of using partially reset data.
    static const uint8_t magic[8] = FMO_INSTALL_MAGIC;
    memcpy(record.magic, magic, sizeof(magic));
    memcpy(record.image_hash, app->app_elf_sha256, sizeof(record.image_hash));
    err = esp_partition_erase_range(marker, 0, marker->size);
    if (err == ESP_OK) err = esp_partition_write(marker, 0, &record, sizeof(record));
    if (err == ESP_OK) ESP_LOGI(TAG, "Application reset complete; identity and Recovery preserved");
    return err;
}
