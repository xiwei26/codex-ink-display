# Codex 墨水屏用量看板

为 nRF52811 4.2 英寸墨水屏制作的 Codex 用量看板。它从当前电脑已登录的 Codex 账号自动读取用量，经过 Web Bluetooth 推送到设备显示。

## 显示的数据

- 今日、本月、累计与峰值日 Token
- 最近 7 天 Token 趋势
- 当前和最长连续使用天数
- 5 小时与周额度已用比例

数据来自本机 `codex app-server` 的 `account/usage/read` 与 `account/rateLimits/read`。额度窗口按照接口返回的时长识别（300 分钟为 5 小时、10080 分钟为 7 天），不依赖 `primary`/`secondary` 字段顺序。服务只返回用于显示的数值，不读取或传输 `auth.json`、访问令牌、邮箱或密码。

## 快速使用

**无需刷机。** 本项目直接使用设备出厂固件提供的蓝牙图片上传协议，不需要 Keil、J-Link、SoftDevice 或 SWD 接线。只要设备能通过商家原网页连接并上传图片，就可以直接使用本看板。

### 1. 准备电脑环境

- 安装并登录 Codex，确认 `codex --version` 可以运行。
- 安装 Python 3。
- 使用支持 Web Bluetooth 的 Chrome 或 Edge。

### 2. 启动本机看板服务

```bash
python local_server.py
```

服务默认只监听 `127.0.0.1:8765`。用 Chrome 或 Edge 打开 <http://127.0.0.1:8765/html/>：

1. 点击“刷新 Codex 数据”。
2. 点击“连接 NRF_EPD”，在浏览器选择设备。
3. 点击“推送看板”。

Web Bluetooth 仅在 HTTPS 或 localhost 下可用，因此不要直接双击打开 HTML 文件，也不要使用 `python -m http.server` 替代本项目的本机服务。

### 原厂固件兼容性

页面只使用原厂固件已有的图片上传能力，不发送自定义看板指令。每次推送都会把本机获得的 Codex 数据绘制成 400×300 黑白位图，然后按商家网页的上传顺序发送：`0x01` 初始化、`0x30` 分包传图、`0x05` 刷新。黑白层首包标志为 `0x0F`、后续包为 `0xFF`；对于型号 `0x02`/`0x03` 的三色屏，还会发送全白的红色层（首包 `0x00`、后续包 `0xF0`），防止未清空的颜色 RAM 造成满屏红色噪点。

仓库中的 nRF52 固件、驱动和 Keil 工程仅保留为上游协议实现参考，运行看板时不会编译或写入这些文件。请保留设备原厂固件；刷新网页或更新本项目也不会改写设备固件。

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
Keil/            上游 nRF52811 工程（仅作协议参考）
SDK/             上游 Nordic nRF5 SDK（仅作协议参考）
html/            本机网页与 Web Bluetooth 控制台
local_server.py  localhost 服务和 Codex app-server 适配器
main.c           上游 nRF52811 固件入口（仅作协议参考）
```

## 限制

- 仅支持当前已用 ChatGPT 登录的 Codex 账号；API Key 模式通常没有 ChatGPT 额度数据。
- Codex 接口不提供每个请求的美元费用或 Claude 用量，因此看板不显示这些字段。
- `account/usage/read` 和 `account/rateLimits/read` 的可用字段取决于当前 Codex 版本及账号计划；缺失的额度时长窗口会显示为“不可用”，不会拿另一个时长的额度代替。

## 来源与许可

项目保留原始项目的 [GPL-3.0 许可证](LICENSE)。底层墨水屏驱动与 Nordic SDK 来自 [YCD12/EPD-nRF5_DYC](https://github.com/YCD12/EPD-nRF5_DYC)，本仓库将其精简为 Codex 用量看板用途。
