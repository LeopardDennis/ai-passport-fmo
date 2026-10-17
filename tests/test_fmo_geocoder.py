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
#define ESP_LOGW(tag,...) ((void)(tag))
typedef int esp_err_t;
typedef int nvs_handle_t;
typedef void *TaskHandle_t;
typedef void *esp_http_client_handle_t;
typedef struct {
 const char *url; int (*crt_bundle_attach)(void*); int timeout_ms,buffer_size,buffer_size_tx;
 const char *user_agent; bool disable_auto_redirect;
} esp_http_client_config_t;
static uint64_t clock_ms;
static int opens,cleanups,saves,reads,status,open_error,truncate_response,oversize,slow,change_speaker;
static size_t offset,heap;
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
static int esp_http_client_open(void *c,int n) {(void)c;(void)n;++opens;return open_error;}
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
static size_t heap_caps_get_largest_free_block(int flags) {(void)flags;return heap;}
static bool fmo_network_location_snapshot(fmo_monitor_state_t *out) {*out=current;return !current.location[0];}
static void fmo_network_location_result(const fmo_monitor_state_t *request,const char *name) {
 fmo_monitor_apply_location(&current,request,name);
}
static jmp_buf stopped;
static int cycles;
static void vTaskDelay(int n) {clock_ms+=n;if(++cycles>4)longjmp(stopped,1);}
static int xTaskCreate(void(*f)(void*),const char *name,int stack,void *arg,int priority,TaskHandle_t *task) {
 (void)f;(void)name;(void)arg;assert(stack>=4096 && priority<5);*task=(void*)1;return 1;
}
"""
tests = r"""
static void reset(void) {
 memset(&cache,0,sizeof(cache)); memset(&current,0,sizeof(current));
 memset(disk_valid,0,sizeof(disk_valid));
 clock_ms=0;opens=cleanups=saves=reads=offset=cycles=0;
 open_error=truncate_response=oversize=slow=change_speaker=0;status=200;heap=100000;buffered_complete=false;
 body="{\"features\":[{\"properties\":{\"city\":\"佛山市\"}}]}";
 fmo_monitor_set_wifi(&current,true);fmo_monitor_set_events(&current,true);
 fmo_monitor_apply_speaker(&current,"TEST","OL63AA",true,false,false,0);
}
static void run_worker(void) {if(!setjmp(stopped))geocoder_task(NULL);}
int main(void) {
 char name[49]; fmo_position_t p={31230000,121470000};
 reset();assert(lookup(p,name,sizeof(name)) && !strcmp(name,"佛山市"));
 assert(strstr(requested,"lat=31.230000&lon=121.470000&limit=1") && cleanups==1);
 reset();buffered_complete=true;assert(lookup(p,name,sizeof(name)) && !strcmp(name,"佛山市"));
 reset();status=429;assert(!lookup(p,name,sizeof(name)) && reads==0 && cleanups==1);
 reset();oversize=1;assert(!lookup(p,name,sizeof(name)) && reads==0 && cleanups==1);
 reset();open_error=-1;assert(!lookup(p,name,sizeof(name)) && cleanups==1);
 reset();truncate_response=1;assert(!lookup(p,name,sizeof(name)) && cleanups==1);
 reset();slow=1;assert(!lookup(p,name,sizeof(name)) && cleanups==1);
 reset();body="invalid";assert(!lookup(p,name,sizeof(name)) && cleanups==1);
 reset();current.speaking=false;run_worker(); // Short voice already ended.
 assert(opens==1 && saves==1 && !strcmp(current.location,"佛山市"));
 memset(&cache,0,sizeof(cache));current.location[0]=0;cycles=0;heap=0;run_worker();
 assert(opens==1 && saves==1 && !strcmp(current.location,"佛山市")); // Persistent cache, no TLS heap.
 reset();change_speaker=1;run_worker();assert(saves==1 && !current.location[0]);
 reset();status=503;run_worker();assert(opens==1 && !saves && !current.location[0]);
 reset();heap=16000;run_worker();assert(!opens && !saves);
 fmo_geocoder_start();assert(worker);
 puts("FMO geocoder: PASS (TLS configuration, bounded responses/timeouts, cache reboot, short PTT, stale result, heap/backoff)");
}
"""
with tempfile.TemporaryDirectory(prefix="fmo-geocoder-test-") as tmp:
    src=Path(tmp)/"worker.c";src.write_text(preamble+source+tests)
    binary=Path(tmp)/"worker"
    subprocess.run([os.environ.get("CC","cc"),"-std=c11","-Wall","-Wextra","-Werror",
                    "-I"+str(ROOT/"main"),"-I"+str(cjson),str(src),str(cjson/"cJSON.c"),
                    *[str(ROOT/"main"/f) for f in ("fmo_location.c","fmo_location_json.c","fmo_text.c","fmo_monitor_state.c")],
                    "-lm","-o",str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
