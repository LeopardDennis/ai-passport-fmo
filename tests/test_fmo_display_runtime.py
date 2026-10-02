"""Test production LVGL-owned sleep/wake ordering with hardware stubs."""
import os
from pathlib import Path
import subprocess
import tempfile
from test_fmo_network import extract_function

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "main/main.c").read_text()
globals_source = source[source.index("typedef enum"):source.index("static uint64_t monotonic_ms")]
header = (ROOT / "main/fmo_network.h").read_text()
network_types = header[header.index("typedef enum"):header.index("/* Starts")]
functions = "\n".join(extract_function(source, name) for name in ("apply_input", "update_display", "ui_tick"))
preamble = r'''
#include "fmo_display_policy.h"
#include "fmo_controls.h"
#include "fmo_monitor_state.h"
#include <stdatomic.h>
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#define CONFIG_FMO_BACKLIGHT 80
#define BSP_BTN_COUNT 3
#define pdTRUE 1
#define ESP_OK 0
typedef enum { BSP_BTN_UP, BSP_BTN_DOWN, BSP_BTN_OK } bsp_btn_t;
typedef enum { BSP_BTN_PRESS, BSP_BTN_CLICK, BSP_BTN_DOUBLE, BSP_BTN_LONG } bsp_btn_ev_t;
typedef int QueueHandle_t;
typedef int TaskHandle_t;
typedef int esp_err_t;
typedef int lv_display_t;
typedef int lv_timer_t;
'''
stubs = r'''
static uint64_t clock_ms;
static int paused, panel_dark, brightness=80, fail_sleep, fail_wake;
static int renders, notices, requests, setups, retries, cancels;
static bool has_snapshot;
static fmo_snapshot_t next_snapshot;
static char trace[100];
static void mark(char c) { size_t n=strlen(trace); assert(n+1<sizeof(trace)); trace[n]=c; trace[n+1]=0; }
static uint64_t monotonic_ms(void) { return clock_ms; }
static void fmo_network_request_refresh(void) { ++requests; }
static void fmo_network_request_setup(void) { ++setups; }
static void fmo_network_request_retry(void) { ++retries; }
static void fmo_network_cancel_setup(void) { ++cancels; }
static lv_display_t *lv_display_get_default(void) { static int display; return &display; }
static lv_timer_t *lv_display_get_refr_timer(lv_display_t *d) { return d; }
static void lv_timer_pause(lv_timer_t *t) { (void)t; mark('p'); paused=1; }
static void lv_timer_resume(lv_timer_t *t) { (void)t; mark('r'); paused=0; }
static void bsp_display_backlight(uint8_t value) { mark('b'); brightness=value; }
static esp_err_t bsp_display_sleep(bool sleep) {
    assert(paused); mark(sleep ? 's' : 'w'); brightness=0;
    if ((sleep && fail_sleep) || (!sleep && fail_wake)) return -1;
    panel_dark=sleep; return ESP_OK;
}
static void *lv_screen_active(void) { return NULL; }
static void lv_obj_invalidate(void *s) { (void)s; assert(!panel_dark && !paused); mark('i'); }
static void fmo_ui_render(const fmo_monitor_state_t *s, const char *e, const char *ssid,
                         const char *pw, int soc, uint64_t now, bool hint, const fmo_controls_t *controls) {
    (void)s; (void)e; (void)ssid; (void)pw; (void)soc; (void)now; (void)hint; (void)controls;
    assert(!panel_dark && !paused); ++renders; mark('d');
}
static void lv_refr_now(lv_display_t *d) {
    (void)d; assert(!panel_dark && brightness==0 && !paused); mark('f');
}
static void xTaskNotifyGive(TaskHandle_t t) { assert(t); ++notices; }
static int xQueueReceive(QueueHandle_t q, void *v, int wait) {
    (void)wait;
    if (q == s_fmo_queue && has_snapshot) {
        memcpy(v, &next_snapshot, sizeof(next_snapshot)); has_snapshot=false; return pdTRUE;
    }
    return 0;
}
'''
tests = r'''
static void key(bsp_btn_t button, bsp_btn_ev_t event) {
    app_input_t input = {.type=APP_INPUT_BUTTON, .value=button, .event=event};
    apply_input(&input);
}
int main(void) {
    s_battery_task=1;
    assert(update_display(0)); assert(brightness==80);
    assert(update_display(30000)); assert(brightness==20);
    clock_ms=90000; trace[0]=0; ui_tick(NULL);
    assert(panel_dark && paused && atomic_load(&s_display_dark) && brightness==0);
    assert(!strcmp(trace,"ps") && renders==0);
    // Press and long press belong to one wake gesture; neither reconfigures Wi-Fi.
    clock_ms=90001; key(BSP_BTN_OK, BSP_BTN_PRESS); trace[0]=0;
    assert(update_display(clock_ms));
    assert(!strcmp(trace,"wridfb") && brightness==80 && notices==1);
    key(BSP_BTN_OK, BSP_BTN_LONG); assert(!setups && !requests);
    key(BSP_BTN_OK, BSP_BTN_PRESS); key(BSP_BTN_OK, BSP_BTN_LONG);
    assert(!setups && s_controls.view==FMO_VIEW_NETWORK);
    key(BSP_BTN_OK, BSP_BTN_CLICK); assert(setups==1);
    fmo_controls_observe_setup(&s_controls, true);
    key(BSP_BTN_OK, BSP_BTN_CLICK); assert(s_controls.setup_info);
    key(BSP_BTN_OK, BSP_BTN_LONG); assert(cancels==1);
    fmo_controls_observe_setup(&s_controls, false);
    key(BSP_BTN_DOWN, BSP_BTN_CLICK); key(BSP_BTN_OK, BSP_BTN_CLICK);
    assert(retries==1 && s_controls.view==FMO_VIEW_MONITOR);
    key(BSP_BTN_UP, BSP_BTN_PRESS); key(BSP_BTN_UP, BSP_BTN_CLICK);
    assert(update_display(clock_ms)); assert(brightness==90);
    // Failed sleep restores refresh and backlight; retries are rate-limited.
    clock_ms += 90000; fail_sleep=1; trace[0]=0;
    assert(update_display(clock_ms));
    assert(!paused && !panel_dark && !atomic_load(&s_display_dark) && brightness==90);
    assert(!strcmp(trace,"psrb")); trace[0]=0;
    assert(update_display(clock_ms+500) && !trace[0]);
    fail_sleep=0; clock_ms+=1000; assert(!update_display(clock_ms));
    // Failed wake keeps refresh paused and backlight off until a later success.
    key(BSP_BTN_DOWN, BSP_BTN_PRESS); fail_wake=1;
    assert(!update_display(clock_ms)); assert(paused && panel_dark && brightness==0);
    assert(!update_display(clock_ms+500));
    fail_wake=0; clock_ms+=1000; assert(update_display(clock_ms));
    key(BSP_BTN_DOWN, BSP_BTN_CLICK); assert(s_brightness==90 && notices==2);
    // A valid PTT wakes before channel confirmation; continuous speech stays awake.
    clock_ms+=90000; assert(!update_display(clock_ms));
    s_state.wifi_connected=s_state.events_connected=s_state.speaking=true;
    assert(update_display(clock_ms+1)); assert(!panel_dark);
    assert(update_display(clock_ms+600000)); assert(brightness==90);
    s_state.speaking=false;
    assert(!update_display(clock_ms+690000));
    strcpy(s_setup_ssid,"FMO-Setup-TEST");
    assert(update_display(clock_ms+690001));
    assert(update_display(clock_ms+990001)); assert(!panel_dark);
    s_setup_ssid[0]=0; s_state.speaking=false;
    clock_ms += 1200000; assert(!update_display(clock_ms));
    // A complete short PTT can be overwritten by its release in the one-slot queue.
    next_snapshot.state=s_state; next_snapshot.speech_activity=1;
    s_fmo_queue=1; s_input_queue=2; has_snapshot=true;
    ui_tick(NULL); assert(!panel_dark && !s_state.speaking && s_speech_activity==1);
    s_brightness=95; key(BSP_BTN_UP, BSP_BTN_CLICK); assert(s_brightness==100);
    s_brightness=25; key(BSP_BTN_DOWN, BSP_BTN_CLICK); assert(s_brightness==20);
    puts("FMO display runtime: PASS (gesture, redraw, PTT/setup, failure recovery)");
}
'''
with tempfile.TemporaryDirectory(prefix="fmo-display-test-") as tmp:
    file = Path(tmp) / "display.c"
    file.write_text(preamble + network_types + globals_source + stubs + functions + tests)
    binary = Path(tmp) / "display"
    subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-I" + str(ROOT / "main"), str(file), str(ROOT / "main/fmo_display_policy.c"), str(ROOT / "main/fmo_controls.c"),
                    "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
