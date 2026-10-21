"""Exercise the production worker with bounded HTTP/NVS and scheduler doubles."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
idf = os.environ.get("IDF_PATH")
if not idf:
    if os.environ.get("FMO_REQUIRE_NETWORK_TEST"):
        raise RuntimeError("Activate ESP-IDF for geocoder tests")
    print("FMO geocoder: SKIP (activate ESP-IDF; required by firmware gate)")
    raise SystemExit(0)
cjson = Path(idf) / "components/json/cJSON"
source = re.sub(r'^#include .*\n', '', (ROOT / 'main/fmo_geocoder.c').read_text(), flags=re.M)
preamble = r"""
#include "fmo_location.h"
#include "fmo_monitor_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <setjmp.h>
#include <time.h>
#define CONFIG_FMO_GEOCODER_URL "https://example.invalid/reverse"
#define ESP_OK 0
#define NVS_READONLY 0
#define NVS_READWRITE 1
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 2
#define pdMS_TO_TICKS(x) (x)
#define pdPASS 1
#define ESP_LOGW(tag,...) do { (void)(tag); if (0) printf(__VA_ARGS__); } while (0)
#define ESP_LOGI ESP_LOGW
typedef int esp_err_t;
typedef int nvs_handle_t;
typedef void *TaskHandle_t;
typedef void *esp_http_client_handle_t;
typedef struct {
 const char *url; int (*crt_bundle_attach)(void*); int timeout_ms,buffer_size,buffer_size_tx;
 const char *user_agent; bool disable_auto_redirect;
} esp_http_client_config_t;
static uint64_t clock_ms, open_times[32], applied_ms;
static int opens,cleanups,saves,reads,status,open_error,truncate_response,oversize,slow,change_speaker,slow_connect;
static size_t offset,heap,largest_block;
static bool buffered_complete;
static const char *body;
static char requested[256];
static fmo_monitor_state_t current;
static fmo_location_entry_t disk[32];
static bool disk_valid[32];
static int esp_crt_bundle_attach(void *x) {(void)x; return 0;}
static int64_t esp_timer_get_time(void) {return clock_ms*1000;}
static esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *c) {
 assert(c->crt_bundle_attach==esp_crt_bundle_attach && c->disable_auto_redirect);
 assert(c->timeout_ms==3000 && c->buffer_size<=1024);
 snprintf(requested,sizeof(requested),"%s",c->url);offset=0;return (void*)1;
}
static int esp_http_client_open(void *c,int n) {(void)c;(void)n;assert(opens<32);open_times[opens++]=clock_ms;if(slow_connect)clock_ms+=9000;return open_error;}
static int64_t esp_http_client_fetch_headers(void *c) {(void)c;return oversize?100000:0;}
static int esp_http_client_get_status_code(void *c) {(void)c;return status;}
static bool esp_http_client_is_complete_data_received(void *c) {(void)c;return buffered_complete || offset==strlen(body);}
static int esp_http_client_read(void *c,char *out,int n) {
 (void)c;++reads;if(truncate_response) return 0;
 size_t count=strlen(body)-offset;if(count>17)count=17;if(count>(size_t)n)count=n;
 memcpy(out,body+offset,count);offset+=count;
 if(slow)clock_ms+=9000;
 if(change_speaker) {strcpy(current.last_speaker,"NEW");strcpy(current.grid,"ON80");}
 return (int)count;
}
static void esp_http_client_cleanup(void *c) {(void)c;++cleanups;}
static int nvs_open(const char *s,int mode,int *h) {(void)s;(void)mode;*h=1;return 0;}
static int nvs_get_blob(int h,const char *k,void *out,size_t *size) {
 (void)h;unsigned slot=(unsigned)atoi(k+1);
 if(!disk_valid[slot])return -1;assert(*size==sizeof(disk[slot]));memcpy(out,&disk[slot],*size);return 0;
}
static int nvs_set_blob(int h,const char *k,const void *p,size_t size) {
 (void)h;unsigned slot=(unsigned)atoi(k+1);assert(size==sizeof(disk[slot]));
 memcpy(&disk[slot],p,size);disk_valid[slot]=true;++saves;return 0;
}
static int nvs_commit(int h) {(void)h;return 0;}
static void nvs_close(int h) {(void)h;}
static size_t heap_caps_get_free_size(int flags) {(void)flags;return heap;}
static size_t heap_caps_get_largest_free_block(int flags) {(void)flags;return largest_block;}
static bool fmo_network_location_snapshot(fmo_monitor_state_t *out) {*out=current;return !current.location[0];}
static void fmo_network_location_result(const fmo_monitor_state_t *request,const char *name) {
 if (fmo_monitor_apply_location(&current,request,name)) applied_ms=clock_ms;
}
static jmp_buf stopped;
static int cycles, cycle_limit;
static void (*cycle_hook)(void);
static void vTaskDelay(int n) {
 clock_ms+=n;if(++cycles>cycle_limit)longjmp(stopped,1);
 if(cycle_hook)cycle_hook();
}
static int xTaskCreate(void(*f)(void*),const char *name,int stack,void *arg,int priority,TaskHandle_t *task) {
 (void)f;(void)name;(void)arg;assert(stack>=4096 && priority<5);*task=(void*)1;return 1;
}
"""
tests = r"""
static void reset(void) {
 memset(&cache,0,sizeof(cache)); memset(&current,0,sizeof(current));
 memset(disk_valid,0,sizeof(disk_valid));
 clock_ms=applied_ms=0;memset(open_times,0,sizeof(open_times));
 opens=cleanups=saves=reads=offset=cycles=0;cycle_limit=4;cycle_hook=NULL;
 open_error=truncate_response=oversize=slow=change_speaker=slow_connect=0;status=200;heap=largest_block=100000;buffered_complete=false;
 body="{\"features\":[{\"properties\":{\"city\":\"佛山市\"}}]}";
 fmo_monitor_set_wifi(&current,true);fmo_monitor_set_events(&current,true);
 fmo_monitor_apply_speaker(&current,"TEST","OL63AA",true,false,false,0);
}
static void next_speaker(void) {
 if(cycles==2) {
  slow_connect=0;
  fmo_monitor_apply_speaker(&current,"NEXT","ON80",true,false,false,clock_ms);
 }
}
static void recover_connection(void) {if(cycles==2)open_error=0;}
static void uncached_target(void) {
 fmo_monitor_set_position(&current,true,(fmo_position_t){1000000+opens*1000,1000000});
}
static void run_worker(void) {if(!setjmp(stopped))geocoder_task(NULL);}
int main(void) {
 char name[49]; bool server_busy; fmo_position_t p={31230000,121470000};
 reset();assert(lookup(p,name,sizeof(name),&server_busy) && !strcmp(name,"佛山市"));
 assert(strstr(requested,"lat=31.230000&lon=121.470000&limit=1") && cleanups==1);
 reset();slow_connect=1;assert(lookup(p,name,sizeof(name),&server_busy) && !strcmp(name,"佛山市") && cleanups==1);
 reset();buffered_complete=true;assert(lookup(p,name,sizeof(name),&server_busy) && !strcmp(name,"佛山市"));
 reset();status=429;assert(!lookup(p,name,sizeof(name),&server_busy) && reads==0 && cleanups==1);
 reset();oversize=1;assert(!lookup(p,name,sizeof(name),&server_busy) && reads==0 && cleanups==1);
 reset();open_error=-1;assert(!lookup(p,name,sizeof(name),&server_busy) && cleanups==1);
 reset();truncate_response=1;assert(!lookup(p,name,sizeof(name),&server_busy) && cleanups==1);
 reset();slow=1;assert(!lookup(p,name,sizeof(name),&server_busy) && cleanups==1);
 reset();body="invalid";assert(!lookup(p,name,sizeof(name),&server_busy) && cleanups==1);
 reset();current.speaking=false;run_worker(); // Short voice already ended.
 assert(opens==1 && saves==1 && !strcmp(current.location,"佛山市"));
 memset(&cache,0,sizeof(cache));current.location[0]=0;cycles=0;heap=0;run_worker();
 assert(opens==1 && saves==1 && !strcmp(current.location,"佛山市")); // Persistent cache, no TLS heap.
 reset();change_speaker=1;run_worker();assert(saves==1 && !current.location[0]);
 reset();status=503;run_worker();assert(opens==1 && !saves && !current.location[0]);
 // Measured device idle heap used to be blocked forever by the 64 KiB gate.
 reset();heap=59724;largest_block=47104;run_worker();
 bool compact_tls=CONFIG_MBEDTLS_DYNAMIC_BUFFER && CONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN<=1024;
 assert(opens==(compact_tls?1:0) && saves==opens);
 reset();heap=56*1024;run_worker();assert(opens==(compact_tls?1:0));
 reset();heap=56*1024-1;run_worker();assert(!opens && !saves);
 reset();heap=64*1024;run_worker();assert(opens==1 && saves==1);
 reset();largest_block=32*1024-1;run_worker();assert(!opens && !saves);
 reset();heap=16000;run_worker();assert(!opens && !saves);
 // A new speaker can start after 3 seconds, without a post-response 15s wait.
 reset();cycle_hook=next_speaker;cycle_limit=20;run_worker();
 assert(opens==2 && open_times[1]-open_times[0]==3000 && saves==2);
 reset();slow_connect=1;cycle_hook=next_speaker;run_worker();
 assert(opens==2 && open_times[1]==9400 && applied_ms==9400);
 // A brief failure retries the same coordinate after 30 seconds, not 5 minutes.
 reset();open_error=-1;cycle_hook=recover_connection;cycle_limit=160;run_worker();
 assert(opens==2 && open_times[1]-open_times[0]==30000 && saves==1);
 // New targets cannot bypass the global exponential backoff during an outage.
 reset();open_error=-1;cycle_hook=uncached_target;cycle_limit=700;run_worker();
 assert(opens==6 && !saves);
 const uint64_t intervals[]={5000,10000,20000,40000,60000};
 for(unsigned i=1;i<6;++i)assert(open_times[i]-open_times[i-1]==intervals[i-1]);
 for(int busy=0;busy<2;++busy) {
  reset();status=busy?503:429;cycle_hook=uncached_target;cycle_limit=310;run_worker();
  assert(opens==2 && open_times[1]-open_times[0]==60000 && !saves);
 }
 // Cached speakers still display immediately during a server cooldown.
 reset();status=429;cycle_hook=next_speaker;
 fmo_position_t cached;assert(fmo_location_grid("ON80",&cached));
 disk[0]=(fmo_location_entry_t){.position=cached,.name="上海市"};disk_valid[0]=true;
 run_worker();assert(opens==1 && applied_ms==400 && !strcmp(current.location,"上海市"));
 fmo_geocoder_start();assert(worker);
 puts("FMO geocoder: PASS (TLS configuration, bounded responses/timeouts, cache reboot, short PTT, stale result, heap/backoff)");
}
"""
with tempfile.TemporaryDirectory(prefix="fmo-geocoder-test-") as tmp:
    src=Path(tmp)/"worker.c";src.write_text(preamble+source+tests)
    binary=Path(tmp)/"worker"
    for dynamic, tx in ((1, 1024), (0, 1024), (1, 4096)):
        subprocess.run([os.environ.get("CC","cc"),"-std=c11","-Wall","-Wextra","-Werror",
                        f"-DCONFIG_MBEDTLS_DYNAMIC_BUFFER={dynamic}",
                        f"-DCONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN={tx}",
                        "-I"+str(ROOT/"main"),"-I"+str(cjson),str(src),str(cjson/"cJSON.c"),
                        *[str(ROOT/"main"/f) for f in ("fmo_location.c","fmo_location_json.c","fmo_text.c","fmo_monitor_state.c")],
                        "-lm","-o",str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
