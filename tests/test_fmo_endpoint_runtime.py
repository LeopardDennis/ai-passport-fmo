"""Exercise real endpoint HTTP handlers, persistence and TCP probe outcomes."""
import os
from pathlib import Path
import subprocess
import tempfile
from test_fmo_network import extract_function

ROOT=Path(__file__).resolve().parents[1]

def run():
    idf=os.environ.get('IDF_PATH')
    if not idf:
        if os.environ.get('FMO_REQUIRE_NETWORK_TEST'):
            raise RuntimeError('IDF_PATH is required for endpoint HTTP tests')
        print('FMO endpoint HTTP: SKIP (activate ESP-IDF for real cJSON)')
        return
    cjson=Path(idf)/'components/json/cJSON'
    source=(ROOT/'main/fmo_provision.c').read_text().replace('void fmo_provision_get_endpoint(', 'static void fmo_provision_get_endpoint(').replace('bool fmo_provision_load(', 'static bool fmo_provision_load(').replace('void fmo_provision_remember_connected(', 'static void fmo_provision_remember_connected(')
    functions='\n'.join(extract_function(source,name) for name in
                        ('save_profiles','to_wifi_config','default_endpoint','endpoint_for_wifi','fmo_provision_get_endpoint','legacy_endpoint','save_endpoint','probe_endpoint','parse_endpoint','configure_post','endpoint_get','endpoint_post','fmo_provision_load','fmo_provision_remember_connected'))
    preamble=r'''
#include "fmo_endpoint.h"
#include "fmo_credentials.h"
#include "fmo_wifi_profiles.h"
#include "fmo_text.h"
#include "cJSON.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <arpa/inet.h>
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG -2
#define ESP_ERR_NVS_NOT_FOUND -3
#define ESP_ERR_NO_MEM -4
#define WIFI_AUTH_WPA2_PSK 2
#define WIFI_AUTH_OPEN 0
#define NVS_READONLY 0
#define NVS_READWRITE 1
#define CONFIG_FMO_HOST "fmo.local"
#define CONFIG_FMO_PORT 80
#define HTTPD_403_FORBIDDEN 403
#define HTTPD_400_BAD_REQUEST 400
#define portMAX_DELAY 0
#define pdTRUE 1
#define HTTPD_RESP_USE_STRLEN -1
typedef int esp_err_t;
typedef int nvs_handle_t;
typedef fmo_wifi_credential_t credentials_t;
typedef struct {credentials_t value;bool remove,finish,endpoint,probe;fmo_endpoint_t destination;} setup_request_t;
typedef struct {size_t content_len;const char *body;size_t read;bool local;const char *token;int code;} httpd_req_t;
typedef struct {struct {unsigned char ssid[32],password[64];struct {int authmode;} threshold;} sta;} wifi_config_t;
typedef struct {unsigned char ssid[33];} wifi_ap_record_t;
static char joined[33]="Home";
static int esp_wifi_sta_get_ap_info(wifi_ap_record_t *ap){strcpy((char *)ap->ssid,joined);return joined[0]?0:-1;}
static fmo_wifi_profiles_t s_profiles,stored_profiles,staged_profiles;
static fmo_wifi_profiles_v1_t old_profiles;
static fmo_wifi_credential_t old_credential;
static fmo_endpoint_t old_endpoint;
static int s_profiles_lock=1,s_pending,commit_error,has_stored,queue_full,has_v1,has_credential,has_endpoint;
static uint8_t setup_flag=1,staged_flag=1;
static atomic_int s_status;
static char s_token[33]="test-token",response[2000];
static setup_request_t queued;
static int slow_request, timer_calls;
static int64_t esp_timer_get_time(void){return slow_request && timer_calls++ ? 20000000 : 0;}
static int xSemaphoreCreateMutex(void){return 1;}
static int nvs_open(const char *ns,int mode,int *handle){(void)mode;assert(!strcmp(ns,"fmo_wifi")||!strcmp(ns,"fmo_net"));*handle=!strcmp(ns,"fmo_wifi")?1:2;return 0;}
static int nvs_get_blob(int h,const char *key,void *out,size_t *size){
    const void *data;size_t length;int exists;
    if(h==2){assert(!strcmp(key,"endpoint_v1"));data=&old_endpoint;length=sizeof(old_endpoint);exists=has_endpoint;}
    else if(!strcmp(key,"profiles_v2")){data=&stored_profiles;length=sizeof(stored_profiles);exists=has_stored;}
    else if(!strcmp(key,"profiles_v1")){data=&old_profiles;length=sizeof(old_profiles);exists=has_v1;}
    else {assert(!strcmp(key,"credentials"));data=&old_credential;length=sizeof(old_credential);exists=has_credential;}
    if(!exists)return ESP_ERR_NVS_NOT_FOUND;
    assert(*size==length);memcpy(out,data,length);*size=length;return 0;
}
static int nvs_get_u8(int h,const char *key,uint8_t *out){assert(h==1&&!strcmp(key,"setup"));*out=setup_flag;return 0;}
static int nvs_set_u8(int h,const char *key,uint8_t value){assert(h==1&&!strcmp(key,"setup"));staged_flag=value;return 0;}
static int nvs_set_blob(int h,const char *key,const void *in,size_t size){assert(h==1&&!strcmp(key,"profiles_v2")&&size==sizeof(staged_profiles));memcpy(&staged_profiles,in,size);return 0;}
static int nvs_commit(int h){assert(h==1);if(commit_error)return -1;stored_profiles=staged_profiles;setup_flag=staged_flag;has_stored=1;return 0;}
static void nvs_close(int h){assert(h==1||h==2);}
static void xSemaphoreTake(int h,int timeout){(void)h;(void)timeout;}
static void xSemaphoreGive(int h){(void)h;}
static bool local_request(httpd_req_t *req){return req->local;}
static int httpd_resp_send_err(httpd_req_t *req,int code,const char *why){(void)why;req->code=code;return -1;}
static int httpd_req_get_hdr_value_str(httpd_req_t *req,const char *key,char *out,size_t cap){assert(!strcmp(key,"X-Setup-Token"));if(!req->token)return -1;snprintf(out,cap,"%s",req->token);return 0;}
static int httpd_req_recv(httpd_req_t *req,char *out,size_t length){size_t n=length>7?7:length;memcpy(out,req->body+req->read,n);req->read+=n;return (int)n;}
static int httpd_resp_set_type(httpd_req_t *req,const char *type){(void)req;(void)type;return 0;}
static int httpd_resp_set_hdr(httpd_req_t *req,const char *key,const char *value){(void)req;(void)key;(void)value;return 0;}
static int httpd_resp_sendstr(httpd_req_t *req,const char *text){(void)req;snprintf(response,sizeof(response),"%s",text);return 0;}
static int xQueueSend(int q,const setup_request_t *r,int wait){(void)q;(void)wait;if(queue_full)return 0;queued=*r;return 1;}

static int dns_error,socket_error,flag_error,connect_result,select_result=1,tcp_error,closed,freed;
static int fake_getaddrinfo(const char *host,const char *port,const struct addrinfo *hints,struct addrinfo **out){
    assert(*host&&*port&&hints->ai_family==AF_INET);if(dns_error)return -1;
    static struct sockaddr_in address;static struct addrinfo info;
    info=(struct addrinfo){.ai_family=AF_INET,.ai_socktype=SOCK_STREAM,.ai_addr=(struct sockaddr *)&address,.ai_addrlen=sizeof(address)};
    *out=&info;return 0;
}
static void fake_freeaddrinfo(struct addrinfo *a){assert(a);++freed;}
static int fake_socket(int family,int type,int protocol){assert(family==AF_INET&&type==SOCK_STREAM);(void)protocol;return socket_error?-1:3;}
static int fake_fcntl(int fd,int command,...){assert(fd==3);(void)command;return flag_error?-1:0;}
static int fake_connect(int fd,const struct sockaddr *address,socklen_t size){assert(fd==3&&address&&size);errno=EINPROGRESS;return connect_result;}
static int fake_select(int count,fd_set *r,fd_set *w,fd_set *e,struct timeval *t){assert(count==4&&!r&&w&&!e&&t->tv_sec==1);return select_result;}
static int fake_getsockopt(int fd,int level,int name,void *out,socklen_t *size){assert(fd==3&&level==SOL_SOCKET&&name==SO_ERROR&&*size==sizeof(int));*(int *)out=tcp_error;return 0;}
static int fake_close(int fd){assert(fd==3);++closed;return 0;}
#define getaddrinfo fake_getaddrinfo
#define freeaddrinfo fake_freeaddrinfo
#define socket fake_socket
#define fcntl fake_fcntl
#define connect fake_connect
#define select fake_select
#define getsockopt fake_getsockopt
#define close fake_close
'''
    tests=r'''
static httpd_req_t request(const char *body){return (httpd_req_t){.content_len=strlen(body),.body=body,.local=true,.token="test-token"};}
int main(void){
    wifi_config_t config;bool force;
    assert(!fmo_provision_load(&config,&force)&&force);
    fmo_endpoint_t loaded,custom={.host="192.168.1.2",.port=8080};
    fmo_provision_get_endpoint(&loaded);assert(!strcmp(loaded.host,"fmo.local")&&loaded.port==80);
    // Migrate V1 networks and global destination; migration failure is retryable.
    old_profiles=(fmo_wifi_profiles_v1_t){.version=1,.count=2,.preferred=0,
        .entries={{.ssid="Home",.password="test-home"},{.ssid="Office",.password="test-office"}}};
    old_endpoint=custom;has_v1=has_endpoint=1;commit_error=1;
    assert(fmo_provision_load(&config,&force)&&!has_stored);
    assert(!strcmp((char *)config.sta.ssid,"Home"));
    commit_error=0;assert(fmo_provision_load(&config,&force)&&has_stored);
    assert(s_profiles.endpoints[0].port==8080 && s_profiles.endpoints[1].port==8080);
    fmo_endpoint_t office={.host="office.local",.port=9090};
    commit_error=1;assert(save_endpoint("Office",&office)!=0);
    assert(s_profiles.endpoints[1].port==8080);
    commit_error=0;assert(save_endpoint("Office",&office)==0);
    assert(save_endpoint("missing",&office)!=0);
    memset(&s_profiles,0,sizeof(s_profiles));assert(fmo_provision_load(&config,&force));
    fmo_provision_get_endpoint(&loaded);assert(loaded.port==8080&&!strcmp(loaded.host,custom.host));
    strcpy(joined,"Office");fmo_provision_get_endpoint(&loaded);assert(loaded.port==9090&&!strcmp(loaded.host,"office.local"));
    commit_error=1;fmo_provision_remember_connected();assert(s_profiles.preferred==0);
    fmo_provision_get_endpoint(&loaded);assert(loaded.port==9090); // actual AP, not last saved preference
    commit_error=0;fmo_provision_remember_connected();assert(s_profiles.preferred==1);
    strcpy(joined,"Unknown");fmo_provision_get_endpoint(&loaded);assert(loaded.port==80&&!strcmp(loaded.host,"fmo.local"));
    strcpy(joined,"Home");
    httpd_req_t req=request("{\"ssid\":\"Home\",\"host\":\"fmo.local\",\"port\":80,\"probe\":true}");
    assert(endpoint_post(&req)==0&&queued.probe&&queued.endpoint&&queued.destination.port==80);
    assert(atomic_load(&s_status)==1);
    assert(!strcmp(queued.value.ssid,"Home"));
    atomic_store(&s_status,0);
    req=request("{\"ssid\":\"New\",\"password\":\"test-new\",\"host\":\"new.local\",\"port\":1234}");
    assert(configure_post(&req)==0 && queued.destination.port==1234 && !queued.endpoint);
    assert(!strcmp(queued.destination.host,"new.local")&&!strcmp(queued.value.ssid,"New"));
    atomic_store(&s_status,0);
    req=request("{\"ssid\":\"New\",\"password\":\"test-new\"}");
    assert(configure_post(&req)!=0 && req.code==400); // cannot silently discard the FMO field

    const char *bad[]={"{\"ssid\":\"Home\",\"host\":\"ws://fmo.local\",\"port\":80}","{\"ssid\":\"Home\",\"host\":\"fmo.local\",\"port\":80.1}",
        "{\"ssid\":\"Home\",\"host\":\"fmo.local\",\"port\":65536}","{\"ssid\":\"Home\",\"host\":\"fmo.local\\u0000/other\",\"port\":80}","{}","{garbage"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);++i){atomic_store(&s_status,0);req=request(bad[i]);assert(endpoint_post(&req)!=0&&req.code==400);assert(!atomic_load(&s_status));}
    req=request("{\"ssid\":\"Home\",\"host\":\"fmo.local\",\"port\":80}");req.local=false;assert(endpoint_post(&req)!=0&&req.code==403);
    req.local=true;req.token="wrong";assert(endpoint_post(&req)!=0&&req.code==403);
    req=request("{\"ssid\":\"Home\",\"host\":\"fmo.local\",\"port\":80}");atomic_store(&s_status,1);assert(endpoint_post(&req)!=0&&req.code==400);
    atomic_store(&s_status,0);req.read=0;queue_full=1;assert(endpoint_post(&req)!=0&&!atomic_load(&s_status));queue_full=0;
    req=request("{\"ssid\":\"Home\",\"host\":\"fmo.local\",\"port\":80}");slow_request=1;timer_calls=0;
    assert(endpoint_post(&req)!=0&&!req.read);slow_request=0;
    req=request("");assert(endpoint_get(&req)==0&&strstr(response,"192.168.1.2"));req.local=false;assert(endpoint_get(&req)!=0&&req.code==403);
    fmo_endpoint_t local={.host="127.0.0.1",.port=80};
    assert(probe_endpoint(&local));assert(closed==1&&freed==1);
    connect_result=-1;assert(probe_endpoint(&local));assert(closed==2&&freed==2);
    select_result=0;assert(!probe_endpoint(&local));assert(closed==3&&freed==3);
    select_result=1;tcp_error=ECONNREFUSED;assert(!probe_endpoint(&local));assert(closed==4&&freed==4);
    tcp_error=0;flag_error=1;assert(!probe_endpoint(&local));assert(closed==5&&freed==5);
    flag_error=0;socket_error=1;assert(!probe_endpoint(&local));assert(closed==5&&freed==6);
    socket_error=0;dns_error=1;assert(!probe_endpoint(&local));assert(closed==5&&freed==6);
    // A new-format empty list takes precedence over all retained legacy blobs.
    fmo_wifi_profiles_init(&s_profiles);assert(save_profiles(&s_profiles,true)==0);
    assert(!fmo_provision_load(&config,&force)&&!force);
    assert(!s_profiles.count);
    // Corrupt V2 cannot resurrect deleted V1 networks.
    stored_profiles.version=99;assert(!fmo_provision_load(&config,&force));
    // Single-network migration also retains its former global destination.
    has_stored=has_v1=0;has_credential=1;old_credential=(credentials_t){.ssid="Old",.password="test-only"};
    assert(fmo_provision_load(&config,&force)&&s_profiles.count==1&&s_profiles.endpoints[0].port==8080);
    puts("FMO endpoint HTTP: PASS (token/AP restrictions, validation, commit failures, TCP reachability)");
}
'''
    with tempfile.TemporaryDirectory(prefix='fmo-endpoint-test-') as tmp:
        file=Path(tmp)/'endpoint.c';file.write_text(preamble+functions+tests)
        binary=Path(tmp)/'endpoint'
        subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',
            '-I'+str(ROOT/'main'),'-I'+str(cjson),str(file),str(cjson/'cJSON.c'),
            str(ROOT/'main/fmo_text.c'),str(ROOT/'main/fmo_endpoint.c'),str(ROOT/'main/fmo_wifi_profiles.c'),str(ROOT/'main/fmo_credentials.c'),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)

if __name__=='__main__':run()
