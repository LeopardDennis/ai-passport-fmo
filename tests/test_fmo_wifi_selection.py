"""Exercise production saved-name resolution and worker switching with Wi-Fi stubs."""
import os
from pathlib import Path
import subprocess
import tempfile
from test_fmo_network import extract_function

ROOT = Path(__file__).resolve().parents[1]
provision = (ROOT / 'main/fmo_provision.c').read_text()
network = (ROOT / 'main/fmo_network.c').read_text()
for name in ('fmo_provision_saved_wifi', 'fmo_provision_config_saved'):
    provision = provision.replace('bool '+name+'(', 'static bool '+name+'(')
network = network.replace('bool fmo_network_request_wifi(', 'static bool fmo_network_request_wifi(')
functions = '\n'.join(extract_function(provision,n) for n in
                     ('to_wifi_config','fmo_provision_saved_wifi','fmo_provision_config_saved'))
functions += '\n' + '\n'.join(extract_function(network,n) for n in ('switch_saved_wifi','fmo_network_request_wifi'))
preamble = r'''
#include "fmo_wifi_profiles.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdatomic.h>
#define pdTRUE 1
#define portMAX_DELAY 99999
#define pdMS_TO_TICKS(x) (x)
#define ESP_OK 0
#define ESP_ERR_TIMEOUT -1
#define WIFI_IF_STA 0
#define WIFI_AUTH_WPA2_PSK 1
#define WIFI_AUTH_OPEN 0
#define FMO_WIFI_READY_BIT 1
#define FMO_WIFI_DISCONNECTED_BIT 2
#define FMO_WIFI_STOPPED_BIT 4
#define FMO_WIFI_CANCEL_BIT 8
#define FMO_UPDATE_WIFI 0
#define MALLOC_CAP_INTERNAL 1
typedef fmo_wifi_credential_t credentials_t;
typedef int esp_err_t;
typedef struct { struct {unsigned char ssid[32],password[64];struct{int authmode;} threshold;} sta; } wifi_config_t;
static fmo_wifi_profiles_t s_profiles;
static int s_profiles_lock=1,s_state_lock=2,s_wifi_bits=1,s_network_task=1;
static struct { struct{bool wifi_connected;} state;char connected_ssid[33];} s_snapshot;
static bool s_selected_pending,s_was_online=true;
static char s_selected_ssid[33];
static atomic_bool s_reconnect=true,s_setup_active;
static uint8_t s_tried_profiles;
static uint64_t s_next_wifi_attempt;
static int s_events_socket,s_control_socket;
static unsigned bits=FMO_WIFI_READY_BIT;
static int locks[3],fail_lock,notices,stop_sockets,dns_clears,connects,start_count,stop_count,stop_failure,barrier_failure;
static bool config_set;
static wifi_config_t chosen;
static char error[48];
static uint64_t now_ms(void){return 10000;}
static int xSemaphoreTake(int lock,int wait){assert(lock>0 && lock<3);if(fail_lock==lock && wait==0)return 0;assert(!locks[lock]);locks[lock]=1;return pdTRUE;}
static void xSemaphoreGive(int lock){assert(locks[lock]);locks[lock]=0;}
static void xTaskNotifyGive(int task){assert(task==1);++notices;}
static void xEventGroupSetBits(int group,unsigned mask){assert(group==1);bits|=mask;}
static void xEventGroupClearBits(int group,unsigned mask){assert(group==1);bits&=~mask;}
static unsigned xEventGroupWaitBits(int group,unsigned mask,int clear,int all,int wait){
    assert(group==1 && mask==FMO_WIFI_STOPPED_BIT && clear==1 && all==1 && wait==3000);
    unsigned result=barrier_failure ? 0 : bits;bits&=~mask;return result;
}
static void stop_socket(int *socket){assert(socket==&s_events_socket || socket==&s_control_socket);++stop_sockets;}
static void update_clock_service(bool online){assert(!online);}
static void post_link(int type,bool online){assert(type==FMO_UPDATE_WIFI && !online);s_snapshot.state.wifi_connected=false;s_snapshot.connected_ssid[0]=0;}
static void post_error(const char *text){snprintf(error,sizeof(error),"%s",text);}
static void clear_dns_cache(void *unused){(void)unused;++dns_clears;}
static int tcpip_callback_wait(void(*callback)(void *),void *arg){callback(arg);return 0;}
static int esp_wifi_stop(void){assert(!atomic_load(&s_reconnect));++stop_count;bits|=FMO_WIFI_STOPPED_BIT;return stop_failure ? -2 : 0;}
static int esp_wifi_set_config(int interface,const wifi_config_t *config){assert(interface==WIFI_IF_STA && !(bits & 3));chosen=*config;config_set=true;return 0;}
static int esp_wifi_start(void){assert(config_set && !atomic_load(&s_reconnect));++start_count;return 0;}
static int esp_wifi_connect(void){assert(config_set && !atomic_load(&s_reconnect));++connects;return 0;}
'''
tests = r'''
int main(void){
    fmo_wifi_profiles_init(&s_profiles);
    for(unsigned i=0;i<5;++i){credentials_t c={0};snprintf(c.ssid,sizeof(c.ssid),"wifi-%u",i);strcpy(c.password,"valid-password");assert(fmo_wifi_profiles_put(&s_profiles,&c));}
    fmo_wifi_list_t list={0};assert(fmo_provision_saved_wifi(&list) && list.count==5);
    assert(!strcmp(list.names[4],"wifi-4"));
    fmo_wifi_list_t previous=list;fail_lock=1;
    assert(!fmo_provision_saved_wifi(&list) && !memcmp(&list,&previous,sizeof(list)));fail_lock=0;
    assert(!fmo_provision_saved_wifi(NULL));
    s_profiles_lock=0;assert(fmo_provision_saved_wifi(&list) && !list.count);s_profiles_lock=1;
    assert(!fmo_network_request_wifi(NULL) && !fmo_network_request_wifi("") && !fmo_network_request_wifi("123456789012345678901234567890123"));
    fail_lock=2;assert(!fmo_network_request_wifi("wifi-2") && !s_selected_pending);fail_lock=0;
    strcpy(s_snapshot.connected_ssid,"wifi-0");s_snapshot.state.wifi_connected=true;
    atomic_store(&s_setup_active,true);
    assert(fmo_network_request_wifi("wifi-2") && s_selected_pending && notices==1 && (bits & FMO_WIFI_CANCEL_BIT));
    atomic_store(&s_setup_active,false);bits&=~FMO_WIFI_CANCEL_BIT;
    assert(switch_saved_wifi());
    assert(!s_selected_pending && !memcmp(chosen.sta.ssid,"wifi-2",6) && !memcmp(chosen.sta.password,"valid-password",14));
    assert(stop_sockets==2 && dns_clears==1 && stop_count==1 && start_count==1 && connects==1);
    assert(s_tried_profiles==4 && s_next_wifi_attempt==35000 && !s_was_online && !error[0]);
    assert(!s_snapshot.connected_ssid[0] && !s_snapshot.state.wifi_connected); // Only GOT_IP confirms the badge.
    assert(switch_saved_wifi() && connects==1); // One request is consumed once.
    strcpy(s_snapshot.connected_ssid,"wifi-2");s_snapshot.state.wifi_connected=true;bits=FMO_WIFI_READY_BIT;
    assert(fmo_network_request_wifi("wifi-2") && switch_saved_wifi() && connects==1 && stop_count==1); // Keep the healthy selected link.
    assert(fmo_network_request_wifi("wifi-3"));
    assert(fmo_wifi_profiles_remove(&s_profiles,"wifi-0")); // Index shifts after the menu was shown.
    assert(switch_saved_wifi() && !memcmp(chosen.sta.ssid,"wifi-3",6) && s_tried_profiles==4 && connects==2);
    assert(fmo_network_request_wifi("wifi-4"));assert(fmo_wifi_profiles_remove(&s_profiles,"wifi-4"));
    assert(switch_saved_wifi() && connects==2 && !strcmp(error,"WI-FI NO LONGER SAVED"));
    assert(fmo_network_request_wifi("wifi-1"));barrier_failure=1;
    assert(!switch_saved_wifi() && connects==2 && !strcmp(error,"WIFI SWITCH FAILED"));barrier_failure=0;
    assert(fmo_network_request_wifi("wifi-1"));stop_failure=1;
    assert(!switch_saved_wifi() && connects==2);stop_failure=0;
    assert(fmo_network_request_wifi("wifi-1") && switch_saved_wifi() && connects==3);
    assert(!locks[1] && !locks[2]);
    puts("FMO saved Wi-Fi selection: PASS (names, current link, stale list, DHCP barrier, failure)");
}
'''
with tempfile.TemporaryDirectory(prefix='fmo-wifi-select-') as tmp:
    file=Path(tmp)/'test.c';file.write_text(preamble+functions+tests)
    binary=Path(tmp)/'test'
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'main'),str(file),str(ROOT/'main/fmo_wifi_profiles.c'),str(ROOT/'main/fmo_credentials.c'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
