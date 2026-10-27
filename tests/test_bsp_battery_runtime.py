"""Exercise the production CW2017 driver and FMO battery worker with fault injection."""
from pathlib import Path
import os
import re
import subprocess
import tempfile
import unittest
from test_fmo_network import extract_function

ROOT = Path(__file__).resolve().parents[1]


def compile_run(code):
    with tempfile.TemporaryDirectory(prefix="fmo-battery-") as directory:
        source = Path(directory) / "test.c"
        binary = Path(directory) / "test"
        source.write_text(code)
        subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra",
                        "-Werror", "-Wno-unused-function", "-Wno-unused-variable",
                        str(source), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


class BatteryTests(unittest.TestCase):
    def test_battery_profile_initialization_and_failure_retry(self):
        source = (ROOT / 'components/bsp/src/bsp_battery.c').read_text()
        import hashlib
        profile_body = re.search(r'static const uint8_t s_battery_profile\[CW_PROFILE_SIZE\] = \{(.*?)\};', source, re.S).group(1)
        profile = bytes(int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]{2})', profile_body))
        self.assertEqual(len(profile), 80)
        self.assertEqual(hashlib.sha256(profile).hexdigest(),
                         '562086dac308f8aa8797a06ce0d731f290252ad3c6ed29206498ebfd267c1f57')
        code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef int esp_err_t;
typedef void *i2c_master_dev_handle_t;
typedef struct {int dev_addr_length,device_address,scl_speed_hz;} i2c_device_config_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NOT_FOUND 0x105
#define ESP_ERR_TIMEOUT 0x107
#define I2C_ADDR_BIT_LEN_7 0
#define BSP_I2C_CW2017_ADDR 0x63
#define pdMS_TO_TICKS(x) (x)
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
static uint8_t regs[256], written_reg[512], written_value[512];
static unsigned reads,writes,profile_writes,voltage_reads,soc_reads,adds,removes,delay_ms;
static unsigned fail_read_at,fail_write_at,ignore_write_at;
static unsigned raw_soc=96*256,valid_after;
static bool fail_bus,fail_add;
static int64_t now_us;
static int64_t esp_timer_get_time(void) {return now_us;}
static void vTaskDelay(unsigned ms) {assert(ms==10||ms==20||ms==100);delay_ms+=ms;now_us+=(int64_t)ms*1000;}
static int bsp_i2c_init(void) {return fail_bus?ESP_FAIL:ESP_OK;}
static void *bsp_i2c_bus(void) {return (void *)2;}
static int i2c_master_bus_add_device(void *bus,const i2c_device_config_t *cfg,void **out) {
 assert(bus==(void *)2 && cfg->device_address==0x63 && cfg->scl_speed_hz==100000);adds++;
 if(fail_add)return ESP_FAIL;*out=(void *)1;return ESP_OK;
}
static void i2c_master_bus_rm_device(void *dev) {assert(dev==(void *)1);removes++;}
static int i2c_master_transmit_receive(void *dev,const void *send,size_t count,void *out,size_t n,int timeout) {
 assert(dev==(void *)1 && count==1 && timeout==100);reads++;
 if(reads==fail_read_at)return ESP_ERR_TIMEOUT;
 unsigned reg=*(const uint8_t *)send;uint8_t *data=out;
 if(reg==4) {
  assert(n==2);soc_reads++;unsigned soc=valid_after && soc_reads>=valid_after?96*256:raw_soc;
  data[0]=soc>>8;data[1]=soc&255;
 } else if(reg==2) {assert(n==2);voltage_reads++;data[0]=0x2e;data[1]=0x40;}
 else {assert(n==1 && (reg==0 || reg==8 || reg==0x0b || (reg>=0x10 && reg<=0x5f)));*data=regs[reg];}
 return ESP_OK;
}
static int i2c_master_transmit(void *dev,const void *send,size_t n,int timeout) {
 assert(dev==(void *)1 && n==2 && timeout==100 && writes<512);
 const uint8_t *data=send;unsigned reg=data[0];writes++;
 written_reg[writes-1]=reg;written_value[writes-1]=data[1];
 if(writes==fail_write_at)return ESP_FAIL;
 assert(reg==8 || reg==0x0b || (reg>=0x10 && reg<=0x5f));
 if(reg>=0x10) {assert(regs[8]==0xf0 && !(regs[0x0b]&0x80));profile_writes++;}
 if(writes!=ignore_write_at)regs[reg]=data[1];
 return ESP_OK;
}
'''
        code += re.sub(r'^#include[^\n]*\n', '', source, flags=re.M)
        code += r'''
