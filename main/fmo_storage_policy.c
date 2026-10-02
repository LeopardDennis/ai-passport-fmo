#include "fmo_storage_policy.h"
#include <string.h>

bool fmo_storage_reset_required(const fmo_install_record_t *record,
                                const uint8_t image_hash[FMO_STORAGE_HASH_SIZE])
{
    static const uint8_t magic[8] = FMO_INSTALL_MAGIC;
    return !record || !image_hash || memcmp(record->magic, magic, sizeof(magic)) ||
           memcmp(record->image_hash, image_hash, FMO_STORAGE_HASH_SIZE);
}

bool fmo_storage_erasure_allowed(uint32_t address, uint32_t size)
{
    if (!size || address % 4096 || size % 4096) return false;
    uint64_t end = (uint64_t)address + size;
    return (address >= 0x9000 && end <= 0x10000) ||
           (address >= 0x310000 && end <= 0x356000) ||
           (address >= 0x35a000 && end <= 0x700000);
}
