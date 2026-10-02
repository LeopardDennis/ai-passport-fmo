"""Compile actual installation cleanup against a simulated flash/partition API."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
HEADERS = {
    "esp_err.h": '''#pragma once
#include <stdint.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_NOT_FOUND 0x105
''',
    "esp_log.h": '''#pragma once
#define ESP_LOGI(tag, ...) ((void)(tag))
#define ESP_LOGE(tag, ...) ((void)(tag))
''',
    "esp_app_desc.h": '''#pragma once
#include <stdint.h>
typedef struct { uint8_t app_elf_sha256[32]; } esp_app_desc_t;
const esp_app_desc_t *esp_app_get_description(void);
''',
    "esp_partition.h": '''#pragma once
#include "esp_err.h"
#include <stddef.h>
typedef struct { uint8_t type, subtype; uint32_t address, size; char label[17]; } esp_partition_t;
typedef void *esp_partition_iterator_t;
#define ESP_PARTITION_TYPE_APP 0
#define ESP_PARTITION_TYPE_DATA 1
#define ESP_PARTITION_SUBTYPE_DATA_NVS 2
#define ESP_PARTITION_SUBTYPE_APP_FACTORY 0
#define ESP_PARTITION_SUBTYPE_APP_TEST 0x20
#define ESP_PARTITION_SUBTYPE_ANY 0xff
const esp_partition_t *esp_partition_find_first(int type, int subtype, const char *label);
esp_partition_iterator_t esp_partition_find(int type, int subtype, const char *label);
const esp_partition_t *esp_partition_get(esp_partition_iterator_t it);
esp_partition_iterator_t esp_partition_next(esp_partition_iterator_t it);
void esp_partition_iterator_release(esp_partition_iterator_t it);
esp_err_t esp_partition_read(const esp_partition_t *p, size_t offset, void *data, size_t length);
esp_err_t esp_partition_write(const esp_partition_t *p, size_t offset, const void *data, size_t length);
esp_err_t esp_partition_erase_range(const esp_partition_t *p, size_t offset, size_t length);
''',
    "nvs_flash.h": '''#pragma once
#include "esp_err.h"
esp_err_t nvs_flash_erase_partition(const char *label);
'''
}
TEST = r'''
#include "fmo_storage.h"
#include "fmo_storage_policy.h"
#include "esp_partition.h"
#include "esp_app_desc.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char flash[0x800000];
static esp_partition_t parts[] = {
    {1,2,0x9000,0x6000,"nvs"}, {1,1,0xf000,0x1000,"phy_init"},
    {0,0,0x10000,0x300000,"factory"}, {1,2,0x310000,0x10000,"pdk_cache"},
    {1,6,0x320000,0x1000,"fmo_install"}, {1,2,0x356000,0x4000,"cardid"},
    {0,0x20,0x700000,0x100000,"recovery"}, {1,6,0x35a000,0x1000,"other_data"},
};
static esp_app_desc_t app;
static int erased, written, active_iterator, fail_erase, fail_write, fail_read, torn_write;
static const char seed[] = "FMO_RESET_ON_INSTALL_V1\n";
const esp_app_desc_t *esp_app_get_description(void) { return &app; }
const esp_partition_t *esp_partition_find_first(int type, int subtype, const char *label) {
    for(unsigned i=0; i<sizeof(parts)/sizeof(parts[0]); ++i)
        if (parts[i].type==type && (subtype==255 || subtype==parts[i].subtype) &&
            (!label || !strcmp(parts[i].label,label))) return parts+i;
    return NULL;
}
esp_partition_iterator_t esp_partition_find(int type, int subtype, const char *label) {
    assert(type==1 && subtype==255 && !label && !active_iterator); active_iterator=1;
    return (void *)(uintptr_t)1;
}
const esp_partition_t *esp_partition_get(esp_partition_iterator_t it) {
    assert(active_iterator); return &parts[(uintptr_t)it-1];
}
esp_partition_iterator_t esp_partition_next(esp_partition_iterator_t it) {
    assert(active_iterator);
    for (unsigned i=(uintptr_t)it; i<sizeof(parts)/sizeof(parts[0]); ++i)
        if (parts[i].type==1) return (void *)(uintptr_t)(i+1);
    active_iterator=0; return NULL;
}
void esp_partition_iterator_release(esp_partition_iterator_t it) { assert(it && active_iterator); active_iterator=0; }
esp_err_t esp_partition_read(const esp_partition_t *p, size_t o, void *v, size_t n) {
    assert(o+n<=p->size); if (fail_read) return -99;
    memcpy(v, flash+p->address+o, n); return ESP_OK;
}
esp_err_t esp_partition_erase_range(const esp_partition_t *p, size_t o, size_t n) {
    assert(p->type==1 && strcmp(p->label,"cardid") && strcmp(p->label,"recovery"));
    assert(!o && n==p->size && fmo_storage_erasure_allowed(p->address,n));
    if (++erased==fail_erase) return -99;
    memset(flash+p->address,0xff,n); return ESP_OK;
}
esp_err_t nvs_flash_erase_partition(const char *label) {
    const esp_partition_t *p=esp_partition_find_first(1,2,label); assert(p);
    return esp_partition_erase_range(p,0,p->size);
}
esp_err_t esp_partition_write(const esp_partition_t *p, size_t o, const void *v, size_t n) {
    assert(!strcmp(p->label,"fmo_install") && !o && o+n<=p->size); ++written;
    if (fail_write) return -99;
    if (torn_write) { memcpy(flash+p->address,v,9); return -99; }
    memcpy(flash+p->address,v,n); return ESP_OK;
}
static void fresh_install(void) {
    memset(flash+0x320000,0xff,4096); memcpy(flash+0x320000,seed,sizeof(seed)-1);
}
static void dirty_data(void) {
    memset(flash+0x9000,0xa1,0x6000); memset(flash+0xf000,0xa2,4096);
    memset(flash+0x310000,0xa3,0x10000); memset(flash+0x35a000,0xa4,4096);
}
static void check_protected(void) {
    for (unsigned i=0; i<sizeof(flash); ++i)
        if (i<0x9000 || (i>=0x10000 && i<0x310000) ||
            (i>=0x356000 && i<0x35a000) || i>=0x700000) assert(flash[i]==0x42);
    assert(!active_iterator);
}
static void check_clean(void) {
    for (unsigned i=0x9000;i<0x10000;++i) assert(flash[i]==0xff);
    for (unsigned i=0x310000;i<0x320000;++i) assert(flash[i]==0xff);
    for (unsigned i=0x35a000;i<0x35b000;++i) assert(flash[i]==0xff);
    check_protected();
}
int main(void) {
    memset(flash,0x42,sizeof(flash)); memset(app.app_elf_sha256,0x24,32);
    dirty_data(); fresh_install();
    assert(fmo_storage_prepare()==ESP_OK && written==1); check_clean();
    // Power cycling the same image must preserve FMO Wi-Fi settings.
    dirty_data(); int previous=erased;
    assert(fmo_storage_prepare()==ESP_OK && erased==previous && written==1);
    assert(flash[0x9000]==0xa1);
    // Full-package reinstall of the same binary must reset again.
    fresh_install(); assert(fmo_storage_prepare()==ESP_OK && written==2); check_clean();
    // A changed application hash also requests a reset.
    dirty_data(); app.app_elf_sha256[31]++; assert(fmo_storage_prepare()==ESP_OK); check_clean();
    // Failure/power loss before commit cannot authorize old/partially erased data.
    dirty_data(); fresh_install(); previous=written; fail_erase=erased+2;
    assert(fmo_storage_prepare()==-99 && written==previous); check_protected();
    fail_erase=0; assert(fmo_storage_prepare()==ESP_OK); check_clean();
    dirty_data(); fresh_install(); torn_write=1;
    assert(fmo_storage_prepare()==-99); check_clean();
    dirty_data(); torn_write=0; assert(fmo_storage_prepare()==ESP_OK); check_clean();
    // Read failure or invalid layout must erase nothing.
    previous=erased; fail_read=1; assert(fmo_storage_prepare()==-99 && erased==previous); fail_read=0;
    parts[5].address=0x355000; assert(fmo_storage_prepare()==ESP_ERR_INVALID_STATE && erased==previous);
    parts[5].address=0x356000;
    parts[7].address=0x10000; assert(fmo_storage_prepare()==ESP_ERR_INVALID_STATE && erased==previous);
    parts[7].address=0x35a000;
    parts[4].label[0]='x'; assert(fmo_storage_prepare()==ESP_ERR_NOT_FOUND && erased==previous);
    parts[4].label[0]='f';
    memset(app.app_elf_sha256,0,32); assert(fmo_storage_prepare()==ESP_ERR_INVALID_STATE && erased==previous);
    check_protected();
    puts("FMO install reset: PASS (fresh/reinstall, reboot, failures, protected bytes)");
}
'''
with tempfile.TemporaryDirectory(prefix="fmo-storage-test-") as directory:
    temp = Path(directory)
    for name, content in HEADERS.items():
        (temp/name).write_text(content)
    (temp/"test.c").write_text(TEST)
    binary = temp/"test"
    subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-I"+str(temp), "-I"+str(ROOT/"main"), str(temp/"test.c"),
                    str(ROOT/"main/fmo_storage.c"), str(ROOT/"main/fmo_storage_policy.c"),
                    "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
# Cleanup must run in the worker before Wi-Fi can read any saved settings.
network = (ROOT/"main/fmo_network.c").read_text().split("static void prepare_network(",1)[1]
assert network.index("fmo_storage_prepare()") < network.index("start_wifi()")
