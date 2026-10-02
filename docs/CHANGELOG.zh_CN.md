<p align="right">
  <strong>简体中文</strong> · <a href="CHANGELOG.md">English</a>
</p>

# Changelog

## Unreleased

- 协调整页守听界面的间距：拉开连接状态与频道栏的可见空隙，统一频道／配置面板间隔及呼号／底部对齐。音频状态、音量与长按 OK 配网提示同时显示，刷新请求移到连接状态行。验证发言、上次通联、初始守听、错误及原生界面的布局／内存，不重新制作固件。

- 接收 FMO `/audio` WebSocket 的通联 PCM 并通过 ES8311 扬声器播放，连接后默认 50% 音量自动播放；增加有界缓存、静音／断线清理、恢复前静音 DMA 填充及故障隔离。上下键调音量、短按 OK 开关音频、长按上键刷新，长按 OK 保留网络菜单；底部显示音频状态及音量。验证本机接口、PCM 分帧、播放任务生命周期、按键及原生界面；本次仅改代码，未制作固件或验证真机声音。

- 顶部时间按屏幕水平居中；不显示网格信息时，将空行空间分配给四行电台配置，呼号下移并缩小底部留白。配网、初始守听及错误提示保留显示空间；仅验证原生界面布局，不重新制作固件。

- FMO 与电量之间增加北京时间分钟时钟。Wi-Fi 联网后通过 SNTP 后台校时，重连复用同一服务；时间有效前显示 `--:--`，唤醒前重绘时钟。验证分钟／跨午夜切换、未校时、重连初始化和界面布局／内存。

- 发言及上次通联界面不再显示网格定位码，保留四行电台配置、粗体呼号及连接／配网错误提示。

- 参考提供的界面增加竖屏四行电台配置面板：设备名称、MHz 频率、天线型号和高度（米）。连接后及每 30 秒读取 FMO 本机物理配置；可选配置独立于发言／频道状态，处理未设置及无效值，断线后清空。验证单位转换、中文文本、长信息行、重连和原生界面的内存／布局。

- 已有有效频道时，发言开始或切换呼号立即显示发言状态，频道在后台确认；丢弃早于新发言的响应时保留原有效频道，不延长确认时间。未确认频道、断线和查询失败仍按同步／恢复流程处理；增加重复开始、短 PTT 和迟到响应的解析回归测试。

- 守听信息行只显示网格，去掉“几秒前”和电台／本机前缀。呼号改用获许可的 Montserrat Bold 32 px 字体，长呼号使用 20 px 粗体；配网密码保持常规字重，并验证原生渲染。

- 显式 PTT 结束通知（`isSpeaking: false/0`）保留已确认频道和上次通联显示，兼容空、null 或省略的呼号，忽略不使用的结束元数据。保留具名结束通知的呼号匹配及无效发言开始校验；增加结束状态、重复通知和迟到响应的解析／状态／界面回归测试。

- 修复双栈 HTTP 服务中 IPv4 映射 IPv6 的热点地址检查，使手机可以正常打开配网页。继续拒绝从站点接口访问，并增加真实 socket 回归测试。

- 频道查询超时或发送不完整时重建控制连接，防止旧响应继承新查询状态。网络启动失败后保留任务，清理部分 Wi-Fi 初始化资源，并支持菜单立即重试。
- 拒绝 JSON 转义 NUL 和超长呼号／网格字段；增加可控制时钟的查询、启动恢复和配网地址故障测试。
- 配网页增加可保存的 FMO 主机名／IPv4 和端口，以及使用已保存 Wi-Fi 的后台 DNS／TCP 端口检查。保留会话令牌和热点访问限制，明确区分端口可达与接口兼容。
- 无损拆分原有字库，使用紧凑字形描述，保留全部现有字形和位图，不改变分区。仅规范化私有构建配置的临时副本，并验证帧缓冲一致与字形覆盖。

- 调整扫码配网页的二维码位置，使其与上方标题、下方浏览器提示的可见间距一致。

- 增加类似 PDKPASS 的网络菜单（配网在重连上方）、连接热点二维码、短按 OK 切换热点信息和手动重连。长按可取消配网，恢复当前已保存网络，不保存未验证密码，不恢复已删除网络；增加按键、二维码转义、唤醒和原生渲染内存测试。
- 每次完整安装固件后清除所有应用数据，同版重刷也生效：清除 Wi-Fi、旧 FMO 配置及 PDKPASS 缓存和提醒。保留设备身份、Recovery 和同次安装重启后的数据。完整包携带安装重置请求，清理成功后才写入固件标记；中断后下次启动重试，联网前完成清理，并校验重置请求已打包。

