// components/bsp/include/bsp_audio.h
// ES8311 音频 codec:I2C 走控制口(复用 bsp_i2c 的共享总线),I2S 可选全双工或仅播放数据口。
#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

// 初始化 codec 与 I2S。默认全双工，保留播放和录音能力。
// 两个初始化入口均会调用 bsp_i2c_init()。由单一音频 worker 串行调用，
// 可能阻塞，不得在按键回调或 LVGL 任务中调用。首次成功初始化后模式固定：
// 相同模式重复调用幂等，切换模式返回 ESP_ERR_INVALID_STATE，需要重启。
// 初始化失败返回底层错误，尚不支持部分初始化失败后的重试或资源释放。
esp_err_t bsp_audio_init(void);

// 仅播放：只创建 I2S TX，DIN 不接入，codec 使用 DAC/OUT 模式。
// 不分配 RX DMA，不设置麦克风增益。先初始化，再 set_format，最后 PCM I/O。
esp_err_t bsp_audio_init_playback(void);

// 设置采样格式。同格式重复调用是廉价的(直接复用已打开的 codec)。
//
// ⚠ 这里有个必须绕开的坑:esp_codec_dev_open() 在 codec【已打开】时会直接返回 OK 且
//   【不重新配置采样率】。若不先 close,16kHz 播完再播 8kHz 会以 16k 时钟送出 ——
//   音调和速度都快一倍。故本函数在格式变化时先 close 再 open。
esp_err_t bsp_audio_set_format(uint32_t hz, uint8_t bits, uint8_t ch);

// 播放 / 录音。bytes 为字节数(16bit 单声道时 = 采样数 x 2)。
// 阻塞操作，仅用于音频 worker；仅播放模式调用 read 返回 ESP_ERR_INVALID_STATE。
esp_err_t bsp_audio_write(const void *pcm, size_t bytes);
esp_err_t bsp_audio_read(void *pcm, size_t bytes);

// 输出音量 0..100(%)。
void bsp_audio_set_volume(uint8_t percent);
