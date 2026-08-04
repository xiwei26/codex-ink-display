# Codex 墨水屏用量看板

为 nRF52811 4.2 英寸墨水屏制作的 Codex 用量看板。它从当前电脑已登录的 Codex 账号自动读取用量，经过 Web Bluetooth 推送到设备显示。

## 显示的数据

- 今日、本月、累计与峰值日 Token
- 最近 7 天 Token 趋势
- 当前和最长连续使用天数
- 5 小时与周额度已用比例

数据来自本机 `codex app-server` 的 `account/usage/read` 与 `account/rateLimits/read`。服务只返回用于显示的数值，不读取或传输 `auth.json`、访问令牌、邮箱或密码。

## 快速使用

### 1. 准备固件

安装 Keil µVision 和 Nordic `nRF_DeviceFamilyPack` 后，打开 [Keil/EPD-nRF52.uvprojx](Keil/EPD-nRF52.uvprojx)，选择 `nRF52811_xxAA`，按 `F7` 编译并通过 J-Link 或 CMSIS-DAP 烧录。

也可使用 Arm GNU Toolchain：在 `SDK/17.1.0_ddde560/components/toolchain/gcc/Makefile.windows` 配置 `GNU_INSTALL_ROOT`，然后运行：

```bash
make -f Makefile.nRF52
```

首次烧录仍需要按硬件调试器流程写入对应的 S112 SoftDevice；仓库不再包含 OTA 或烧录工具。

### 2. 启动本机看板服务

前提：已安装并登录 Codex，且 `codex --version` 可运行；Windows 上还需要 Python 3。

```bash
python local_server.py
```

服务默认只监听 `127.0.0.1:8765`。用 Chrome 或 Edge 打开 <http://127.0.0.1:8765/html/>：

1. 点击“刷新 Codex 数据”。
2. 点击“连接 NRF_EPD”，在浏览器选择设备。
3. 点击“推送看板”。

Web Bluetooth 仅在 HTTPS 或 localhost 下可用，因此不要直接双击打开 HTML 文件，也不要使用 `python -m http.server` 替代本项目的本机服务。

### 原厂与看板固件兼容性

页面连接后会读取设备固件版本并自动选择传输方式：

- **商家原厂固件（例如 `0x19`）**：将看板绘制成 400×300 黑白位图，使用商家网页验证过的 `0x30` 图片传输和 `0x05` 刷新命令发送，无需刷写自定义固件。
- **本仓库看板固件（`0x20`）**：使用紧凑的 `0x22` 看板数据包，传输更快。

因此，若设备此前被推送后全白，请重新连接并再次推送：新版网页会自动走原厂图片协议。不要向原厂固件发送 `0x20` 且模式值为 `3`，原厂日历/时钟绘制器不认识该模式，会执行空白刷新。

### 常见问题

- **“未找到 codex 命令”**：确认 `codex --version` 可运行；若没有，请先安装 Codex 并重新打开终端。
- **“Codex 请求失败”或显示“不可用”**：在 Codex 中重新登录 ChatGPT 账号后刷新页面。API Key 模式不会返回 ChatGPT 用量与额度。
- **无法连接设备**：使用 Chrome 或 Edge，确认页面地址是 `127.0.0.1`/`localhost`，并打开电脑蓝牙。

## 工作原理

```text
浏览器页面 ──HTTP(仅 127.0.0.1)──> local_server.py
                                         │
                                         └──> codex app-server ──> 当前登录的 Codex 账号
浏览器页面 ──Web Bluetooth──> NRF_EPD 墨水屏
```

`local_server.py` 会为每次刷新临时启动 `codex app-server`，先完成 JSON-RPC 初始化，再依次读取账户状态、Token 活动和额度窗口。它不解析 `~/.codex/auth.json`，也不会将认证信息暴露给网页或 BLE 设备。

## BLE 看板协议

服务端页面使用固定的 EPD 自定义服务：

- 服务：`62750001-d828-918d-fb46-b6c11c675aec`
- 特征：`62750002-d828-918d-fb46-b6c11c675aec`
- `0x01`：初始化屏幕驱动
- `0x20`：同步显示时间
- `0x22, 0x00`：摘要包，依次为今日/月度 Token、当前/最长连续天数、5h/周额度已用比例、累计/峰值日 Token
- `0x22, 0x01`：7 天趋势包，到达后触发一次屏幕刷新

为兼容 20-byte ATT MTU，Token 以 `0.1M` 传输，额度比例以 `0.01%` 传输。过大的数值在设备传输前会安全截断为协议上限。

## 项目目录

```text
EPD/             BLE 服务、墨水屏驱动与看板协议
GUI/             看板绘制和字库
Keil/            nRF52811 Keil 工程
SDK/             Nordic nRF5 SDK 17.1（构建依赖）
html/            本机网页与 Web Bluetooth 控制台
local_server.py  localhost 服务和 Codex app-server 适配器
main.c           nRF52811 固件入口
```

## 限制

- 仅支持当前已用 ChatGPT 登录的 Codex 账号；API Key 模式通常没有 ChatGPT 额度数据。
- Codex 接口不提供每个请求的美元费用或 Claude 用量，因此看板不显示这些字段。
- `account/usage/read` 和 `account/rateLimits/read` 的可用字段取决于当前 Codex 版本及账号计划；缺失字段会显示为 0 或“不可用”。

## 来源与许可

项目保留原始项目的 [GPL-3.0 许可证](LICENSE)。底层墨水屏驱动与 Nordic SDK 来自 [YCD12/EPD-nRF5_DYC](https://github.com/YCD12/EPD-nRF5_DYC)，本仓库将其精简为 Codex 用量看板用途。
