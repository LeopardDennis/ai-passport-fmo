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

最多保存五组验证成功的 Wi-Fi，自动导入旧版单网络配置。相同 SSID 的密码只有在新连接
成功后才更新；第六个不同网络会被拒绝，需要先删除一个。配网页面只显示已保存名称，
不返回密码，删除前有确认，并提供不添加网络直接完成的按钮。删除全部网络后保留配网
入口，不会恢复旧版凭据。一次配网结束后，可再次长按 OK 添加其他网络。

开机优先尝试上次成功的网络。断线后，每次连接最多等待 25 秒，再按扫描信号强度尝试
其他已保存网络；未扫描到或隐藏网络也会尝试。全部失败后等待 30 秒再开始下一轮。
正常联网时不会为了更强信号或因为 FMO 不可用而换网。Wi-Fi 断线会关闭旧 FMO 连接，
联网后为新局域网重新创建连接；新网络中也需要能访问 FMO。只有首选网络改变时才写入
NVS，不会每次重连都写 Flash。支持 WPA2 及以上个人网络和开放网络，不支持企业认证。

无预置凭据的固件首次启动会开启加密热点。手机连接屏幕显示的
`FMO-Setup-XXXX`，输入同屏显示的随机热点密码。即使手机提示没有互联网，
也请保持连接，并手动打开 `http://192.168.4.1`。选择附近的 2.4 GHz Wi-Fi，
或手动输入隐藏网络名称，再填写密码提交。列表在每次进入配网时扫描一次，
连接失败可直接重试。

固件最多等待 25 秒验证 Wi-Fi 和 DHCP，成功后才保存到独立的 `fmo_wifi`
NVS 命名空间；失败时不会主动删除原有凭据。成功后关闭热点及网页服务，
通过 mDNS 解析并连接 `fmo.local:80`。两台设备需要处于允许组播和设备互访的局域网。

正常运行时长按 **OK** 会重启进入配网；旧路由器不可用时也可使用。新连接成功前
保留旧配置。配网期间忽略长按 OK；若配网服务启动失败，可关机重试。
本版本不提供自动弹出的认证门户，也不假定其他 Passport 固件的密码可以共享。
网页只允许从配网热点访问，提交使用随机会话令牌，不会返回已保存的 Wi-Fi 密码。
凭据保存在普通 NVS 中，未启用加密存储。

高级用户仍可通过 `idf.py menuconfig` → **FMO Live Monitor** 设置编译期备用
Wi-Fi、FMO 地址／端口和亮度。设备保存的 Wi-Fi 优先；不要公开包含密码的固件。

## 构建和安装

激活 ESP-IDF 5.5.3，然后执行：

```bash
./tools/validate.sh --static
./tools/validate.sh --setup
```

可安装的合并固件为 `build/FoloToy-AI-Passport-full.bin`。必须保留受保护的 `cardid` 和 Recovery 分区。对于已经完成出厂配置的设备，优先使用 AI Passport 小程序安装；不要擦除整片 Flash。

## 按键

- **上键**：背光增加 10%；
- **下键**：背光降低 10%；
- **确定键**：立即刷新当前 FMO 频道。
- **长按确定键**：重启进入 Wi-Fi 配网，不删除原有凭据。

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

无需预置密码的通用安装包使用 `./tools/validate.sh --setup` 构建，仅读取仓库默认配置，
忽略私密的 `sdkconfig`，输出 `build/FoloToy-AI-Passport-full.bin`，安装后使用手机配网。

`./tools/validate.sh` 执行主机检查和**启用完整网络路径的验证构建**，使用
`tests/sdkconfig.network` 中公开的虚拟配置，产物放在
`build/validation/FoloToy-AI-Passport-full.bin`，不能用作实际安装固件，也不会覆盖用户的配置版镜像。

给自己的设备使用时，执行 `idf.py menuconfig`，然后执行
`./tools/validate.sh --configured`。该命令把本地配置复制到私有临时文件，检查配置并生成
`build/FoloToy-AI-Passport-full.bin`。镜像包含 Wi-Fi 配置，不应公开发布；CI 产物只包含虚拟配置。

本项目基于 [FoloToy/ai-passport](https://github.com/FoloToy/ai-passport)。
接口参考：[FMO 网页客户端 API](https://github.com/niufox/fmo-mobile-controller/blob/main/API_DOCUMENTATION_v2.md)。
