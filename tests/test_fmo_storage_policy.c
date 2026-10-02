#include "fmo_storage_policy.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    const uint8_t magic[8] = FMO_INSTALL_MAGIC;
    uint8_t hash[32]; memset(hash, 0x24, sizeof(hash));
    fmo_install_record_t record; memset(&record, 0xff, sizeof(record));
    assert(fmo_storage_reset_required(&record, hash));
    memcpy(record.magic, magic, sizeof(magic));
    memcpy(record.image_hash, hash, sizeof(hash));
    assert(!fmo_storage_reset_required(&record, hash));
    ++hash[31]; assert(fmo_storage_reset_required(&record, hash));
    assert(fmo_storage_reset_required(NULL, hash));
    assert(fmo_storage_reset_required(&record, NULL));
    assert(fmo_storage_erasure_allowed(0x9000, 0x6000));
    assert(fmo_storage_erasure_allowed(0xf000, 0x1000));
    assert(fmo_storage_erasure_allowed(0x310000, 0x10000));
    assert(fmo_storage_erasure_allowed(0x320000, 0x1000));
    assert(fmo_storage_erasure_allowed(0x35a000, 0x3a6000));
    assert(!fmo_storage_erasure_allowed(0, 0x9000));
    assert(!fmo_storage_erasure_allowed(0x10000, 0x300000));
    assert(!fmo_storage_erasure_allowed(0x356000, 0x4000));
    assert(!fmo_storage_erasure_allowed(0x700000, 0x100000));
    assert(!fmo_storage_erasure_allowed(0x355000, 0x2000));
    assert(!fmo_storage_erasure_allowed(0x359000, 0x2000));
    assert(!fmo_storage_erasure_allowed(0x6ff000, 0x2000));
    assert(!fmo_storage_erasure_allowed(0xfffff000, 0x2000));
    assert(!fmo_storage_erasure_allowed(0x9001, 4096));
    assert(!fmo_storage_erasure_allowed(0x9000, 0));
    return 0;
}