static void reset(void) {
 memset(regs,0,sizeof(regs));regs[0]=0x0f;regs[0x0b]=0x14;
 reads=writes=profile_writes=voltage_reads=soc_reads=adds=removes=delay_ms=0;
 fail_read_at=fail_write_at=ignore_write_at=valid_after=0;
 raw_soc=96*256;fail_bus=fail_add=false;now_us=0;s_dev=NULL;s_init_retry_at_us=0;
}
static void seed_profile(void) {memcpy(regs+0x10,s_battery_profile,80);regs[0x0b]=0x94;}
int main(void) {
 reset();assert(bsp_battery_init()==ESP_OK && s_dev);
 assert(profile_writes==80 && regs[0x0b]==0x94 && regs[8]==0);
 assert(memcmp(regs+0x10,s_battery_profile,80)==0);
 unsigned complete_writes=writes,complete_reads=reads;
 assert(complete_writes==86 && delay_ms==250 && voltage_reads==0);
 assert(written_reg[0]==8 && written_value[0]==0x30);
 assert(written_reg[1]==8 && written_value[1]==0xf0);
 assert(written_reg[2]==0x0b && written_value[2]==0x14);
 assert(written_reg[83]==0x0b && written_value[83]==0x94);
 assert(written_reg[84]==8 && written_value[84]==0x30);
 assert(written_reg[85]==8 && written_value[85]==0);
 assert(bsp_battery_init()==ESP_OK && writes==complete_writes && reads==complete_reads);
 // A later MCU boot with matching parameters does not rewrite or reset the gauge.
 s_dev=NULL;assert(bsp_battery_init()==ESP_OK && writes==complete_writes);
 for(unsigned soc=0;soc<=100;soc++) {raw_soc=soc*256+255;assert(bsp_battery_soc()==(int)soc);}
 assert(bsp_battery_mv()==3700 && voltage_reads==1);
 raw_soc=0xfee8;unsigned before_reads=reads;
 for(int i=0;i<20;i++) {now_us+=300000000;assert(bsp_battery_soc()==-1);}
 assert(writes==complete_writes && reads==before_reads+20); // No periodic restart/profile polling.
 raw_soc=83*256;assert(bsp_battery_soc()==83);
 // Missing flag, mismatched last byte and erased bytes each require a verified rewrite.
 reset();seed_profile();regs[0x0b]=0x14;assert(bsp_battery_init()==ESP_OK && profile_writes==80);
 reset();seed_profile();regs[0x5f]^=1;assert(bsp_battery_init()==ESP_OK && profile_writes==80);
 reset();memset(regs+0x10,255,80);assert(bsp_battery_init()==ESP_OK && profile_writes==80);
 reset();seed_profile();regs[8]=0xf0;assert(bsp_battery_init()==ESP_OK && writes==2 && profile_writes==0);
 // Startup calculation can settle; waiting is bounded and never maps 254 to a percentage.
 reset();raw_soc=0xfee8;valid_after=3;assert(bsp_battery_init()==ESP_OK && soc_reads==3);
 reset();raw_soc=0xfee8;assert(bsp_battery_init()==ESP_ERR_TIMEOUT && !s_dev && soc_reads==50);
 assert(delay_ms==5150 && removes==1);unsigned attempts=adds;
 for(int i=0;i<20;i++)assert(bsp_battery_soc()==-1);
 assert(adds==attempts);now_us=s_init_retry_at_us;raw_soc=96*256;
 assert(bsp_battery_soc()==96 && adds==attempts+1 && writes==complete_writes);
 // Every failed write/read aborts initialization and can recover on the next permitted sample.
 for(unsigned i=1;i<=complete_writes;i++) {
  reset();fail_write_at=i;assert(bsp_battery_init()!=ESP_OK && !s_dev && removes==1 && writes==i);
  fail_write_at=0;now_us=s_init_retry_at_us;
  assert(bsp_battery_soc()==96 && s_dev && memcmp(regs+0x10,s_battery_profile,80)==0);
 }
 for(unsigned i=1;i<=complete_reads;i++) {
  reset();fail_read_at=i;assert(bsp_battery_init()!=ESP_OK && !s_dev && removes==1 && reads==i);
  fail_read_at=0;now_us=s_init_retry_at_us;assert(bsp_battery_soc()==96 && s_dev);
 }
 // Failed reads of an existing matching profile must never trigger an overwrite.
 reset();seed_profile();assert(bsp_battery_init()==ESP_OK);unsigned matched_reads=reads;
 for(unsigned i=1;i<=matched_reads;i++) {
  reset();seed_profile();fail_read_at=i;
  assert(bsp_battery_init()!=ESP_OK && !s_dev && writes==0);
 }
 // Acknowledged but ignored sleep/profile/flag/activation writes must fail readback.
 const unsigned ignored[]={2,4,83,84,86};
 for(unsigned j=0;j<sizeof(ignored)/sizeof(ignored[0]);j++) {
  reset();ignore_write_at=ignored[j];assert(bsp_battery_init()!=ESP_OK && !s_dev);
  ignore_write_at=0;now_us=s_init_retry_at_us;assert(bsp_battery_soc()==96);
 }
 reset();seed_profile();regs[0x5f]^=1;ignore_write_at=3;
 assert(bsp_battery_init()!=ESP_OK && !s_dev && profile_writes==0); // Cannot clear update flag.
 reset();fail_bus=true;assert(bsp_battery_init()!=ESP_OK && !s_dev && adds==0);
 reset();fail_add=true;assert(bsp_battery_init()!=ESP_OK && !s_dev && reads==0);
 reset();fail_read_at=1;assert(bsp_battery_init()==ESP_ERR_NOT_FOUND && !s_dev);
 for(int i=0;i<20;i++)assert(bsp_battery_soc()==-1);
 assert(adds==1);fail_read_at=0;now_us=s_init_retry_at_us-1;assert(bsp_battery_soc()==-1);
 now_us++;assert(bsp_battery_soc()==96 && adds==2);
 fail_read_at=reads+1;before_reads=reads;unsigned before_writes=writes;
 assert(bsp_battery_soc()==-1 && reads==before_reads+1 && writes==before_writes);
 puts("Battery original profile restore, verification, bounded startup retry and no periodic reset: PASS");
}
'''
        compile_run(code)

    def test_worker_survives_startup_failure_and_respects_display(self):
        source = (ROOT / 'main/main.c').read_text()
        worker = extract_function(source, 'battery_task')
        startup = source[source.index('void app_main(void)'):]
        code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include <setjmp.h>
#define ESP_OK 0
#define pdTRUE 1
#define pdPASS 1
#define pdMS_TO_TICKS(x) (x)
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGD(...) ((void)0)
#define APP_INPUT_BATTERY 1
#define FMO_STUB_ERROR -1
typedef int esp_err_t;
typedef struct {int type,value;} app_input_t;
typedef struct {char error[48];} fmo_snapshot_t;
static int s_state, s_brightness, s_fmo_queue, s_input_queue, s_battery_task;
static struct {bool audio_enabled;int volume;} s_controls;
static atomic_bool s_display_dark;
static unsigned samples, publications, waits, created, initialized, network_started;
static int queue_value;
static jmp_buf worker_end;
static void battery_task(void *argument);
static void fmo_monitor_state_init(int *state) {*state=0;}
static int bsp_i2c_init(void) {return ESP_OK;}
static int bsp_display_init(void) {return ESP_OK;}
static bool bsp_lvgl_init(void) {return true;}
static void bsp_display_backlight(int value) {(void)value;}
static int xQueueCreate(int length,size_t size) {assert(length>0 && size>0);static int q;return ++q;}
static bool bsp_lvgl_lock(int wait) {(void)wait;return true;}
static void bsp_lvgl_unlock(void) {}
static void build_ui(void) {}
static void button_event(void) {}
static int bsp_button_init(void (*callback)(void),void *user) {(void)callback;(void)user;return ESP_OK;}
static int bsp_battery_init(void) {initialized++;return FMO_STUB_ERROR;}
static int xTaskCreate(void (*callback)(void *),const char *name,unsigned stack,void *arg,unsigned priority,int *handle) {
 assert(callback==battery_task && !strcmp(name,"battery_monitor") && stack>=2048 && !arg && priority>0);
 created++;*handle=1;return pdPASS;
}
static void fmo_audio_set_volume(int volume) {(void)volume;}
static int fmo_audio_start(void) {return ESP_OK;}
static int fmo_network_start(int queue) {assert(queue);network_started++;return ESP_OK;}
static void xQueueOverwrite(int queue,void *value) {(void)queue;(void)value;}
static int bsp_battery_soc(void) {
 assert(!atomic_load(&s_display_dark));
 const int values[]={-1,-1,73,0};assert(samples<4);return values[samples++];
}
static int xQueueSend(int queue,const app_input_t *input,int wait) {
 assert(queue==s_input_queue && !wait && input->type==APP_INPUT_BATTERY);
 queue_value=input->value;publications++;return pdPASS;
}
static unsigned ulTaskNotifyTake(int clear,unsigned timeout) {
 assert(clear==pdTRUE);
 switch(waits++) {
 case 0: assert(samples==1 && queue_value==-1 && timeout==60000);break;
 case 1: // It remains unavailable; the screen goes dark before another retry.
  assert(samples==2 && timeout==60000);atomic_store(&s_display_dark,true);break;
 case 2: // Dark iteration must not access the gauge. Simulate a wake notification.
  assert(samples==2 && publications==2 && timeout==120000);
  atomic_store(&s_display_dark,false);return 1;
 case 3: // Recovery returns a real reading and restores the normal cadence.
  assert(samples==3 && queue_value==73 && timeout==120000);break;
 case 4: // Zero percent is valid, not an initialization failure.
  assert(samples==4 && queue_value==0 && timeout==120000);longjmp(worker_end,1);
 default: assert(false);
 }
 return 0;
}
'''
        code += worker + startup
        code += r'''
int main(void) {
 app_main();
 assert(initialized==1 && created==1 && s_battery_task && network_started==1);
 if(!setjmp(worker_end))battery_task(NULL);
 assert(samples==4 && publications==4 && waits==5);
 puts("FMO battery worker: PASS (failed startup, retry, dark pause, wake, recovery, valid zero)");
}
'''
        compile_run(code)


if __name__ == '__main__':
    unittest.main()