- 使用经过验证的 UTF-8 保留中文频道名，超长文本按完整字符截断。非法实时消息安全清除发言状态，旧频道响应不再清除新发言者。
- 空闲 30 秒调暗、90 秒使 LCD 休眠。保持 FMO 联网与按键扫描；发言或第一次按键唤醒并重绘，首次按键不触发其他功能。配网和持续发言保持亮屏，熄屏暂停电量轮询，面板切换失败可重试。

- 配网热点改用 `192.168.9.1/24`，减少与家用局域网网段冲突。切换凭据前等待
  Wi-Fi 站点停止事件，防止上一次连接的迟到事件误判为新凭据验证成功。启动时记录
  LVGL 内存池用量，原生界面测试检查重绘余量。配网页面区分认证失败、找不到网络、
  安全模式不兼容和 DHCP 超时。新增 macOS 串口日志采集工具，仅用于真机诊断，
  不烧录或发送命令。

- 电量启动后立即读取一次，后续刷新间隔由 30 秒改为 2 分钟，FMO 状态刷新频率不变。

- 电池胶囊由 44x22 缩小为 32x16 像素，保留内部百分比数字和顶部垂直居中。

- 状态和操作提示改为中文，BAT 文字改为 iOS 风格的电池胶囊图标，内部显示百分比数字；
  20% 及以下为红色，读数不可用时显示轮廓，不根据电量猜测充电状态。

- 按可见字形修正频道和呼号区域的上下居中，兼顾中文字库留白、长呼号缩小字号
  和配网状态切换。

- 主界面改为 FMO 风格黑橙白竖屏布局，放大当前发言人呼号，支持中文频道字形，
  配网界面同步配色；增加原生 LVGL 渲染测试和帧缓冲预览。

- 支持五组 Wi-Fi、旧配置迁移、主动删除和有重试间隔的自动切换；优先上次成功网络，
  正常连接不主动换网，Wi-Fi 断线后重新创建 FMO 连接。

- 新增加密热点手机配网、附近 Wi-Fi 选择、连接验证后保存 NVS、长按 OK 重新配网、
  默认 fmo.local mDNS 解析，以及 `./tools/validate.sh --setup` 无凭据安装固件构建。

- 修复配置版固件打包，隔离虚拟网络配置验证产物。
- 支持正常关闭重连和客户端创建重试；发布完整状态快照，频道过期失效、切换清空呼号。
- 支持 WebSocket 分片消息，减少未变化标签的分配，移除多余演示构建输入，加入堆／栈诊断和回归测试。

- 将参考演示菜单替换为 FMO 实时通联显示应用。固件连接局域网 FMO 的 `/events` 和 `/ws` WebSocket 接口，显示当前频道、正在发言或最近发言的呼号、连接状态和电量，并支持背光调节与手动刷新频道；Wi-Fi 和 FMO 私密配置通过项目 Kconfig 写入未跟踪的本地配置，不进入仓库。

- 加入厂家为优特利 520mAh 电芯生成的 80 字节 CW2017 profile，并实现内容与更新标志检查、写入后校验、规定的重启时序以及有上限的 SOC 就绪等待。

- 按功能域整理文档并采用双入口：根目录 `AGENTS.md` 变为薄路由（只保留硬约束与任务路由），详细的 AI 开发工作流下沉到 `docs/development/ai-guide.md`，`agent-guide.md` 并入其中。为 `docs/development/` 增加二级分区（`engineering/`、`ci/`、`release/`），把 `plays/` 应用档案与 `experiences/` 移入带专属 README 的 `docs/reference/` 参考区；删除 `docs/software-design/`（空脚手架）；把 `assets/{fonts,images,music}/README` 三个叶子 README 并入 `assets/` README；把 `project-completion` 的六个子文档压平为单文件；并把每个目录统一为单一 README，消除所有 `INDEX` 文件与一处重复经验索引。所有交叉引用与文献链接已更新；未丢弃任何内容。

- 将小程序 BLE 安装兼容提升为二创模板强制契约：固定保护 `cardid`/Recovery 分区，
  保留上键持续 5 秒进入 Recovery 的 bootloader hook，并在 CI 强制校验合并镜像结构、
  分区表 MD5/范围、3 MB 应用上限和保护分区数据不入包。
