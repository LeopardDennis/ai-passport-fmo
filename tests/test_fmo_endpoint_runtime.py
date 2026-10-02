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
    source=(ROOT/'main/fmo_provision.c').read_text().replace('void fmo_provision_get_endpoint(', 'static void fmo_provision_get_endpoint(')
    functions='\n'.join(extract_function(source,name) for name in
                        ('fmo_provision_get_endpoint','save_endpoint','probe_endpoint','endpoint_get','endpoint_post'))
    preamble=r'''
#include "fmo_endpoint.h"
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
static fmo_endpoint_t s_endpoint,stored,staged;
static int s_profiles_lock,s_pending,commit_error,has_stored,queue_full;
static atomic_int s_status;
static char s_token[33]="test-token",response[200];
static setup_request_t queued;
static int slow_request, timer_calls;
static int64_t esp_timer_get_time(void){return slow_request && timer_calls++ ? 20000000 : 0;}
static int nvs_open(const char *ns,int mode,int *handle){assert(!strcmp(ns,"fmo_net"));(void)mode;*handle=1;return 0;}
static int nvs_get_blob(int h,const char *key,void *out,size_t *size){assert(h==1&&!strcmp(key,"endpoint_v1"));if(!has_stored)return -1;assert(*size==sizeof(stored));memcpy(out,&stored,*size);return 0;}
static int nvs_set_blob(int h,const char *key,const void *in,size_t size){assert(h==1&&!strcmp(key,"endpoint_v1")&&size==sizeof(staged));memcpy(&staged,in,size);return 0;}
static int nvs_commit(int h){assert(h==1);if(commit_error)return -1;stored=staged;has_stored=1;return 0;}
static void nvs_close(int h){assert(h==1);}
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
    fmo_provision_get_endpoint(&s_endpoint);assert(!strcmp(s_endpoint.host,"fmo.local")&&s_endpoint.port==80);
    fmo_endpoint_t custom={.host="192.168.1.2",.port=8080};
    commit_error=1;assert(save_endpoint(&custom)!=0);assert(!strcmp(s_endpoint.host,"fmo.local"));
    commit_error=0;assert(save_endpoint(&custom)==0);
    fmo_endpoint_t loaded;fmo_provision_get_endpoint(&loaded);assert(!strcmp(loaded.host,custom.host)&&loaded.port==8080);
    stored.port=0;fmo_provision_get_endpoint(&loaded);assert(!strcmp(loaded.host,"fmo.local"));
    httpd_req_t req=request("{\"host\":\"fmo.local\",\"port\":80,\"probe\":true}");
    assert(endpoint_post(&req)==0&&queued.probe&&queued.endpoint&&queued.destination.port==80);
    assert(atomic_load(&s_status)==1);
    const char *bad[]={"{\"host\":\"ws://fmo.local\",\"port\":80}","{\"host\":\"fmo.local\",\"port\":80.1}",
        "{\"host\":\"fmo.local\",\"port\":65536}","{\"host\":\"fmo.local\\u0000/other\",\"port\":80}","{}","{garbage"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);++i){atomic_store(&s_status,0);req=request(bad[i]);assert(endpoint_post(&req)!=0&&req.code==400);assert(!atomic_load(&s_status));}
    req=request("{\"host\":\"fmo.local\",\"port\":80}");req.local=false;assert(endpoint_post(&req)!=0&&req.code==403);
    req.local=true;req.token="wrong";assert(endpoint_post(&req)!=0&&req.code==403);
    req=request("{\"host\":\"fmo.local\",\"port\":80}");atomic_store(&s_status,1);assert(endpoint_post(&req)!=0&&req.code==400);
    atomic_store(&s_status,0);req.read=0;queue_full=1;assert(endpoint_post(&req)!=0&&!atomic_load(&s_status));queue_full=0;
    req=request("{\"host\":\"fmo.local\",\"port\":80}");slow_request=1;timer_calls=0;
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
    puts("FMO endpoint HTTP: PASS (token/AP restrictions, validation, commit failures, TCP reachability)");
}
'''
    with tempfile.TemporaryDirectory(prefix='fmo-endpoint-test-') as tmp:
        file=Path(tmp)/'endpoint.c';file.write_text(preamble+functions+tests)
        binary=Path(tmp)/'endpoint'
        subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',
            '-I'+str(ROOT/'main'),'-I'+str(cjson),str(file),str(cjson/'cJSON.c'),
            str(ROOT/'main/fmo_text.c'),str(ROOT/'main/fmo_endpoint.c'),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)

if __name__=='__main__':run()
