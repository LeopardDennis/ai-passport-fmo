"""Exercise production parser/state transitions against ESP-IDF's real cJSON."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def extract_function(source, name):
    match = re.search(r"^static [^\n]+\b" + name + r"\([^;]*?\)\n\{", source, re.M)
    if not match:
        raise AssertionError(f"Missing production function: {name}")
    start = match.start()
    end = match.end()
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"


def run():
    idf = os.environ.get("IDF_PATH")
    if not idf:
        if os.environ.get("FMO_REQUIRE_NETWORK_TEST"):
            raise RuntimeError("IDF_PATH is required for real-cJSON network tests")
        print("FMO network parser: SKIP (activate ESP-IDF for real cJSON; required by firmware gate)")
        return
    cjson = Path(idf) / "components/json/cJSON"
    source = (ROOT / "main/fmo_network.c").read_text()
    header = (ROOT / "main/fmo_network.h").read_text()
    types = header[header.index("typedef enum"):header.index("/* Starts")]
    functions = "\n".join(extract_function(source, name) for name in (
        "update_clock_service", "request_radio_profile", "post_update", "json_bool", "copy_ascii", "invalidate_live_message", "parse_fmo_message", "request_current_channel", "cleanup_wifi", "prepare_network", "post_link"))
    preamble = r'''
#include "fmo_monitor_state.h"
#include "fmo_text.h"
#include "cJSON.h"
#include <assert.h>
#include <stdio.h>
#include <stdatomic.h>
#include <string.h>
#define portMAX_DELAY 0
#define ESP_LOGW(...) ((void)0)
typedef enum { FMO_SOCKET_EVENTS, FMO_SOCKET_CONTROL } fmo_socket_kind_t;
'''
    stubs = r'''
static fmo_snapshot_t s_snapshot, published;
static int s_state_lock, s_update_queue, locked, refreshes;
static bool s_query_pending;
static uint32_t s_speaker_revision, s_query_revision;
static uint64_t clock_ms = 10000, s_query_ms, s_radio_query_ms;
static bool s_radio_requested;
static bool s_clock_initialized, s_clock_online;
#define FMO_RADIO_REFRESH_MS 30000
static uint64_t now_ms(void) { return clock_ms; }
#define pdMS_TO_TICKS(x) (x)
#define pdTRUE 1
#define ESP_OK 0
#define ESP_LOGE(...) ((void)0)
typedef int esp_err_t;
typedef struct { bool wait_for_sync; const char *server; } esp_sntp_config_t;
#define ESP_NETIF_SNTP_DEFAULT_CONFIG(host) {true,host}
static int clock_inits, clock_restarts, clock_fail;
static int esp_netif_sntp_init(const esp_sntp_config_t *config) {
    assert(!config->wait_for_sync && !strcmp(config->server,"pool.ntp.org"));
    ++clock_inits;return clock_fail ? -1 : ESP_OK;
}
static int esp_netif_sntp_start(void) { ++clock_restarts;return clock_fail ? -1 : ESP_OK; }
static struct {int client;} s_control_socket = {1};
static int sends, short_send;
static char last_request[128];
static int esp_websocket_client_is_connected(int client) { return client; }
static int esp_websocket_client_send_text(int client, const char *data, size_t length, int wait) {
    (void)client; (void)wait; ++sends;
    snprintf(last_request,sizeof(last_request),"%s",data);
    return short_send ? 0 : (int)length;
}
static int storage_attempts, wifi_attempts, cleanups, backoffs, setup_checks;
static bool recovery_test, storage_good;
static atomic_bool s_retry_requested, s_reconnect, s_setup_active;
static int station_handle, *s_station;
static bool s_wifi_initialized;
static void *s_wifi_handler, *s_ip_handler;
static int s_wifi_bits, unregistered, destroyed;
#define WIFI_EVENT 1
#define ESP_EVENT_ANY_ID 0
#define IP_EVENT 2
#define IP_EVENT_STA_GOT_IP 1
#define FMO_WIFI_READY_BIT 1
#define FMO_WIFI_DISCONNECTED_BIT 2
#define FMO_WIFI_STOPPED_BIT 4
#define FMO_WIFI_CANCEL_BIT 8
static int esp_event_handler_instance_unregister(int base,int id,void *handler){(void)base;(void)id;assert(handler);++unregistered;return 0;}
static int esp_wifi_stop(void){assert(s_wifi_initialized);return 0;}
static int esp_wifi_deinit(void){assert(s_wifi_initialized);++cleanups;return 0;}
static void esp_netif_destroy_default_wifi(int *station){assert(station==&station_handle);++destroyed;}
static void xEventGroupClearBits(int bits,int mask){(void)bits;assert(mask==15);}
static void post_link(fmo_update_type_t type, bool connected);
static char last_error[48];
static int fmo_storage_prepare(void) {
    ++storage_attempts;
    storage_good = !recovery_test || storage_attempts > 1;
    return storage_good ? 0 : -1;
}
static int start_wifi(void) {
    assert(storage_good); ++wifi_attempts;
    s_station=&station_handle;s_wifi_initialized=true;
    s_wifi_handler=(void *)1;s_ip_handler=(void *)2;
    return !recovery_test || wifi_attempts > 1 ? 0 : -1;
}
static void check_setup_request(void) { ++setup_checks; }
static void post_error(const char *error) { snprintf(last_error,sizeof(last_error),"%s",error); }
static void ulTaskNotifyTake(int clear, int wait) {
    assert(clear == pdTRUE && wait == 5000); ++backoffs;
    // Simulate a user retry while startup is in its error backoff.
    atomic_store(&s_retry_requested,true);
}
static void xSemaphoreTake(int handle, int delay) {
    (void)handle; (void)delay; assert(!locked); locked = 1;
}
static void xSemaphoreGive(int handle) { (void)handle; assert(locked); locked = 0; }
static void xQueueOverwrite(int queue, const fmo_snapshot_t *state) {
    (void)queue; assert(locked); published = *state;
}
static void fmo_network_request_refresh(void) { assert(!locked); ++refreshes; }
static void query(void) { s_query_pending = true; s_query_revision = s_speaker_revision; }
'''
    tests = r'''
#define CHANNEL42 "{\"type\":\"station\",\"subType\":\"getCurrentResponse\",\"data\":{\"uid\":42,\"name\":\"安吉FMO中继\"}}"
#define CHANNEL43 "{\"type\":\"station\",\"subType\":\"getCurrentResponse\",\"data\":{\"uid\":43,\"name\":\"上海\"}}"
#define START "{\"type\":\"qso\",\"subType\":\"callsign\",\"data\":{\"callsign\":\"BG5ESN\",\"isSpeaking\":true,\"grid\":\"PM01\"}}"
int main(void) {
    fmo_update_t link = {.type=FMO_UPDATE_WIFI, .connected=true};
    post_update(&link); link.type=FMO_UPDATE_EVENTS_LINK; post_update(&link);
    link.type=FMO_UPDATE_CONTROL_LINK; post_update(&link);
    query(); parse_fmo_message(FMO_SOCKET_CONTROL, CHANNEL42);
    assert(published.state.channel_valid && !strcmp(published.state.channel_name, "安吉FMO中继"));
    // A known channel stays visible at PTT start, even across a stale query reply.
    uint64_t confirmed = published.state.channel_confirmed_ms;
    ++clock_ms;
    query(); parse_fmo_message(FMO_SOCKET_EVENTS, START);
    assert(s_snapshot.state.speaking && s_snapshot.state.channel_valid);
    assert(published.speech_activity == 1);
    assert(published.state.channel_confirmed_ms == confirmed);
    int before_start = refreshes;
    parse_fmo_message(FMO_SOCKET_EVENTS, START); // Duplicate start, no new refresh.
    assert(published.state.channel_valid && refreshes == before_start);
    parse_fmo_message(FMO_SOCKET_CONTROL, CHANNEL43);
    assert(s_snapshot.state.speaking && !strcmp(s_snapshot.state.speaker, "BG5ESN"));
    assert(s_snapshot.state.channel_valid && s_snapshot.state.channel_uid == 42);
    query(); parse_fmo_message(FMO_SOCKET_CONTROL, CHANNEL42);
    assert(s_snapshot.state.channel_valid && s_snapshot.state.speaking);
    parse_fmo_message(FMO_SOCKET_EVENTS,
        "{\"type\":\"qso\",\"subType\":\"callsign\",\"data\":{\"callsign\":\"OTHER\",\"isSpeaking\":false}}");
    assert(s_snapshot.state.speaking);
    parse_fmo_message(FMO_SOCKET_EVENTS,
        "{\"type\":\"qso\",\"subType\":\"callsign\",\"data\":{\"callsign\":\"BG5ESN\",\"isSpeaking\":0}}");
    assert(!s_snapshot.state.speaking && !strcmp(s_snapshot.state.last_speaker, "BG5ESN"));
    assert(published.speech_activity == 2);
    assert(published.state.channel_valid);
    // Explicit idle notifications need no callsign, grid or host metadata.
    const char *releases[] = {
        "{\"type\":\"qso\",\"subType\":\"callsign\",\"data\":{\"callsign\":\"\",\"isSpeaking\":false}}",
        "{\"type\":\"qso\",\"subType\":\"callsign\",\"data\":{\"callsign\":null,\"isSpeaking\":false}}",
        "{\"type\":\"qso\",\"subType\":\"callsign\",\"data\":{\"isSpeaking\":false}}",
        "{\"type\":\"qso\",\"subType\":\"callsign\",\"data\":{\"isSpeaking\":0}}",
        "{\"type\":\"qso\",\"subType\":\"callsign\",\"data\":{\"callsign\":\"BG5ESN\",\"isSpeaking\":false,\"grid\":123,\"isHost\":null}}"
    };
    for (unsigned i=0; i<sizeof(releases)/sizeof(releases[0]); ++i) {
        parse_fmo_message(FMO_SOCKET_EVENTS, START);
        query(); parse_fmo_message(FMO_SOCKET_CONTROL, CHANNEL42);
        query(); // A normal release must preserve ownership of an in-flight query.
        int before=refreshes; uint32_t revision=s_speaker_revision;
        ++clock_ms; parse_fmo_message(FMO_SOCKET_EVENTS, releases[i]);
        assert(published.state.channel_valid && published.state.channel_uid==42);
        assert(!published.state.speaking && !published.state.speaker[0]);
        assert(!strcmp(published.state.last_speaker,"BG5ESN") && !strcmp(published.state.grid,"PM01"));
        assert(published.state.last_speaker_ms==clock_ms);
        assert(refreshes==before && s_speaker_revision==revision && s_query_pending);
        parse_fmo_message(FMO_SOCKET_CONTROL, CHANNEL42);
        uint64_t ended=published.state.last_speaker_ms;
        ++clock_ms; parse_fmo_message(FMO_SOCKET_EVENTS, releases[i]);
        assert(published.state.channel_valid && published.state.last_speaker_ms==ended);
    }
    // A different talker and a short PTT keep the confirmed channel visible.
    query(); parse_fmo_message(FMO_SOCKET_EVENTS, START);
    int before_talker = refreshes;
    parse_fmo_message(FMO_SOCKET_EVENTS,
        "{\"type\":\"qso\",\"subType\":\"callsign\",\"data\":{\"callsign\":\"TEST/2\",\"isSpeaking\":true}}");
    assert(published.state.channel_valid && published.state.speaking);
    assert(!strcmp(published.state.speaker,"TEST/2") && refreshes==before_talker+1);
    parse_fmo_message(FMO_SOCKET_EVENTS, releases[0]);
    assert(published.state.channel_valid && !published.state.speaking);
    parse_fmo_message(FMO_SOCKET_CONTROL, CHANNEL43);
    assert(published.state.channel_valid && published.state.channel_uid==42);
    assert(!strcmp(published.state.last_speaker,"TEST/2"));
    // Without a confirmed channel, a short PTT cannot bless a stale response.
    link.type=FMO_UPDATE_EVENTS_LINK; link.connected=false; post_update(&link);
    link.connected=true; post_update(&link);
    query(); parse_fmo_message(FMO_SOCKET_EVENTS, START);
    parse_fmo_message(FMO_SOCKET_EVENTS, releases[0]);
    assert(!published.state.channel_valid && !published.state.speaking);
    parse_fmo_message(FMO_SOCKET_CONTROL, CHANNEL43);
    assert(!published.state.channel_valid && published.state.channel_uid==42);
    query(); parse_fmo_message(FMO_SOCKET_CONTROL, CHANNEL42);
    assert(published.state.channel_valid && !published.state.speaking);
    assert(!strcmp(published.state.last_speaker,"BG5ESN"));
    query(); parse_fmo_message(FMO_SOCKET_CONTROL, CHANNEL43);
    assert(!s_snapshot.state.last_speaker[0]);
    query(); parse_fmo_message(FMO_SOCKET_CONTROL,
        "{\"type\":\"station\",\"subType\":\"getCurrentResponse\",\"data\":{\"uid\":43,\"name\":\"一二三四五六七八九十十一十二十三十四十五十六\"}}");
    assert(!strcmp(s_snapshot.state.channel_name, "一二三四五六七八九十"));
    const char *bad[] = {"{", "{}garbage",
        "{\"type\":\"qso\",\"subType\":\"callsign\",\"data\":{\"callsign\":\"\",\"isSpeaking\":true}}",
        "{\"type\":\"qso\",\"subType\":\"callsign\",\"data\":{\"callsign\":null,\"isSpeaking\":true}}",
        "{\"type\":\"qso\",\"subType\":\"callsign\",\"data\":{\"isSpeaking\":true}}",
        "{\"type\":\"qso\",\"subType\":\"callsign\",\"data\":{\"callsign\":123,\"isSpeaking\":false}}",
        "{\"type\":\"qso\",\"subType\":\"callsign\",\"data\":null}",
        "{\"type\":\"qso\",\"subType\":\"callsign\",\"data\":{\"callsign\":\"BG5ESN\",\"isSpeaking\":\"false\"}}",
        "{\"type\":\"qso\",\"subType\":\"callsign\",\"data\":{\"callsign\":\"\\u4e0a\",\"isSpeaking\":false}}",
        "{\"type\":\"qso\",\"subType\":\"callsign\",\"data\":{\"callsign\":\"BG5ESN\\u0000/OTHER\",\"isSpeaking\":false}}",
        "{\"type\":\"qso\",\"subType\":\"callsign\",\"data\":{\"callsign\":\"1234567890123456\",\"isSpeaking\":true}}",
        "{\"type\":\"qso\",\"subType\":\"callsign\",\"data\":{\"callsign\":\"BG5ESN\",\"grid\":\"123456789012\",\"isSpeaking\":true}}"};
    for (unsigned i=0; i<sizeof(bad)/sizeof(bad[0]); ++i) {
        parse_fmo_message(FMO_SOCKET_EVENTS, START); query();
        parse_fmo_message(FMO_SOCKET_EVENTS, bad[i]);
        assert(!published.state.speaking && !published.state.speaker[0]);
        assert(!published.state.channel_valid && s_query_pending);
        parse_fmo_message(FMO_SOCKET_CONTROL, CHANNEL43);
        assert(!s_snapshot.state.channel_valid);
    }
    parse_fmo_message(FMO_SOCKET_EVENTS, START);
    parse_fmo_message(FMO_SOCKET_EVENTS, "{\"type\":\"other\",\"subType\":\"event\",\"data\":{}}");
    assert(s_snapshot.state.speaking);
    query(); link.type=FMO_UPDATE_WIFI; link.connected=false; post_update(&link);
    parse_fmo_message(FMO_SOCKET_CONTROL, CHANNEL42);
    parse_fmo_message(FMO_SOCKET_EVENTS, START);
    assert(!s_snapshot.state.channel_valid && !s_snapshot.state.speaking);
    assert(refreshes > 0 && !locked);
    // Exercise the actual coordinator query function with a controllable clock.
    link.connected=true; post_update(&link);
    link.type=FMO_UPDATE_EVENTS_LINK; post_update(&link);
    link.type=FMO_UPDATE_CONTROL_LINK; post_update(&link);
    query(); parse_fmo_message(FMO_SOCKET_CONTROL, CHANNEL42);
    clock_ms=10000; assert(!request_current_channel());
    parse_fmo_message(FMO_SOCKET_EVENTS, START);
    clock_ms=11999; assert(!request_current_channel()); assert(sends==1);
    assert(published.state.channel_valid && published.state.speaking);
    clock_ms=12000; assert(request_current_channel()); assert(sends==1);
    assert(!s_query_pending && !s_snapshot.state.control_connected);
    parse_fmo_message(FMO_SOCKET_CONTROL, CHANNEL43); // Delayed Q1, before client is drained.
    assert(s_snapshot.state.speaking && s_snapshot.state.channel_uid==42);
    assert(!s_snapshot.state.channel_valid);
    post_update(&link); // Coordinator has drained/replaced the old client.
    assert(!request_current_channel()); assert(sends==2);
    parse_fmo_message(FMO_SOCKET_CONTROL, CHANNEL42);
    assert(s_snapshot.state.channel_valid && s_snapshot.state.speaking);
    short_send=1;assert(request_current_channel());assert(!s_query_pending);
    parse_fmo_message(FMO_SOCKET_CONTROL, CHANNEL43);
    assert(s_snapshot.state.speaking && s_snapshot.state.channel_uid==42);

    // Real config responses: MHz units, Chinese antenna/name and valid zero height.
    short_send=0; link.type=FMO_UPDATE_CONTROL_LINK; link.connected=true; post_update(&link);
    query(); parse_fmo_message(FMO_SOCKET_CONTROL, CHANNEL42);
    parse_fmo_message(FMO_SOCKET_EVENTS, START);
    query(); uint32_t radio_revision=s_speaker_revision;
    const char *profile[] = {
        "{\"type\":\"config\",\"subType\":\"getUserPhyDeviceNameResponse\",\"data\":{\"deviceName\":\"QUANSHENG\"}}",
        "{\"type\":\"config\",\"subType\":\"getUserPhyFreqResponse\",\"data\":{\"freq\":439.875}}",
        "{\"type\":\"config\",\"subType\":\"getUserPhyAntResponse\",\"data\":{\"ant\":\"示例GP\"}}",
        "{\"type\":\"config\",\"subType\":\"getUserPhyAntHeightResponse\",\"data\":{\"height\":63}}"
    };
    for(unsigned i=0;i<4;++i)parse_fmo_message(FMO_SOCKET_CONTROL,profile[i]);
    assert(!strcmp(published.state.radio.device_name,"QUANSHENG"));
    assert(!strcmp(published.state.radio.antenna,"示例GP"));
    assert(published.state.radio.frequency_100hz==4398750);
    assert(published.state.radio.height_valid && published.state.radio.antenna_height_m==63);
    assert(published.state.channel_valid && published.state.speaking);
    assert(s_query_pending && s_speaker_revision==radio_revision);
    parse_fmo_message(FMO_SOCKET_CONTROL,
        "{\"type\":\"config\",\"subType\":\"getUserPhyAntHeightResponse\",\"data\":{\"height\":0}}");
    assert(published.state.radio.height_valid && published.state.radio.antenna_height_m==0);
    const char *bad_profile[] = {
        "{\"type\":\"config\",\"subType\":\"getUserPhyFreqResponse\",\"data\":{\"freq\":1001}}",
        "{\"type\":\"config\",\"subType\":\"getUserPhyAntHeightResponse\",\"data\":{\"height\":1.5}}",
        "{\"type\":\"config\",\"subType\":\"getUserPhyDeviceNameResponse\",\"data\":{\"deviceName\":12}}",
        "{\"type\":\"config\",\"subType\":\"getUserPhyAntResponse\",\"data\":{\"ant\":\"bad\\ntext\"}}"
    };
    for(unsigned i=0;i<4;++i)parse_fmo_message(FMO_SOCKET_CONTROL,bad_profile[i]);
    assert(!published.state.radio.frequency_100hz && !published.state.radio.height_valid);
    assert(!published.state.radio.device_name[0] && !published.state.radio.antenna[0]);
    assert(published.state.channel_valid && published.state.speaking && s_query_pending);
    int radio_sends=sends;
    assert(!request_radio_profile() && sends==radio_sends+4);
    assert(strstr(last_request,"getUserPhyAntHeight") && !strstr(last_request,"set"));
    assert(!request_radio_profile() && sends==radio_sends+4);
    clock_ms+=29999; assert(!request_radio_profile() && sends==radio_sends+4);
    ++clock_ms; assert(!request_radio_profile() && sends==radio_sends+8);
    for(unsigned i=0;i<4;++i)parse_fmo_message(FMO_SOCKET_CONTROL,profile[i]);
    link.connected=false;post_update(&link);
    assert(!published.state.radio.device_name[0] && !s_radio_requested);
    for(unsigned i=0;i<4;++i)parse_fmo_message(FMO_SOCKET_CONTROL,profile[i]);
    assert(!published.state.radio.frequency_100hz && !published.state.radio.height_valid);
    link.connected=true;post_update(&link);
    assert(!request_radio_profile() && sends==radio_sends+12);
    short_send=1;clock_ms+=30000;assert(request_radio_profile());
    short_send=0;
    // Clock init is asynchronous and owned once; failures/reconnects stay independent of PTT.
    update_clock_service(false);assert(clock_inits==0);
    clock_fail=1;update_clock_service(true);
    assert(clock_inits==1 && !s_clock_initialized && !s_clock_online);
    clock_fail=0;update_clock_service(true);update_clock_service(true);
    assert(clock_inits==2 && s_clock_initialized && s_clock_online);
    update_clock_service(false);update_clock_service(true);
    assert(clock_inits==2 && clock_restarts==1 && s_clock_online);
    assert(s_snapshot.state.speaking);
    recovery_test=true;prepare_network();
    assert(storage_attempts==3 && wifi_attempts==2 && cleanups==1 && backoffs==2);
    assert(setup_checks==2 && !last_error[0] && !atomic_load(&s_retry_requested));
    assert(unregistered==2 && destroyed==1);
    // Failed initialization before Wi-Fi init still releases the STA netif.
    s_wifi_initialized=false;s_wifi_handler=s_ip_handler=NULL;s_station=&station_handle;
    cleanup_wifi();assert(unregistered==2 && destroyed==2 && cleanups==1 && !s_station);
    puts("FMO network parser: PASS (radio metadata, PTT background refresh, UTF-8, stale replies, invalid releases, reconnect)");
}
'''
    with tempfile.TemporaryDirectory(prefix="fmo-network-test-") as tmp:
        file = Path(tmp) / "network.c"
        file.write_text(preamble + types + stubs + functions + tests)
        binary = Path(tmp) / "network"
        subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                        "-I" + str(ROOT / "main"), "-I" + str(cjson), str(file),
                        str(cjson / "cJSON.c"), str(ROOT / "main/fmo_monitor_state.c"),
                        str(ROOT / "main/fmo_text.c"), "-lm", "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    run()
