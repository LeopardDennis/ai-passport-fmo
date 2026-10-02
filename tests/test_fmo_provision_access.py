"""Exercise the production AP guard and root handler with real dual-stack sockets."""
import os
from pathlib import Path
import subprocess
import tempfile
from test_fmo_network import extract_function

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'main/fmo_provision.c').read_text()
functions = '\n'.join(extract_function(source, name) for name in ('local_request', 'root_get'))
preamble = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#define HTTPD_403_FORBIDDEN 403
typedef int esp_err_t;
typedef struct {int fd, code, chunks;} httpd_req_t;
static uint32_t s_ap_address;
static const char page[]="setup", page_end[]="end", s_token[]="test-token";
static int httpd_req_to_sockfd(httpd_req_t *req){return req->fd;}
static int httpd_resp_send_err(httpd_req_t *req,int code,const char *text){assert(!strcmp(text,"AP only"));req->code=code;return -1;}
static int httpd_resp_set_type(httpd_req_t *req,const char *type){(void)req;(void)type;return 0;}
static int httpd_resp_set_hdr(httpd_req_t *req,const char *key,const char *value){(void)req;(void)key;(void)value;return 0;}
static int httpd_resp_send_chunk(httpd_req_t *req,const char *data,size_t size){(void)data;(void)size;++req->chunks;return 0;}
static int simulated, socket_error;
static struct sockaddr_storage socket_address;
static socklen_t socket_size;
static int test_getsockname(int fd,struct sockaddr *out,socklen_t *size){
    if(!simulated)return getsockname(fd,out,size);
    if(socket_error)return -1;
    socklen_t copied=*size<socket_size?*size:socket_size;
    memcpy(out,&socket_address,copied);*size=copied;return 0;
}
#define getsockname test_getsockname
'''
tests = r'''
static void check(bool allowed,int fd){
    httpd_req_t req={.fd=fd};
    assert(local_request(&req)==allowed);
    assert((root_get(&req)==0)==allowed);
    assert(allowed ? req.chunks==4 && !req.code : req.code==403 && !req.chunks);
}
static void ipv4(const char *ip){
    memset(&socket_address,0,sizeof(socket_address));
    struct sockaddr_in *a=(struct sockaddr_in *)&socket_address;
    a->sin_family=AF_INET;assert(inet_pton(AF_INET,ip,&a->sin_addr)==1);
    socket_size=sizeof(*a);
}
#if CONFIG_LWIP_IPV6
static void ipv6(const char *ip){
    memset(&socket_address,0,sizeof(socket_address));
    struct sockaddr_in6 *a=(struct sockaddr_in6 *)&socket_address;
    a->sin6_family=AF_INET6;assert(inet_pton(AF_INET6,ip,&a->sin6_addr)==1);
    socket_size=sizeof(*a);
}
#endif
static void real_connection(int family){
    simulated=0;
    int listener=socket(family,SOCK_STREAM,0);assert(listener>=0);
    struct sockaddr_storage address={0};socklen_t length=0;
    if(family==AF_INET){
        struct sockaddr_in *a=(struct sockaddr_in *)&address;
        a->sin_family=AF_INET;a->sin_addr.s_addr=htonl(INADDR_LOOPBACK);length=sizeof(*a);
    }
#if CONFIG_LWIP_IPV6
    else {
        int only=0;assert(setsockopt(listener,IPPROTO_IPV6,IPV6_V6ONLY,&only,sizeof(only))==0);
        struct sockaddr_in6 *a=(struct sockaddr_in6 *)&address;
        a->sin6_family=AF_INET6;a->sin6_addr=in6addr_any;length=sizeof(*a);
    }
#endif
    assert(bind(listener,(struct sockaddr *)&address,length)==0);
    assert(getsockname(listener,(struct sockaddr *)&address,&length)==0);
    assert(listen(listener,1)==0);
    struct sockaddr_in target={.sin_family=AF_INET,.sin_addr={.s_addr=htonl(INADDR_LOOPBACK)}};
    target.sin_port=family==AF_INET ? ((struct sockaddr_in *)&address)->sin_port :
#if CONFIG_LWIP_IPV6
        ((struct sockaddr_in6 *)&address)->sin6_port;
#else
        0;
#endif
    int client=socket(AF_INET,SOCK_STREAM,0);assert(client>=0);
    assert(connect(client,(struct sockaddr *)&target,sizeof(target))==0);
    int accepted=accept(listener,NULL,NULL);assert(accepted>=0);
#if CONFIG_LWIP_IPV6
    if(family==AF_INET6){
        length=sizeof(address);assert(getsockname(accepted,(struct sockaddr *)&address,&length)==0);
        assert(address.ss_family==AF_INET6);
        assert(IN6_IS_ADDR_V4MAPPED(&((struct sockaddr_in6 *)&address)->sin6_addr));
    }
#endif
    s_ap_address=htonl(INADDR_LOOPBACK);check(true,accepted);
    s_ap_address=inet_addr("192.168.9.1");check(false,accepted);
    close(accepted);close(client);close(listener);
}
int main(void){
    real_connection(AF_INET);
#if CONFIG_LWIP_IPV6
    real_connection(AF_INET6);
#endif
    simulated=1;s_ap_address=inet_addr("192.168.9.1");
    ipv4("192.168.9.1");check(true,0);
    ipv4("192.168.1.20");check(false,0);
    ipv4("0.0.0.0");check(false,0);
    ipv4("192.168.9.1");socket_size=sizeof(struct sockaddr_in)-1;check(false,0);
#if CONFIG_LWIP_IPV6
    ipv6("::ffff:192.168.9.1");check(true,0);
    socket_size=sizeof(struct sockaddr_in6)-1;check(false,0);
    ipv6("::ffff:192.168.1.20");check(false,0);
    ipv6("::");check(false,0);
    ipv6("::1");check(false,0);
    ipv6("2001:db8::c0a8:901");check(false,0);
#endif
    memset(&socket_address,0,sizeof(socket_address));socket_size=sizeof(socket_address);check(false,0);
    socket_error=1;check(false,0);socket_error=0;
    simulated=0;check(false,-1);
    puts("FMO setup AP access: PASS (IPv4/mapped IPv6, real sockets, station rejection)");
}
'''
with tempfile.TemporaryDirectory(prefix='fmo-provision-access-') as tmp:
    file = Path(tmp) / 'access.c'
    file.write_text(preamble + functions + tests)
    for ipv6 in (1, 0):
        binary = Path(tmp) / f'access-{ipv6}'
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                        f'-DCONFIG_LWIP_IPV6={ipv6}', str(file), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