- 规定多应用发布的 Release 标题约定：tag 按 `v<版本>-<应用名>`（如 `v0.1.0-voice-keychain`）命名，让 Release 标题同时带版本与应用名；发布成功后核对标题，保证一眼扫 Release 列表就能区分是哪个应用。
- 新增发布后收尾流程：`issue-suggestions` skill 用于把用户反馈作为 issue 提交到上游项目；`experience-pr` skill 用于把可复用的开发经验作为文档 PR 提交；新增 `docs/experiences/` 目录保存单条经验文件；并配套 `project-completion`、`file-issues` 与经验索引文档。
- 精简仓库根目录：将 GitHub 可识别的社区治理文档迁入 `.github/`，将变更记录迁入 `docs/`，同步全部引用，并在仓库检查中加入根目录文档白名单。
- 全仓库文档语言规范：所有维护中的 Markdown 默认 `.md` 文件使用英文，简体中文使用配对的 `.zh_CN.md`，双方提供语言切换；静态检查会阻止缺失配对、缺失切换链接或英文默认页混入中文正文。
- AI 开发流程一期：精简按任务加载的上下文入口，统一本地/CI 验证脚本，新增 PR 自动构建与模板，并提交依赖锁文件以提高构建可复现性。
- PR 审查修复：GitHub Actions 固定到完整 commit SHA，构建与发布 job 按最小权限拆分，同步 checkout 关闭凭证持久化；补充 Feature Request / Usage Question issue 表单；启用并修正私密安全报告兜底说明；清理 README 路径、CI 触发条件与历史分支描述漂移。
- 语言规范变更：commit 标题、PR 标题与 body 由"默认中文"改为**使用英文**（`docs/contribution/commit-and-pr.md` 更新）；中文写作规范（全角标点）适用范围剔除 PR/MR 描述（`doc-conventions.md` 更新）。
- CI 构建改造：`build-firmware.yml` 显式传入 `SDKCONFIG_DEFAULTS=sdkconfig.defaults` 再 `idf.py build`，由 defaults 启用自定义分区表（`CONFIG_PARTITION_TABLE_CUSTOM=y`，文件名为 `partitions.csv`）；`CONFIG_ESPTOOLPY_HEADER_FLASHSIZE_UPDATE` 改为 `n`，再用 `idf.py merge-bin -o build/FoloToy-AI-Passport-full.bin` 合并可直刷完整固件；产物精简为仅 full.bin；`actions/cache` 升级到 v5 以消除 GitHub Actions Node.js 20 弃用警告；CI 文档同步更新。
- 合并上游 PR #6（wireless-low-power-demos）以解决 PR #4 冲突：引入无线/低功耗 demo（`main/demo_wifi.c`、`demo_ble.c`、`demo_radio.c`、`demo_low_power.c`）、`partitions.csv`（NVS/PHY/3 MB factory-app 分区）、`main/CMakeLists.txt`/`main.c`/`demo.h`/`sdkconfig.defaults` 更新；同步硬件指南的 Wi-Fi/BLE/低功耗章节；README 能力契约表补充 Wi-Fi/Bluetooth LE/Low power 三项（中英双语）。
- 提交规范补充：`docs/contribution/commit-and-pr.md` 明确 PR 标题与 commit 标题使用相同的 Conventional Commit 格式和英文祈使句，不用名词短语当标题。
- CI 与文档清理：`sync-main.yml` 移除 `test_mode` 残留模板注释；`docs/development/coding-conventions.md` 将「Redis TTL」条目泛化为「缓存组件」条目（当前固件无 TTL 约束需求，消除从模板带入的无关约定）。
- 补充通用规范（借鉴 Shinku）：`docs/contribution/doc-conventions.md` 新增中文全角标点规范（正文 `，`；`（`）`，代码/命令/路径保留英文原样）、凭证不入仓规范（token/密钥/私钥绝不入仓，提交前 git diff 扫描敏感前缀）、文件删除安全规范（删除走系统回收站，不用 rm -rf/git clean -fd）。
- 代码注释规范强化：`docs/development/coding-conventions.md` 补充完善注释要求——函数说明（用途/参数/返回值/副作用/线程上下文/内存所有权/初始化顺序）、变量说明（语义/取值范围/生命周期/同步要求）、逻辑注释（状态机/时序/寄存器/魔数依据），覆盖范围宁多勿少，中文注释保留英文技术术语。
- 文档去 AI 化：`docs/README.md` / `docs/README.zh_CN.md` 移除 AI 专属章节（Entry point、Source-of-truth、提需求格式、BSP 边界、Runtime invariants、验收交付格式、构建命令），README 只保留给人看的项目介绍、硬件能力契约、demo 案例与项目结构；构建命令章节删除（与 `docs/development/build-and-test.md` 重复）。
- 新增 `docs/development/agent-guide.md`：集中承载"AI 如何在本仓库工作"（上下文建立顺序、事实来源优先级、提需求格式、BSP 边界、运行时规则、交付格式），并链接 build-and-test 与硬件指南，不重复构建命令与验收矩阵。
- 同步更新索引：`AGENTS.md` 规则索引新增 agent-guide 条目；`docs/INDEX.md` 与 `docs/development/README.md` 新增 agent-guide 索引行。
- 文档补充：`docs/fork-guide.md` 说明「为什么根目录不放置 README」——根目录 README 预留给 fork 开发者自行放置（上游留空），fork 后可将自己的内容写入根目录 `README.md` 介绍 fork 后的项目；GitHub 显示优先级（根 README > docs/README.md）契合该预留意图。
- 分支合并：创建 `main-update` 分支（基于与上游一致的 main），将 `feature/repo-structure`、`ci/build-firmware`、`ci/sync-main` 三个分支合并进来，统一 docs 结构（CI 文档归入 `docs/development/`，workflow 文件随 ci 分支引入 `.github/workflows/`）；解决 development/software-design README 的 add/add 冲突。
- 合并后审查修复：`docs/INDEX.md` 补充 CI 文档索引；`docs/fork-guide.md` 修正 workflow 引用为 `.github/workflows/sync-main.yml`；`docs/README` 双语项目结构块补充 `.github/workflows/` 与 CI 文档说明。
- ci 分支 CI 文档路径调整：`ci/build-firmware` 的 `docs/software-design/CI-build-and-release.md` 与 `ci/sync-main` 的 `docs/software-design/CI-sync-main.md` 均移入各分支的 `docs/development/`（CI 属工程规范）；`docs/software-design/README.md` 保留为软件设计索引；feature 分支的 software-design 索引同步更新引用。
- fork 补充文档目录迁移：`assets/docs/` 移至 `docs/assets/`（文档素材归入 docs/ 更合理），新增 `docs/assets/.gitkeep` 空目录占位；同步更新 AGENTS.md / INDEX / doc-conventions / fork-guide 的路径引用。
- 文档结构调整：根目录不再放 README——上游英文 README 移入 `docs/README.md`、中文移入 `docs/README.zh_CN.md`（GitHub 从 docs/ 识别主 README）；原 `docs/README.md` 根总索引更名为 `docs/INDEX.md`；同步更新 AGENTS.md / CONTRIBUTING / SUPPORT / fork-guide / doc-conventions 的路径引用。
- 初始化项目文档：新增 `AGENTS.md`、`CLAUDE.md` 和 `CHANGELOG.md`。
- 仓库结构规整：上游英文 `README.md` 更名为 `README.en_US.md`，保留 `README.zh_CN.md`。
- 新增目录骨架：`docs/`（software-design / hardware-design）、`assets/`（fonts / images / music，各含 `README.md`）、`skills/`。
- 将上游硬件开发指南归位到 `docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md`。
- 文档规范：子目录 readme 统一为大写 `README.md`；补充 fork 用户约定（main 只动根 README）。
- 扩展 fork 用户约定：`main` 分支允许修改根目录 `README.md` 和 `assets/docs/`（README 不足以说明项目时存放补充文档与素材）。
- 新增 `assets/docs/` 目录约定：上游 main 只保留空目录 `.gitkeep`，内容文件仅存在于 fork；使用方法规范写入 AGENTS.md「给 fork 用户」约定。
- CI 文档迁移：`docs/software-design/CI.md` 从本分支移除，迁至 `ci/build-firmware` 分支并改名为 `docs/software-design/CI-build-and-release.md`。
- 补充 `main` 分支策略说明：解释 `main` 保持干净的两大原因（与上游同步无冲突 + 多小项目按分支整理）；例外——执意 main 开发需停用 CI 自动同步；提醒 fork 用户默认 action 关闭需手动启用（此条为整个 CI 的通用要求，统一写入 AGENTS.md）。
- 文档拆分：将 `AGENTS.md` 按主题拆为公共文档——新增 `docs/contribution/`（doc-conventions.md、commit-and-pr.md）与 `docs/development/`（build-and-test.md、coding-conventions.md），新增 `docs/fork-guide.md`；`AGENTS.md` 精简为简介 + 项目概述 + 必读文档索引。
- 同步更新索引：`docs/software-design/README.md`、`README.en_US.md` / `README.zh_CN.md` 的 `docs/` 目录说明。
- 参考 cindy 仓库文档组织完善索引：新增 `docs/README.md` 根总索引；AGENTS.md 规则索引按触发场景改写（附触发条件）；`docs/contribution/` 与 `docs/development/` 的 README 补充收录标准。
- 引入社区治理文档（参照 cindy 改写，放仓库根目录）：新增 `CONTRIBUTING.md` / `.zh_CN.md`（贡献指南，针对 ESP-IDF/AI agent/fork 场景改写）、`CODE_OF_CONDUCT.md` / `.zh_CN.md`（贡献者公约）、`SECURITY.md` / `.zh_CN.md`（安全报告流程）、`SUPPORT.md` / `.zh_CN.md`（支持渠道）；AGENTS.md 与 docs/README.md 同步引用。
