"""Exercise production provisioning with queued web requests and Wi-Fi events."""
import os
from pathlib import Path
import subprocess
import tempfile
from test_fmo_network import extract_function

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'main/fmo_provision.c').read_text().replace('esp_err_t fmo_provision_run(', 'static esp_err_t fmo_provision_run(')
functions = '\n'.join(extract_function(source, n) for n in ('restart_station', 'fmo_provision_run'))
preamble = r'''
#include "fmo_wifi_profiles.h"
#include "fmo_endpoint.h"
#include <assert.h>
#include <stdatomic.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#define ESP_OK 0
#define ESP_ERR_NO_MEM -1
#define ESP_ERR_TIMEOUT -2
#define BIT0 1
#define BIT1 2
#define BIT2 4
#define BIT3 8
#define FMO_WIFI_READY_BIT BIT0
#define FMO_WIFI_DISCONNECTED_BIT BIT1
#define FMO_WIFI_STOPPED_BIT BIT2
#define FMO_WIFI_CANCEL_BIT BIT3
#define pdTRUE 1
#define pdFALSE 0
#define portMAX_DELAY 99999
#define pdMS_TO_TICKS(x) (x)
#define WIFI_IF_STA 0
#define WIFI_IF_AP 1
#define WIFI_MODE_STA 0
#define WIFI_MODE_APSTA 1
#define WIFI_AUTH_OPEN 0
#define WIFI_AUTH_WPA2_PSK 1
#define HTTP_GET 0
#define HTTP_POST 1
#define HTTPD_DEFAULT_CONFIG() ((httpd_config_t){0})
typedef int esp_err_t;
typedef int EventGroupHandle_t;
typedef unsigned EventBits_t;
typedef int esp_netif_t;
typedef void *httpd_handle_t;
typedef void (*fmo_setup_display_t)(const char *, const char *);
typedef fmo_wifi_credential_t credentials_t;
typedef struct {credentials_t value; bool remove, finish, endpoint, probe; fmo_endpoint_t destination;} setup_request_t;
typedef union {
    struct {unsigned char ssid[32], password[64]; struct {int authmode;} threshold;} sta;
    struct {unsigned char ssid[32], password[64]; int authmode,max_connection,channel;} ap;
} wifi_config_t;
typedef struct {unsigned char ssid[32];} wifi_ap_record_t;
typedef struct {int unused;} wifi_scan_config_t;
typedef struct {struct {unsigned addr;} ip;} esp_netif_ip_info_t;
typedef struct {int max_open_sockets,lru_purge_enable,stack_size,recv_wait_timeout,send_wait_timeout;} httpd_config_t;
typedef struct {const char *uri; int method; int (*handler)(void);} httpd_uri_t;
static int root_get(void){return 0;} static int networks_get(void){return 0;}
static int status_get(void){return 0;} static int saved_get(void){return 0;}
static int configure_post(void){return 0;}
static int endpoint_get(void){return 0;} static int endpoint_post(void){return 0;}
static int endpoint_saves,probes;
static int save_endpoint(const fmo_endpoint_t *e){assert(!strcmp(e->host,"fmo.local"));++endpoint_saves;return 0;}
static bool probe_endpoint(const fmo_endpoint_t *e);
static int s_profiles_lock=1;
static void *s_pending;
static fmo_wifi_profiles_t s_profiles;
static atomic_int s_status,s_attempt_error;
static char s_token[33];
static unsigned s_ap_address;
static uint16_t s_record_count;
static wifi_ap_record_t s_records[16];
static unsigned bits;
static int scenario, step, stop_count, http_stopped, saves, connects, forced=1;
static wifi_config_t selected;
static bool probe_endpoint(const fmo_endpoint_t *e){assert(!strcmp(e->host,"fmo.local"));++probes;return scenario!=7;}
static unsigned xEventGroupGetBits(int b){(void)b;return bits;}
static void xEventGroupClearBits(int b,unsigned mask){(void)b;bits&=~mask;}
static unsigned xEventGroupWaitBits(int b,unsigned mask,int clear,int all,int timeout){
    (void)b;(void)all;(void)timeout;
    if(mask & FMO_WIFI_READY_BIT) {
        if(scenario==1 || scenario==2 || scenario==9) bits|=FMO_WIFI_CANCEL_BIT;
        else bits|=FMO_WIFI_READY_BIT;
    }
    unsigned result=bits;
    if(clear)bits&=~mask;
    return result;
}
static int esp_wifi_stop(void){++stop_count;bits=FMO_WIFI_STOPPED_BIT|(bits&FMO_WIFI_CANCEL_BIT);return 0;}
static int esp_wifi_start(void){return 0;}
static int esp_wifi_set_mode(int m){return scenario==10 && m==WIFI_MODE_APSTA ? -3 : 0;}
static int esp_wifi_set_config(int i,const wifi_config_t *c){if(i==WIFI_IF_STA)selected=*c;return 0;}
static int esp_wifi_connect(void){++connects;return 0;}
static int esp_wifi_disconnect(void){bits&=~FMO_WIFI_READY_BIT;return 0;}
static int esp_wifi_get_mac(int i,uint8_t *m){(void)i;memset(m,0,6);return 0;}
static int esp_wifi_sta_get_ap_info(wifi_ap_record_t *a){memcpy(a->ssid,selected.sta.ssid,32);return 0;}
static int esp_wifi_scan_start(void *s,bool sync){(void)s;(void)sync;return 0;}
static int esp_wifi_scan_get_ap_records(uint16_t *n,void *r){(void)r;*n=0;return 0;}
static void esp_wifi_clear_ap_list(void){}
static uint32_t esp_random(void){return 1234;}
static esp_netif_t *esp_netif_create_default_wifi_ap(void){static int ap;return &ap;}
static void esp_netif_destroy_default_wifi(esp_netif_t *ap){(void)ap;}
static int setup_ap_address(esp_netif_t *ap){(void)ap;return 0;}
static int esp_netif_get_ip_info(esp_netif_t *a,esp_netif_ip_info_t *i){(void)a;i->ip.addr=1;return 0;}
static int httpd_start(httpd_handle_t *s,const httpd_config_t *c){(void)c;if(scenario==11)return -3;*s=(void *)1;return 0;}
static int httpd_register_uri_handler(httpd_handle_t s,const httpd_uri_t *u){(void)s;(void)u;return scenario==12?-3:0;}
static void httpd_stop(httpd_handle_t s){assert(s);http_stopped=1;}
static void *xQueueCreate(int n,size_t size){(void)n;(void)size;return scenario==13?NULL:(void *)1;}
static void vQueueDelete(void *q){assert(q && (http_stopped || scenario==10 || scenario==11));}
static int xQueueReceive(void *q,setup_request_t *r,int wait){
    assert(q && wait==500); memset(r,0,sizeof(*r));
    if(scenario>=5){
        if(step++==0){
            r->endpoint=true;r->probe=scenario!=5;strcpy(r->destination.host,"fmo.local");r->destination.port=80;return 1;
        }
        assert(atomic_load(&s_status)==(scenario==5?11:scenario==6?13:scenario==7?14:12));
        bits|=FMO_WIFI_CANCEL_BIT;return 0;
    }
    if(scenario==2 && step++==0){r->remove=true;strcpy(r->value.ssid,"old");return 1;}
    if(scenario==2){bits|=FMO_WIFI_CANCEL_BIT;return 0;}
    if(scenario==4){r->finish=true;return 1;}
    strcpy(r->value.ssid,"new");strcpy(r->value.password,"new-password");return 1;
}
static void xSemaphoreTake(int m,int timeout){assert(m);(void)timeout;}
static void xSemaphoreGive(int m){assert(m);}
static int save_profiles(const fmo_wifi_profiles_t *p,bool finish){
    s_profiles=*p;++saves;if(finish)forced=0;return 0;
}
static void to_wifi_config(wifi_config_t *c,const credentials_t *v){
    memset(c,0,sizeof(*c));memcpy(c->sta.ssid,v->ssid,strlen(v->ssid));
    memcpy(c->sta.password,v->password,strlen(v->password));
}
static void display(const char *ssid,const char *password){assert(*ssid && *password);if(!scenario)bits|=FMO_WIFI_CANCEL_BIT;}
'''
tests = r'''
int main(void){
    for(scenario=0;scenario<5;++scenario){
        fmo_wifi_profiles_init(&s_profiles);
        credentials_t old={.ssid="old",.password="old-password"};
        assert(fmo_wifi_profiles_put(&s_profiles,&old));
        bits=0;step=0;stop_count=0;http_stopped=0;saves=0;connects=0;forced=1;
        memset(&selected,0,sizeof(selected));
        assert(fmo_provision_run(1,FMO_WIFI_READY_BIT,display)==ESP_OK);
        assert(http_stopped && !s_pending && !forced);
        assert(!(bits&FMO_WIFI_CANCEL_BIT));
        if(scenario<=2){
            assert(stop_count==1);
            assert(fmo_wifi_profiles_find(&s_profiles,"new")==-1);
            if(scenario==2){assert(!s_profiles.count && !selected.sta.ssid[0] && saves==2);}
            else {assert(s_profiles.count==1 && !strcmp((char *)selected.sta.ssid,"old") && saves==1);}
        } else if(scenario==3){
            assert(s_profiles.count==2 && connects==1 && saves==1);
            assert(!strcmp((char *)selected.sta.ssid,"new"));
        } else {assert(s_profiles.count==1 && !connects && saves==1);}
    }
    for(scenario=5;scenario<=8;++scenario){
        fmo_wifi_profiles_init(&s_profiles);
        credentials_t old={.ssid="old",.password="old-password"};
        if(scenario!=8)assert(fmo_wifi_profiles_put(&s_profiles,&old));
        bits=0;step=stop_count=http_stopped=saves=connects=endpoint_saves=probes=0;forced=1;
        assert(fmo_provision_run(1,FMO_WIFI_READY_BIT,display)==0);
        assert(endpoint_saves==(scenario==5) && probes==((scenario==6)||(scenario==7)));
        assert(connects==((scenario==6)||(scenario==7)));
        assert(s_profiles.count==(scenario!=8));
    }
    scenario=9;fmo_wifi_profiles_init(&s_profiles);
    credentials_t old={.ssid="old",.password="old-password"};
    assert(fmo_wifi_profiles_put(&s_profiles,&old));
    bits=step=stop_count=http_stopped=saves=connects=endpoint_saves=probes=0;forced=1;
    assert(fmo_provision_run(1,FMO_WIFI_READY_BIT,display)==0 && !probes && !endpoint_saves);
    assert(s_profiles.count==1 && !forced);
    for(scenario=10;scenario<=13;++scenario){
        bits=step=stop_count=http_stopped=0;
        assert(fmo_provision_run(1,FMO_WIFI_READY_BIT,display)!=0 && !s_pending);
        assert(http_stopped==(scenario==12));
    }
    puts("FMO provisioning runtime: PASS (cancel idle/connecting/deleted list, verified save, finish)");
}
'''
with tempfile.TemporaryDirectory(prefix='fmo-provision-test-') as tmp:
    file=Path(tmp)/'provision.c';file.write_text(preamble+functions+tests)
    binary=Path(tmp)/'provision'
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'main'),str(file),str(ROOT/'main/fmo_wifi_profiles.c'),str(ROOT/'main/fmo_credentials.c'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
