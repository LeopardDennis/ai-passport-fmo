#pragma once
#include <stdbool.h>
#include <stdint.h>

#define FMO_STORAGE_HASH_SIZE 32
#define FMO_INSTALL_PARTITION "fmo_install"
#define FMO_INSTALL_MAGIC { 'F', 'M', 'O', 'D', 'O', 'N', 'E', 1 }

typedef struct {
    uint8_t magic[8];
    uint8_t image_hash[FMO_STORAGE_HASH_SIZE];
} fmo_install_record_t;

bool fmo_storage_reset_required(const fmo_install_record_t *record,
                                const uint8_t image_hash[FMO_STORAGE_HASH_SIZE]);
/* Sector-aligned application data only: exclude boot/app, identity and Recovery. */
bool fmo_storage_erasure_allowed(uint32_t address, uint32_t size);
