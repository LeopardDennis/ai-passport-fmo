<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# AI Passport FMO 实时通联显示器

这套固件把 FoloToy AI Passport 变成一块常驻的 FMO（NFM Over Internet）状态屏。它会显示当前选择的 FMO 频道、正在发言的呼号、空闲时的最近发言呼号，以及可用的网格／主机标记、连接状态和电池电量。

## 工作方式

显示器使用 FMO 设备提供的局域网网页接口：

- `ws://<host>:<port>/events` 提供 `qso/callsign` 事件，其中包括 `callsign`、`isSpeaking`、`isHost` 和可选的 `grid` 字段；
- `ws://<host>:<port>/ws` 定期接收 `station/getCurrent` 请求，让屏幕显示当前选择的频道。

两个 WebSocket 都会自动重连。固件只观察 FMO 的局域网接口，不会模拟 FMO 电台，也不会直接连接 FMO MQTT 服务器，因此无需把 FMO 设备证书或私钥复制到 AI Passport。

## 配置

使用 ESP-IDF 5.5.3，通过下面的命令把私密配置写入已被 Git 忽略的 `sdkconfig` 文件：

```bash
idf.py menuconfig
```

进入 **FMO Live Monitor**，设置：

- 2.4 GHz Wi-Fi 名称和密码；
- FMO 主机名或 IPv4 地址，不包含 `ws://`、端口和路径；
- FMO 网页接口端口，通常为 `80`；
- 初始屏幕亮度。

如果局域网不能解析 `fmo.local`，建议直接填写 IPv4 地址。仓库中的默认配置故意不包含任何凭证。没有填写 SSID 或 FMO 主机时，屏幕会显示 `SET WIFI + FMO HOST`，并且不会启动无线网络。

## 构建和安装

激活 ESP-IDF 5.5.3，然后执行：

```bash
./tools/validate.sh --static
./tools/validate.sh --configured
```

可安装的合并固件为 `build/FoloToy-AI-Passport-full.bin`。必须保留受保护的 `cardid` 和 Recovery 分区。对于已经完成出厂配置的设备，优先使用 AI Passport 小程序安装；不要擦除整片 Flash。

## 按键

- **上键**：背光增加 10%；
- **下键**：背光降低 10%；
- **确定键**：立即刷新当前 FMO 频道。

## FMO 兼容性

当前实现依据现有社区 FMO 网页客户端所使用的接口。不同 FMO 固件版本可能修改局域网事件字段或路径。如果屏幕已经连接，但始终没有呼号，请从 FMO 网页接口获取脱敏后的 `/events` JSON，并调整 `main/fmo_network.c` 中的 `parse_fmo_message()`。不要公开 Wi-Fi 密码、FMO Secret、证书、私钥或未经脱敏的日志。

## 仍需进行的真机验证

- 确认 Passport 能连接到预期的 2.4 GHz 网络；
- 确认开机及 FMO 端切换频道后，屏幕显示的频道与 FMO 一致；
- 使用电台按下和释放 PTT，检查 `ON AIR`、呼号、主机／网格信息及 `LAST HEARD` 状态切换；
- 让两台设备在 Wi-Fi 中断后继续运行，确认能够自动重连；
- 在真机上检查 USB 日志、最小剩余堆、按键响应、界面裁切和电量显示。


## 可靠性与资源使用

- 网络侧在互斥锁内归并状态，以单槽快照交给界面；即使界面来不及处理每次 PTT 变化，也能收到最终状态。
- 异常断线和正常 CLOSE 握手都会重连。客户端创建失败时，在 Wi-Fi 可用后重试。
- 每秒查询频道，新发言也会触发确认；确认前显示同步状态，避免把发言直接归到旧频道。频道变化时清空旧呼号，频道确认超过 5 秒即失效。
- 局域网通联事件没有频道 UID，两个连接无法提供原子化的频道／发言快照。因此频道归属属于尽力同步：频道切换时清掉无法确定归属的发言，等待后续事件；轮询不能保证瞬时一致。
- 固定大小的文本组装器支持接收分段、连续帧和穿插控制帧；过大或非法帧会使实时发言状态失效。
- 标签内容变化时才更新。未使用的演示源码、BLE 和 LVGL 示例不参与应用构建；Recovery 的 BLE 位于出厂永久固件中。
- 每分钟输出内部堆最低剩余量、最大可分配块、协调任务和 WebSocket 栈余量。电池任务的栈余量以 DEBUG 级别输出。真机测量前保留保守栈大小。

## 验证与安装的区别

`./tools/validate.sh` 执行主机检查和**启用完整网络路径的验证构建**，使用
`tests/sdkconfig.network` 中公开的虚拟配置，产物放在
`build/validation/FoloToy-AI-Passport-full.bin`，不能用作实际安装固件，也不会覆盖用户的配置版镜像。

给自己的设备使用时，执行 `idf.py menuconfig`，然后执行
`./tools/validate.sh --configured`。该命令把本地配置复制到私有临时文件，检查配置并生成
`build/FoloToy-AI-Passport-full.bin`。镜像包含 Wi-Fi 配置，不应公开发布；CI 产物只包含虚拟配置。

本项目基于 [FoloToy/ai-passport](https://github.com/FoloToy/ai-passport)。
接口参考：[FMO 网页客户端 API](https://github.com/niufox/fmo-mobile-controller/blob/main/API_DOCUMENTATION_v2.md)。
