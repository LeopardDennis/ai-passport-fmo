"""Compile the production BSP audio driver against checked I2S/codec stubs."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
HEADER = r'''
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE -2
#define BSP_I2S_PORT 0
#define BSP_I2S_MCLK 6
#define BSP_I2S_BCLK 5
#define BSP_I2S_WS 3
#define BSP_I2S_DOUT 2
#define BSP_I2S_DIN 4
#define BSP_I2S_PA_CTRL (-1)
#define BSP_I2C_PORT 0
#define BSP_I2C_SDA 10
#define BSP_I2C_SCL 7
#define BSP_I2C_ES8311_ADDR 0x18
#define I2S_GPIO_UNUSED (-1)
#define I2S_ROLE_MASTER 1
#define I2S_CLK_SRC_DEFAULT 0
#define I2S_MCLK_MULTIPLE_256 256
#define I2S_DATA_BIT_WIDTH_16BIT 16
#define I2S_SLOT_BIT_WIDTH_AUTO 0
#define I2S_SLOT_MODE_STEREO 2
#define I2S_STD_SLOT_BOTH 3
#define ESP_CODEC_DEV_WORK_MODE_DAC 2
#define ESP_CODEC_DEV_WORK_MODE_BOTH 3
#define ESP_CODEC_DEV_TYPE_OUT 2
#define ESP_CODEC_DEV_TYPE_IN_OUT 3
#define ESP_CODEC_DEV_MAKE_CHANNEL_MASK(n) (1U << (n))
static inline void discard_log(const char *format, ...) {(void)format;}
#define ESP_LOGI(tag,...) ((void)(tag),discard_log(__VA_ARGS__))
#define ESP_LOGE(tag,...) ((void)(tag),discard_log(__VA_ARGS__))
const char *esp_err_to_name(esp_err_t err);
typedef struct {bool enabled;} channel_t;
typedef channel_t *i2s_chan_handle_t;
typedef struct {int id, role, dma_desc_num, dma_frame_num; bool auto_clear_after_cb, auto_clear_before_cb; int intr_priority;} i2s_chan_config_t;
typedef struct {
    struct {uint32_t sample_rate_hz; int clk_src, ext_clk_freq_hz, mclk_multiple;} clk_cfg;
    struct {int data_bit_width, slot_bit_width, slot_mode, slot_mask, ws_width; bool ws_pol, bit_shift, left_align, big_endian, bit_order_lsb;} slot_cfg;
    struct {int mclk, bclk, ws, dout, din; struct {bool mclk_inv,bclk_inv,ws_inv;} invert_flags;} gpio_cfg;
} i2s_std_config_t;
typedef void *esp_codec_dev_handle_t;
typedef struct {int placeholder;} audio_codec_ctrl_if_t;
typedef struct {int placeholder;} audio_codec_data_if_t;
typedef struct {int placeholder;} audio_codec_if_t;
typedef struct {int port,addr;void *bus_handle;} audio_codec_i2c_cfg_t;
typedef struct {int port;i2s_chan_handle_t tx_handle,rx_handle;} audio_codec_i2s_cfg_t;
typedef struct {const audio_codec_ctrl_if_t *ctrl_if; void *gpio_if; int codec_mode,pa_pin;bool pa_reverted,master_mode,use_mclk;struct {float pa_voltage,codec_dac_voltage;} hw_gain;bool no_dac_ref;} es8311_codec_cfg_t;
typedef struct {int dev_type;const audio_codec_if_t *codec_if;const audio_codec_data_if_t *data_if;} esp_codec_dev_cfg_t;
typedef struct {uint8_t bits_per_sample,channel;unsigned channel_mask;uint32_t sample_rate;int mclk_multiple;} esp_codec_dev_sample_info_t;
esp_err_t bsp_i2c_init(void);
void *bsp_i2c_bus(void);
esp_err_t i2s_new_channel(const i2s_chan_config_t *,i2s_chan_handle_t *,i2s_chan_handle_t *);
esp_err_t i2s_channel_init_std_mode(i2s_chan_handle_t,const i2s_std_config_t *);
esp_err_t i2s_channel_enable(i2s_chan_handle_t);
const audio_codec_ctrl_if_t *audio_codec_new_i2c_ctrl(const audio_codec_i2c_cfg_t *);
const audio_codec_data_if_t *audio_codec_new_i2s_data(const audio_codec_i2s_cfg_t *);
void *audio_codec_new_gpio(void);
const audio_codec_if_t *es8311_codec_new(const es8311_codec_cfg_t *);
esp_codec_dev_handle_t esp_codec_dev_new(const esp_codec_dev_cfg_t *);
int esp_codec_dev_open(esp_codec_dev_handle_t,const esp_codec_dev_sample_info_t *);
int esp_codec_dev_close(esp_codec_dev_handle_t);
int esp_codec_dev_set_in_gain(esp_codec_dev_handle_t,float);
int esp_codec_dev_write(esp_codec_dev_handle_t,void *,size_t);
int esp_codec_dev_read(esp_codec_dev_handle_t,void *,size_t);
int esp_codec_dev_set_out_vol(esp_codec_dev_handle_t,uint8_t);
'''
TEST = r'''
#include "audio_env.h"
#include "bsp_audio.h"
#include <stdio.h>
#include <string.h>
static bool playback;
static channel_t tx,rx;
static audio_codec_ctrl_if_t ctrl;
static audio_codec_data_if_t data;
static audio_codec_if_t codec;
static int device,bus,gpio;
static unsigned allocations,tx_inits,rx_inits,opens,closes,gains,reads,writes,volume;
static int io_error,open_error;
static uint32_t expected_rate=8000;
const char *esp_err_to_name(esp_err_t err) {(void)err;return "stub";}
esp_err_t bsp_i2c_init(void) {return ESP_OK;}
void *bsp_i2c_bus(void) {return &bus;}
esp_err_t i2s_new_channel(const i2s_chan_config_t *cfg,i2s_chan_handle_t *out,i2s_chan_handle_t *in) {
    assert(cfg->dma_desc_num==6 && cfg->dma_frame_num==240 && cfg->role==I2S_ROLE_MASTER);
    assert(cfg->auto_clear_after_cb && !cfg->auto_clear_before_cb);
    assert(out && (playback ? in==NULL : in!=NULL));
    ++allocations;*out=&tx;if(in)*in=&rx;return ESP_OK;
}
esp_err_t i2s_channel_init_std_mode(i2s_chan_handle_t handle,const i2s_std_config_t *cfg) {
    assert(cfg->gpio_cfg.din==(playback ? I2S_GPIO_UNUSED : BSP_I2S_DIN));
    assert(cfg->gpio_cfg.dout==BSP_I2S_DOUT && cfg->gpio_cfg.mclk==BSP_I2S_MCLK);
    assert(cfg->clk_cfg.mclk_multiple==256 && cfg->slot_cfg.slot_mode==I2S_SLOT_MODE_STEREO);
    if(handle==&tx)++tx_inits;else {assert(handle==&rx && !playback);++rx_inits;}
    return ESP_OK;
}
esp_err_t i2s_channel_enable(i2s_chan_handle_t handle) {
    assert(handle==&tx || (!playback && handle==&rx));handle->enabled=true;return ESP_OK;
}
const audio_codec_ctrl_if_t *audio_codec_new_i2c_ctrl(const audio_codec_i2c_cfg_t *cfg) {
    assert(cfg->bus_handle==&bus && cfg->addr==(BSP_I2C_ES8311_ADDR<<1));return &ctrl;
}
const audio_codec_data_if_t *audio_codec_new_i2s_data(const audio_codec_i2s_cfg_t *cfg) {
    assert(cfg->tx_handle==&tx && cfg->rx_handle==(playback ? NULL : &rx));return &data;
}
void *audio_codec_new_gpio(void) {return &gpio;}
const audio_codec_if_t *es8311_codec_new(const es8311_codec_cfg_t *cfg) {
    assert(cfg->codec_mode==(playback ? ESP_CODEC_DEV_WORK_MODE_DAC : ESP_CODEC_DEV_WORK_MODE_BOTH));
    assert(cfg->ctrl_if==&ctrl && cfg->gpio_if==&gpio && cfg->pa_pin==BSP_I2S_PA_CTRL);
    assert(cfg->use_mclk && !cfg->master_mode && cfg->no_dac_ref);return &codec;
}
esp_codec_dev_handle_t esp_codec_dev_new(const esp_codec_dev_cfg_t *cfg) {
    assert(cfg->dev_type==(playback ? ESP_CODEC_DEV_TYPE_OUT : ESP_CODEC_DEV_TYPE_IN_OUT));
    assert(cfg->codec_if==&codec && cfg->data_if==&data);return &device;
}
int esp_codec_dev_open(esp_codec_dev_handle_t h,const esp_codec_dev_sample_info_t *fs) {
    assert(h==&device && tx.enabled && (playback ? !rx.enabled : rx.enabled));
    assert(fs->sample_rate==expected_rate && fs->bits_per_sample==16 && fs->channel==1);
    assert(fs->channel_mask==1);++opens;return open_error;
}
int esp_codec_dev_close(esp_codec_dev_handle_t h) {
    assert(h==&device);tx.enabled=rx.enabled=false;++closes;return 0;
}
int esp_codec_dev_set_in_gain(esp_codec_dev_handle_t h,float gain) {
    assert(h==&device && !playback && gain==30.0f);++gains;return 0;
}
int esp_codec_dev_write(esp_codec_dev_handle_t h,void *pcm,size_t bytes) {
    assert(h==&device && pcm && bytes==320);++writes;return io_error;
}
int esp_codec_dev_read(esp_codec_dev_handle_t h,void *pcm,size_t bytes) {
    assert(!playback && h==&device && pcm && bytes==320);++reads;return io_error;
}
int esp_codec_dev_set_out_vol(esp_codec_dev_handle_t h,uint8_t percent) {
    assert(h==&device);volume=percent;return 0;
}
int main(int argc,char **argv) {
    assert(argc==2);playback=!strcmp(argv[1],"playback");
    int16_t pcm[160]={0};
    assert(bsp_audio_set_format(8000,16,1)==ESP_ERR_INVALID_STATE);
    assert(bsp_audio_read(pcm,sizeof(pcm))==ESP_ERR_INVALID_STATE);
    assert(bsp_audio_write(pcm,sizeof(pcm))==ESP_ERR_INVALID_STATE);
    esp_err_t (*init)(void)=playback ? bsp_audio_init_playback : bsp_audio_init;
    esp_err_t (*other)(void)=playback ? bsp_audio_init : bsp_audio_init_playback;
    assert(init()==ESP_OK && init()==ESP_OK && allocations==1);
    assert(other()==ESP_ERR_INVALID_STATE && allocations==1);
    assert(tx_inits==1 && rx_inits==(playback ? 0U : 1U));
    assert(bsp_audio_set_format(8000,16,1)==ESP_OK);
    assert(bsp_audio_set_format(8000,16,1)==ESP_OK && opens==1 && closes==0);
    expected_rate=16000;
    assert(bsp_audio_set_format(16000,16,1)==ESP_OK && opens==2 && closes==1);
    expected_rate=8000;open_error=-7;
    assert(bsp_audio_set_format(8000,16,1)==ESP_FAIL);
    open_error=0;
    assert(bsp_audio_set_format(8000,16,1)==ESP_OK && opens==4 && closes==2);
    assert(gains==(playback ? 0U : 3U));
    assert(bsp_audio_write(pcm,sizeof(pcm))==ESP_OK && writes==1);
    bsp_audio_set_volume(50);assert(volume==50);
    bsp_audio_set_volume(0);assert(volume==0);
    assert(bsp_audio_read(pcm,sizeof(pcm))==(playback ? ESP_ERR_INVALID_STATE : ESP_OK));
    assert(reads==(playback ? 0U : 1U));
    io_error=-1;
    assert(bsp_audio_write(pcm,sizeof(pcm))==ESP_FAIL);
    assert(bsp_audio_read(pcm,sizeof(pcm))==(playback ? ESP_ERR_INVALID_STATE : ESP_FAIL));
    assert(init()==ESP_OK && other()==ESP_ERR_INVALID_STATE && allocations==1);
    printf("BSP audio %s: PASS (DMA direction, codec mode, idempotence, format reopen, I/O)\n",argv[1]);
}
'''
with tempfile.TemporaryDirectory(prefix="bsp-audio-test-") as tmp:
    directory = Path(tmp)
    (directory / "audio_env.h").write_text(HEADER)
    for name in ("esp_err.h", "esp_log.h", "bsp_i2c.h", "bsp_pins.h",
                 "esp_codec_dev.h", "esp_codec_dev_defaults.h", "es8311_codec.h",
                 "driver/i2s_std.h"):
        path = directory / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('#include "audio_env.h"\n')
    (directory / "test.c").write_text(TEST)
    binary = directory / "bsp-audio-test"
    subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-I" + str(directory), "-I" + str(ROOT / "components/bsp/include"),
                    str(ROOT / "components/bsp/src/bsp_audio.c"), str(directory / "test.c"),
                    "-o", str(binary)], check=True)
    for mode in ("playback", "duplex"):
        subprocess.run([str(binary), mode], check=True, timeout=10)
