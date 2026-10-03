<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 资源目录（Assets）

本目录集中存放可复用的资源（字库、图片、音乐等），按资源类型分子目录管理。每个资源放在其类型对应的子目录，并记录放置路径、命名方式、集成方式与来源/许可。二进制资源（字体、图片、音频）不属于纯 markdown 文档，请勿与文档混放。涉及版权/授权的资源需注明来源与许可。

## 字库（fonts）

`fonts/NotoSansCJKsc-Regular.otf` 是未经修改的 Noto CJK 常规字体，来源为
[Noto CJK](https://github.com/notofonts/noto-cjk/tree/main/Sans/OTF/SimplifiedChinese)，
使用 SIL OFL 1.1 许可（`fonts/OFL.txt`）。`fonts/fmo_channel_font.c` 为转换后的
16px、2bpp 压缩 LVGL 字库，覆盖 ASCII、中日韩标点及 U+4E00–U+9FFF 基本汉字区，
不包含扩展区汉字或表情。固件只链接生成的 C 文件，原文件保留供重新生成。
使用 `lv_font_conv@1.5.3` 执行生成文件头部的完整命令，并开启 LVGL 字体压缩和大字形偏移。
该字库用于频道名称，不是 FMO 原厂呼号字体。

可复用的字库文件与生成的字库源码放在 `fonts/`。

- 命名要能反映字族、字重、字级与格式。
- 记录来源、许可、字符范围、转换命令与目标放置路径。
- 添加字库前评估 Flash 与内部 RAM 影响；ESP32-C3 无 PSRAM。
- 不提交许可不允许分发的字库。

## 图片（images）

可复用的源图与生成的显示资产放在 `images/`。

- 使用描述性命名，并记录尺寸、像素格式、转换步骤与目标路径。
- 优先采用适合 240 × 320 RGB565 显示的格式，并纳入 Flash 与内部 RAM 考量。
- 许可允许时保留可编辑源文件，并记录来源与许可。
- 图片中不得包含设备二维码秘密、凭证或个人数据。

## 音乐与音效（music）

可复用的音乐与音效源码放在 `music/`。

- 记录来源、许可、采样率、位深、声道、转换命令与目标路径。
- 与当前 BSP 音频路径匹配时优先采用 16 kHz、16 位单声道 PCM。
- 嵌入音频前评估 Flash 与内部 RAM 成本；长录音应流式或分块。
- 无再分发许可不提交媒体文件。

## FMO 频道字库

`fonts/NotoSansCJKsc-Regular.otf` 和 `fonts/fmo_channel_font.c` 保留 Noto 字体来源及原始
16 px、2 bpp 压缩字形，许可见 `fonts/OFL.txt`。构建时 `tools/compact_font.py`
无损拆成位图各小于 1 MiB 的回退字体；固件和原生预览使用同一生成源码和紧凑字形描述。
不删除现有字符或抗锯齿效果，也不增加资源分区。

## FMO 呼号字体

`fonts/Montserrat-Bold.ttf` 是 LVGL v9.5.0 中 `tests/src/test_files/fonts/`
携带的未修改 Montserrat Bold 源字体，来自
[Montserrat 项目](https://github.com/JulietaUla/Montserrat)，使用 SIL OFL 1.1
许可（`fonts/Montserrat-OFL.txt`）。`fonts/fmo_callsign_bold_14.c`、`fonts/fmo_callsign_bold_20.c` 和
`fonts/fmo_callsign_bold_32.c` 是 14／20／32 px、4 bpp、未压缩的可打印 ASCII
字形（U+0020-U+007E），覆盖呼号后缀和标点。固件与原生预览只链接生成的 C 文件。
配网密码保留内置常规字重，中文标签保留现有字库。
在仓库根目录用 `lv_font_conv@1.5.3` 重新生成：

```bash
lv_font_conv --size 14 --bpp 4 --format lvgl --lv-include lvgl.h --font assets/fonts/Montserrat-Bold.ttf -r 0x20-0x7e --no-compress --no-prefilter --force-fast-kern-format -o assets/fonts/fmo_callsign_bold_14.c
lv_font_conv --size 20 --bpp 4 --format lvgl --lv-include lvgl.h --font assets/fonts/Montserrat-Bold.ttf -r 0x20-0x7e --no-compress --no-prefilter --force-fast-kern-format -o assets/fonts/fmo_callsign_bold_20.c
lv_font_conv --size 32 --bpp 4 --format lvgl --lv-include lvgl.h --font assets/fonts/Montserrat-Bold.ttf -r 0x20-0x7e --no-compress --no-prefilter --force-fast-kern-format -o assets/fonts/fmo_callsign_bold_32.c
```
