"""Run the production audio worker against deterministic WebSocket/codec stubs."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
header = r'''
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM -2
#define ESP_ERR_INVALID_STATE -3
#define ESP_ERR_INVALID_ARG -4
typedef const char *esp_event_base_t;
typedef void *TaskHandle_t;
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define pdTRUE 1
#define pdPASS 1
#define pdMS_TO_TICKS(x) (x)
void lock_pcm(void);
void unlock_pcm(void);
#define taskENTER_CRITICAL(x) ((void)(x),lock_pcm())
#define taskEXIT_CRITICAL(x) ((void)(x),unlock_pcm())
static inline void discard_log(const char *format, ...) {(void)format;}
#define ESP_LOGI(tag,...) ((void)(tag),discard_log(__VA_ARGS__))
#define ESP_LOGW(tag,...) ((void)(tag))
const char *esp_err_to_name(esp_err_t err);
int64_t esp_timer_get_time(void);
int xTaskCreate(void (*task)(void *), const char *name, unsigned stack, void *arg, unsigned priority, TaskHandle_t *handle);
void xTaskNotifyGive(TaskHandle_t task);
unsigned ulTaskNotifyTake(int clear, unsigned timeout);
void vTaskDelay(unsigned ticks);
typedef struct {char host[64]; unsigned short port;} fmo_endpoint_t;
void fmo_provision_get_endpoint(fmo_endpoint_t *endpoint);
typedef enum {WIFI_PS_NONE, WIFI_PS_MIN_MODEM, WIFI_PS_MAX_MODEM} wifi_ps_type_t;
esp_err_t esp_wifi_get_ps(wifi_ps_type_t *ps);
esp_err_t esp_wifi_set_ps(wifi_ps_type_t ps);
esp_err_t bsp_audio_init(void);
esp_err_t bsp_audio_set_format(uint32_t rate,uint8_t bits,uint8_t channels);
void bsp_audio_set_volume(uint8_t percent);
esp_err_t bsp_audio_write(const void *pcm,size_t bytes);
typedef struct {int generation;} client_t;
typedef client_t *esp_websocket_client_handle_t;
typedef struct {
    const char *uri; int buffer_size, task_stack;
    bool enable_close_reconnect, disable_pingpong_discon;
    int reconnect_timeout_ms, network_timeout_ms, ping_interval_sec, pingpong_timeout_sec;
} esp_websocket_client_config_t;
typedef struct {int payload_len,payload_offset,data_len,op_code; bool fin; char *data_ptr;} esp_websocket_event_data_t;
#define WEBSOCKET_EVENT_ANY 0
#define WEBSOCKET_EVENT_CONNECTED 1
#define WEBSOCKET_EVENT_DISCONNECTED 2
#define WEBSOCKET_EVENT_CLOSED 3
#define WEBSOCKET_EVENT_ERROR 4
#define WEBSOCKET_EVENT_DATA 5
typedef void (*callback_t)(void *,esp_event_base_t,int32_t,void *);
esp_websocket_client_handle_t esp_websocket_client_init(const esp_websocket_client_config_t *config);
esp_err_t esp_websocket_register_events(esp_websocket_client_handle_t client,int id,callback_t callback,void *arg);
esp_err_t esp_websocket_client_start(esp_websocket_client_handle_t client);
esp_err_t esp_websocket_client_stop(esp_websocket_client_handle_t client);
esp_err_t esp_websocket_client_destroy(esp_websocket_client_handle_t client);
'''
tests = r'''
#include "audio_env.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "fmo_audio.c"
static jmp_buf done;
static int mode, locked, codec_inits, formats, create_calls, starts, stops, destroys;
static int applied, writes, waits, notifications, task_creates, prime_writes;
static int init_fail, register_fail, start_fail, codec_fail, task_fail;
static uint64_t clock_ms;
static wifi_ps_type_t wifi_ps;
static unsigned wifi_awake_calls, wifi_restore_calls;
static unsigned drained_samples;
static callback_t callback;
static client_t fake_client;
static uint8_t incoming[5120];
void lock_pcm(void) {assert(!locked);locked=1;}
void unlock_pcm(void) {assert(locked);locked=0;}
const char *esp_err_to_name(esp_err_t err) {(void)err;return "stub";}
int64_t esp_timer_get_time(void) {return (int64_t)clock_ms*1000;}
int xTaskCreate(void (*task)(void *),const char *name,unsigned stack,void *arg,unsigned priority,TaskHandle_t *handle) {
    assert(task==audio_task && !strcmp(name,"fmo_audio") && stack==4096 && !arg && priority==3);
    ++task_creates;if(task_fail)return 0;*handle=&fake_client;return pdPASS;
}
void xTaskNotifyGive(TaskHandle_t task) {assert(task);++notifications;}
void vTaskDelay(unsigned ticks) {
    assert(!locked);clock_ms+=ticks;
    if(mode==8)fmo_audio_set_volume(0);
    if(mode==6 && clock_ms%20==0) {
        int16_t out[160];
        lock_pcm();
        size_t n=fmo_pcm_read(&s_pcm,out,160,clock_ms);
        unlock_pcm();
        for(size_t i=0;i<n;++i)assert(out[i]==0x1234);
        drained_samples+=(unsigned)n;
    }
    if(mode==5 && applied>0) {
        if(writes==1) {
            uint8_t level=fmo_audio_get_level();assert(level>0);
            fmo_audio_set_volume(10);assert(fmo_audio_get_level()==level);
            clock_ms+=121;assert(fmo_audio_get_level()==0);clock_ms-=121;
        }
        if(writes==2) {assert(fmo_audio_get_level()==0);longjmp(done,1);}
    }
}
void fmo_provision_get_endpoint(fmo_endpoint_t *e) {strcpy(e->host,"fmo.test");e->port=8080;}
esp_err_t bsp_audio_init(void) {assert(!locked);++codec_inits;return codec_fail ? ESP_FAIL : ESP_OK;}
esp_err_t bsp_audio_set_format(uint32_t rate,uint8_t bits,uint8_t channels) {
    assert(!locked && rate==8000 && bits==16 && channels==1);++formats;return ESP_OK;
}
esp_err_t esp_wifi_get_ps(wifi_ps_type_t *ps) {assert(!locked);*ps=wifi_ps;return ESP_OK;}
esp_err_t esp_wifi_set_ps(wifi_ps_type_t ps) {
    assert(!locked);wifi_ps=ps;
    if(ps==WIFI_PS_NONE)++wifi_awake_calls;else ++wifi_restore_calls;
    return ESP_OK;
}
void bsp_audio_set_volume(uint8_t volume) {assert(!locked);applied=volume;}
esp_websocket_client_handle_t esp_websocket_client_init(const esp_websocket_client_config_t *c) {
    assert(!locked && !strcmp(c->uri,"ws://fmo.test:8080/audio") && c->buffer_size==1024 && c->task_stack==4096);
    assert(c->network_timeout_ms==10000 && c->ping_interval_sec==10 && c->disable_pingpong_discon);
    if(mode>0)assert(wifi_ps==WIFI_PS_NONE);
    ++create_calls;if(init_fail){--init_fail;return NULL;}fake_client.generation++;return &fake_client;
}
esp_err_t esp_websocket_register_events(esp_websocket_client_handle_t c,int id,callback_t cb,void *arg) {
    assert(!locked && c==&fake_client && id==WEBSOCKET_EVENT_ANY && !arg);
    callback=cb;return register_fail ? ESP_FAIL : ESP_OK;
}
static void send_frame(void) {
    int16_t value=starts==1 ? 0x1234 : 0x5678;
    for(size_t i=0;i<sizeof(incoming)/2;++i){incoming[2*i]=(uint8_t)value;incoming[2*i+1]=(uint8_t)((uint16_t)value>>8);}
    for(int off=0;off<5120;off+=1024) {
        esp_websocket_event_data_t data={.payload_len=5120,.payload_offset=off,.data_len=1024,
            .op_code=2,.fin=true,.data_ptr=(char *)incoming+off};
        callback(NULL,NULL,WEBSOCKET_EVENT_DATA,&data);
    }
}
esp_err_t esp_websocket_client_start(esp_websocket_client_handle_t c) {
    assert(!locked && c==&fake_client);++starts;
    if(start_fail)return ESP_FAIL;
    callback(NULL,NULL,WEBSOCKET_EVENT_CONNECTED,NULL);send_frame();return ESP_OK;
}
esp_err_t esp_websocket_client_stop(esp_websocket_client_handle_t c) {
    assert(!locked && c==&fake_client && applied==0);++stops;
    /* Even a queued callback during shutdown cannot reintroduce old audio. */
    send_frame();assert(!s_pcm.count);
    callback(NULL,NULL,WEBSOCKET_EVENT_DISCONNECTED,NULL);return ESP_OK;
}
esp_err_t esp_websocket_client_destroy(esp_websocket_client_handle_t c) {
    assert(!locked && c==&fake_client);++destroys;return ESP_OK;
}
esp_err_t bsp_audio_write(const void *pcm,size_t bytes) {
    assert(!locked && bytes==320);clock_ms+=20;
    const int16_t *samples=pcm;
    if(applied==0) {
        ++prime_writes;
        for(size_t i=0;i<160;++i)assert(samples[i]==0);
        return ESP_OK;
    }
    ++writes;
    if(mode==1) {
        int16_t expected=starts==1 ? 0x1234 : 0x5678;
        for(size_t i=0;i<160;++i)assert(samples[i]==expected);
        if(writes==1){assert(applied==50 && prime_writes==16);fmo_audio_set_volume(60);}
        if(writes==2){assert(applied==60);fmo_audio_set_volume(0);}
        if(writes==3){assert(applied==70 && prime_writes==32);fmo_audio_set_online(false);}
    } else if(mode==2) {
        if(!starts)for(size_t i=0;i<160;++i)assert(samples[i]==0);
        clock_ms+=1000;
        if(writes==12){assert(create_calls==3 && starts==1);longjmp(done,1);}
    } else if(mode==7) {
        if(writes==1){assert(starts==1);clock_ms+=FMO_AUDIO_ACTIVITY_TIMEOUT_MS;}
        else if(starts==2) {
            for(size_t i=0;i<160;++i)assert(samples[i]==0x5678);
            assert(stops==1 && destroys==1 && prime_writes==32);
            longjmp(done,1);
        } else for(size_t i=0;i<160;++i)assert(samples[i]==0);
    } else if(mode==4) return ESP_FAIL;
    else if(mode==5 && writes==2) {
        /* A reconnect while codec I/O blocks must not publish the old chunk. */
        callback(NULL,NULL,WEBSOCKET_EVENT_DISCONNECTED,NULL);
        callback(NULL,NULL,WEBSOCKET_EVENT_CONNECTED,NULL);
    }
    return ESP_OK;
}
unsigned ulTaskNotifyTake(int clear,unsigned timeout) {
    assert(!locked && clear==pdTRUE && timeout>0);++waits;clock_ms+=timeout;
    if(mode==1) {
        assert(wifi_ps==WIFI_PS_MAX_MODEM);
        assert(applied==0 && !s_pcm.count);
        assert(fmo_audio_get_level()==0);
        if(waits==1){assert(stops==1 && destroys==1);fmo_audio_set_volume(70);fmo_audio_set_online(false);}
        if(waits==2){assert(starts==1);fmo_audio_set_online(true);}
        if(waits==3){assert(stops==2 && destroys==2 && codec_inits==1 && formats==1);longjmp(done,1);}
    }
    if((mode==3 || mode==4) && waits==3) {
        assert(codec_inits==1);
        if(mode==3)assert(formats==0 && starts==0 && writes==0);
        else assert(starts==1 && stops==1 && destroys==1 && applied==0 && writes==1);
        longjmp(done,1);
    }
    return 0;
}
static void reset_test(int scenario) {
    mode=scenario;locked=codec_inits=formats=create_calls=starts=stops=destroys=0;
    applied=writes=waits=notifications=prime_writes=0;clock_ms=1000;
    init_fail=register_fail=start_fail=codec_fail=0;callback=NULL;
    wifi_ps=WIFI_PS_MAX_MODEM;wifi_awake_calls=wifi_restore_calls=0;
    reset_stream(false);atomic_store(&s_online,true);atomic_store(&s_volume,50);
}
int main(void) {
    task_fail=1;assert(fmo_audio_start()==ESP_ERR_NO_MEM && !s_task);
    task_fail=0;assert(fmo_audio_start()==ESP_OK && s_task);
    assert(fmo_audio_start()==ESP_ERR_INVALID_STATE && task_creates==2);
    fmo_audio_set_online(true);fmo_audio_set_online(true);
    fmo_audio_set_volume(255);assert(atomic_load(&s_volume)==100 && notifications==2);
    reset_test(0);
    esp_websocket_client_handle_t client=NULL;
    init_fail=1;assert(start_stream(&client)==ESP_ERR_NO_MEM && !client && !destroys);
    register_fail=1;assert(start_stream(&client)==ESP_FAIL && !client && destroys==1);
    register_fail=0;start_fail=1;assert(start_stream(&client)==ESP_FAIL && !client && destroys==2);
    for(int scenario=1;scenario<=5;++scenario) {
        reset_test(scenario);
        if(scenario==2)init_fail=2;
        if(scenario==3)codec_fail=1;
        if(!setjmp(done))audio_task(NULL);
        assert(!locked);
        if(scenario==1)assert(wifi_awake_calls==2 && wifi_restore_calls==2);
        if(scenario==3)assert(!wifi_awake_calls);
        if(scenario==4)assert(wifi_awake_calls==1 && wifi_restore_calls==1);
    }
    // Bursting socket reads yield to the codec instead of discarding words.
    reset_test(6);callback=audio_event;starts=1;reset_stream(true);
    uint32_t burst_drops=s_dropped_samples, burst_bytes=s_received_bytes;
    for(unsigned i=0;i<64;++i)send_frame();
    assert(s_received_bytes-burst_bytes==64*5120);
    assert(s_dropped_samples==burst_drops && drained_samples>0 && s_buffer_wait_ms>0);
    assert(s_pcm.count+drained_samples==64*2560 && !s_pcm.pending_count);
    // Reserve a whole maximum-size message without pending-buffer deadlock.
    static uint8_t maximum[FMO_PCM_MESSAGE_BYTES];
    for(unsigned i=0;i<sizeof(maximum);i+=2){maximum[i]=0x34;maximum[i+1]=0x12;}
    esp_websocket_event_data_t maximum_frame={.payload_len=sizeof(maximum),.data_len=sizeof(maximum),
        .op_code=2,.fin=true,.data_ptr=(char *)maximum};
    audio_event(NULL,NULL,WEBSOCKET_EVENT_DATA,&maximum_frame);
    assert(s_dropped_samples==burst_drops && !s_pcm.pending_count);
    assert(s_pcm.count+drained_samples==64*2560+FMO_PCM_RATE);
    // PCM alone keeps a busy stream alive even when PONG is delayed/lost.
    for(unsigned i=0;i<120;++i){clock_ms+=1000;send_frame();assert(!stream_stalled());}
    reset_test(0);callback=audio_event;reset_stream(true);
    clock_ms+=FMO_AUDIO_ACTIVITY_TIMEOUT_MS-1;assert(!stream_stalled());
    ++clock_ms;assert(stream_stalled());
    esp_websocket_event_data_t pong={.op_code=10,.fin=true};
    audio_event(NULL,NULL,WEBSOCKET_EVENT_DATA,&pong);assert(!stream_stalled());
    clock_ms+=FMO_AUDIO_ACTIVITY_TIMEOUT_MS;assert(stream_stalled());
    reset_stream(false);assert(!stream_stalled());
    // The worker tears down a silent, unresponsive stream and re-primes fresh audio.
    reset_test(7);
    if(!setjmp(done))audio_task(NULL);
    assert(!locked);
    // Mute interrupts a full-buffer wait before accepting stale audio.
    reset_test(0);callback=audio_event;starts=1;reset_stream(true);
    for(unsigned i=0;i<4;++i)send_frame();
    uint32_t mute_bytes=s_received_bytes;mode=8;
    send_frame();assert(s_received_bytes==mute_bytes && !locked && s_pcm.count==4*2560);
    reset_test(0);
    s_level=75;s_level_ms=clock_ms;s_accepting=true;
    assert(fmo_audio_get_level()==75);
    fmo_audio_set_volume(0);assert(fmo_audio_get_level()==0);
    fmo_audio_set_volume(50);assert(fmo_audio_get_level()==0);
    s_level=75;
    fmo_audio_set_online(false);assert(fmo_audio_get_level()==0);
    fmo_audio_set_online(true);assert(fmo_audio_get_level()==0);
    s_level=75;
    reset_stream(true);
    esp_websocket_event_data_t bad={.payload_len=-1};
    s_level=75;
    audio_event(NULL,NULL,WEBSOCKET_EVENT_DATA,&bad);assert(!s_pcm.count && !fmo_audio_get_level());
    reset_stream(true);
    callback=audio_event;
    uint32_t bytes_before=s_received_bytes, drops_before=s_dropped_samples;
    for(unsigned i=0;i<5;++i)send_frame();
    assert(s_received_bytes-bytes_before==5*5120);
    assert(s_dropped_samples-drops_before==5*2560-FMO_PCM_CAPACITY);
    assert(s_invalid_messages>0);
    puts("FMO audio runtime: PASS (worker ownership, mute/offline flush, retry, format, meter freshness/reset, failure isolation)");
}
'''
with tempfile.TemporaryDirectory(prefix="fmo-audio-test-") as tmp:
    directory = Path(tmp)
    (directory / "audio_env.h").write_text(header)
    for name in ("esp_err.h", "esp_log.h", "esp_timer.h", "esp_wifi.h", "esp_websocket_client.h", "bsp_audio.h", "fmo_provision.h", "freertos/FreeRTOS.h", "freertos/task.h"):
        path = directory / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('#include "audio_env.h"\n')
    # Quoted includes resolve beside fmo_audio.c first; replace only its include
    # lines with the same stub header for the production hardware dependencies.
    source = (ROOT / "main/fmo_audio.c").read_text()
    source = source.replace('#include "fmo_provision.h"', '#include "audio_env.h"')
    (directory / "fmo_audio.c").write_text(source)
    (directory / "test.c").write_text(tests)
    binary = directory / "audio-test"
    subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-I"+str(directory), "-I"+str(ROOT / "main"), str(directory / "test.c"),
                    str(ROOT / "main/fmo_pcm.c"), str(ROOT / "main/fmo_audio_meter.c"),
                    "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True, timeout=15)
