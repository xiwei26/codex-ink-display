# Codex 墨水屏用量看板

为 nRF52811 4.2 英寸墨水屏制作的 Codex 用量看板。它从当前电脑已登录的 Codex 账号自动读取用量，经过 Web Bluetooth 推送到设备显示。

## 显示的数据

- 今日、本月、累计与峰值日 Token
- 最近 7 天 Token 趋势
- 当前和最长连续使用天数
- 5 小时与周额度已用比例

数据来自本机 `codex app-server` 的 `account/usage/read` 与 `account/rateLimits/read`。额度窗口按照接口返回的时长识别（300 分钟为 5 小时、10080 分钟为 7 天），不依赖 `primary`/`secondary` 字段顺序。服务只返回用于显示的数值，不读取或传输 `auth.json`、访问令牌、邮箱或密码。

## 快速使用

### 1. 准备固件

安装 Keil µVision 和 Nordic `nRF_DeviceFamilyPack` 后，打开 [Keil/EPD-nRF52.uvprojx](Keil/EPD-nRF52.uvprojx)，选择 `nRF52811_xxAA`，按 `F7` 编译并通过 J-Link 或 CMSIS-DAP 烧录。

也可使用 Arm GNU Toolchain：在 `SDK/17.1.0_ddde560/components/toolchain/gcc/Makefile.windows` 配置 `GNU_INSTALL_ROOT`，然后运行：

```bash
make -f Makefile.nRF52
```

首次烧录仍需要按硬件调试器流程写入对应的 S112 SoftDevice；仓库不再包含 OTA 或烧录工具。

### 1.1 安装看板固件（推荐：Keil + J-Link）

本项目的固件目标是 **nRF52811_xxAA**，需要 **S112 7.3.0 SoftDevice**。烧录必须使用 SWD 调试接口，不能通过网页蓝牙完成。

#### 准备工具和连线

1. 安装 Keil MDK 5，并在 Pack Installer 安装 `NordicSemiconductor::nRF_DeviceFamilyPack`（工程记录的版本为 8.40.3）。若 Keil 提示编译器不可用，安装并选择 Legacy Arm Compiler 5。
2. 安装 SEGGER J-Link 软件；若希望使用命令行烧录，再安装 Nordic nRF Command Line Tools（提供 `nrfjprog`）。
3. 将 J-Link 与墨水屏板的 SWD 焊盘连接：`SWDIO`、`SWCLK`、`GND`，并让目标板处于 3.0–3.3V 供电状态；`RESET` 可选。不要把 5V 接到 nRF52811。

> 如需保留商家原版固件，先用 J-Link/nRF Connect Programmer 导出备份。`Erase all` 会清除原固件、配置和可能存在的引导程序，通常无需执行。

#### 编译应用程序

1. 打开 [Keil/EPD-nRF52.uvprojx](Keil/EPD-nRF52.uvprojx)。
2. 在工具栏 Target 下拉框选择 **`nRF52811_xxAA`**，不要选择 `flash_softdevice`。
3. 选择 **Project → Rebuild all target files**（或按 `F7`）。
4. 成功后得到 `Keil/_build_nRF52/EPD-nRF52.hex`。

#### 写入 SoftDevice 与应用

**图形界面方式**：在 nRF Connect Programmer 中连接 J-Link，依次添加并写入下面两个 HEX 文件：

1. `SDK/17.1.0_ddde560/components/softdevice/s112/hex/s112_nrf52_7.3.0_softdevice.hex`
2. `Keil/_build_nRF52/EPD-nRF52.hex`

**命令行方式**：确认 `nrfjprog --version` 可运行后，在项目根目录执行：

```powershell
# 首次写入或不确定 SoftDevice 版本时执行
nrfjprog -f nrf52 --program SDK/17.1.0_ddde560/components/softdevice/s112/hex/s112_nrf52_7.3.0_softdevice.hex --sectorerase

# 每次更新看板固件时执行
nrfjprog -f nrf52 --program Keil/_build_nRF52/EPD-nRF52.hex --sectorerase
nrfjprog -f nrf52 --reset
```

也可在已配置好 `GNU_INSTALL_ROOT` 与 `nrfjprog` 的环境中运行：

```powershell
make -f Makefile.nRF52
make -f Makefile.nRF52 flash_softdevice
make -f Makefile.nRF52 flash
```

#### 验证结果

重启设备后，打开本地网页并重新连接。连接状态应显示 **`固件 0x20 · 看板协议`**；随后点击“推送看板”，墨水屏会收到两个紧凑数据包并刷新。若显示 `0x19`，说明设备仍是商家原厂固件，网页会自动改走图片传输兼容模式。

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

页面会读取设备固件版本和面板配置作提示，但**永远不会据此选择看板直传协议**。每次推送都会把本机获得的 Codex 数据绘制成 400×300 黑白位图，然后按商家网页的上传顺序发送：`0x01` 初始化、`0x30` 分包传图、`0x05` 刷新。黑白层首包标志为 `0x0F`、后续包为 `0xFF`；对于型号 `0x02`/`0x03` 的三色屏，还会发送全白的红色层（首包 `0x00`、后续包 `0xF0`），防止未清空的颜色 RAM 造成满屏红色噪点。

因此，即使连接状态显示 `固件 0x20`，也不需要自定义固件或额外选择传输模式；重连后直接推送即可。不要向原厂固件发送 `0x20` 且模式值为 `3`，原厂日历/时钟绘制器不认识该模式，会执行空白刷新。

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
- `0x30`：将 400×300 的黑白位图以 18-byte 数据块写入屏幕 RAM
- `0x05`：刷新屏幕

网页使用 20-byte ATT MTU（1 byte 命令 + 1 byte 图传标志 + 18 byte 像素数据）；120,000 个像素压缩为 15,000 bytes，约 834 个数据包。三色屏会再发送 834 个全白颜色层数据包，以确保红色层被完全清除。

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
- `account/usage/read` 和 `account/rateLimits/read` 的可用字段取决于当前 Codex 版本及账号计划；缺失的额度时长窗口会显示为“不可用”，不会拿另一个时长的额度代替。

## 来源与许可

项目保留原始项目的 [GPL-3.0 许可证](LICENSE)。底层墨水屏驱动与 Nordic SDK 来自 [YCD12/EPD-nRF5_DYC](https://github.com/YCD12/EPD-nRF5_DYC)，本仓库将其精简为 Codex 用量看板用途。
