<p align="right">
  <strong>简体中文</strong> · <a href="build-and-test.md">English</a>
</p>

> FMO 分支：`--firmware` 使用公开虚拟网络配置，仅生成
> `build/validation/FoloToy-AI-Passport-full.bin`。私人安装镜像请先运行
> `idf.py menuconfig`，再运行 `./tools/validate.sh --configured`，输出仍为
> `build/FoloToy-AI-Passport-full.bin`。CI 仅上传验证产物，已关闭自动发布 Release。
> 详情参见根目录 README。无预置凭据的可安装版本使用 `./tools/validate.sh --setup`。
> 紧凑字库保留全部现有字形，要求关闭 `CONFIG_LV_FONT_FMT_TXT_LARGE`；
> 私有构建仅规范化配置的临时副本，不更改原配置。

# 构建与验证（Build & Test）

使用 ESP-IDF 5.5.3。全新机器或缺少工具链时，先按
[环境引导](environment-setup.zh_CN.md)完成安装。

> 可安装固件使用 `./tools/validate.sh --setup`，烧录优先把验证通过的
> `build/FoloToy-AI-Passport-full.bin` 写入空白设备；对已有身份的设备，只有合并文件
> 在保护区 `cardid` 之前结束时才可从 `0x0` 直刷，其余情况优先用小程序或分段
> `idf.py flash`。`idf.py build` 和
> `idf.py flash` 只作为增量开发命令，不作为默认交付方式。

```bash
source <ESP-IDF-v5.5.3-路径>/export.sh
idf.py --version             # 必须输出 ESP-IDF v5.5.3
./tools/validate.sh --setup    # 编译并验证可安装的合并固件
idf.py set-target esp32c3     # 配置目标芯片（fresh checkout 后/换 target 后运行）
idf.py build                  # 可选：增量 app 编译
idf.py flash monitor          # 可选：增量 app 烧录
idf.py fullclean              # 只清空过期生成状态（勿用于清理用户源码改动）
```

`idf.py fullclean` 不能让已有 `sdkconfig` 完整同步变更后的 defaults。需要重建
target 或已跟踪 defaults 时，先保留有意的本地设置，再运行
`idf.py set-target esp32c3`。

仓库提交 `dependencies.lock` 以固定 ESP-IDF Managed Components 的解析结果。修改 `idf_component.yml` 后必须使用 ESP-IDF 5.5.3 重新生成锁文件、review 版本变化并与 manifest 一起提交；普通构建不应产生未提交的锁文件差异。

固件门禁使用全新的临时构建目录，并从仓库 `sdkconfig.defaults` 生成隔离的 `sdkconfig`。它不会读取或覆盖开发者根目录的 `sdkconfig`，将验证镜像放到 `build/validation/FoloToy-AI-Passport-full.bin`；`--setup`／`--configured` 的安装镜像放到 `build/FoloToy-AI-Passport-full.bin`。门禁同时强制检查[小程序 BLE 兼容契约](ble-recovery-compatibility.zh_CN.md)：保护分区地址、应用大小、分区表 MD5、保护区数据不入包，以及 Recovery bootloader hook。

当前基线含一个可独立运行的纯逻辑测试：

```bash
cc -std=c11 -Wall -Wextra -Werror -Imain \
  tests/test_ui_pixel_math.c main/ui_pixel_math.c \
  -o /tmp/test_ui_pixel_math
/tmp/test_ui_pixel_math
```

统一验证入口：

```bash
./tools/validate.sh --static    # 仓库、workflow、文档、敏感信息及主机测试（含 Node.js 配网页面测试）
./tools/validate.sh --firmware  # ESP-IDF build、merge-bin、BLE 兼容及原生界面测试
./tools/validate.sh             # 完整验证
```

完整验证要求预先激活 ESP-IDF 5.5.3。CI 与本地使用同一脚本；若 CI 和本地行为不同，应先修复脚本或环境，而不是维护两份命令。

涉及物理外设的改动必须在真机运行硬件指南验收清单，并把“编译通过”与“硬件验证通过”分开记录。

固件检查使用 ESP-IDF 的真实 cJSON 测试生产网络解析函数；未激活 ESP-IDF 时，
`--static` 会跳过此项。主机检查覆盖 UTF-8 边界，以及用硬件替身验证 LCD 切换与
唤醒手势顺序。依赖检查使用已提交的锁定版本，不检查可选的新版本。
这些检查不能代替真机唤醒和电流测量。

安装重置主机测试模拟首次安装、同版重刷、正常重启、固件变更、清理／标记写入
中断和非法分区布局；检查受保护字节不变，且清理早于联网。完整镜像检查要求包含
安装重置资源载荷；仍需在真机验收安装和重启行为。

## macOS 真机日志采集

采集工具只依赖 Python 3。先开始采集，再重启 Passport，以记录完整的启动和重连过程：

```bash
python3 tools/device-test/serial_capture.py --list-ports
python3 tools/device-test/serial_capture.py --seconds 300
# 如果有多个 USB modem，请指定设备端口：
python3 tools/device-test/serial_capture.py --port /dev/cu.usbmodemXXXX --seconds 300
```

工具不会烧录或发送命令。原始日志以私有权限保存在 `/tmp/fmo-device-logs/`，终端
汇总启动、界面内存、Wi-Fi、WebSocket 和错误事件。分享前请检查并遮盖敏感内容。

社区只能上传验证通过的 `build/FoloToy-AI-Passport-full.bin`，不得上传应用单镜像
`build/FoloToy-AI-Passport.bin`，后者没有小程序可安全解析与转换的完整结构。

通用可安装固件使用 `./tools/validate.sh --setup`；`--firmware` 只生成
`build/validation/FoloToy-AI-Passport-full.bin` 验证镜像。`--setup` 和 `--configured`
将可安装镜像放到 `build/FoloToy-AI-Passport-full.bin`。紧凑字库保留全部现有字形，
要求关闭 `CONFIG_LV_FONT_FMT_TXT_LARGE`；私有构建仅规范化配置的临时副本。
直接使用 `idf.py build` 的旧配置须先关闭该选项。

完整检查覆盖频道查询超时与迟到响应、部分初始化失败后的重试、地址校验和保存，以及
注入系统调用结果的 DNS／TCP 检查。原生 UI 覆盖字体回退区间；字库主机检查逐字形
比较度量与压缩字节。真实局域网连通性和电流仍须真机验证。
