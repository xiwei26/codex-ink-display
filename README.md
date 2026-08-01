# Codex 墨水屏用量看板

一个为 nRF52811 4.2 英寸墨水屏制作的 Codex 用量看板。固件通过 BLE 接收用量数据，在屏幕上显示今日/本月 Token、请求数、费用、Codex 与 Claude 的拆分，以及近 7 日趋势。

本项目从商家固件中仅保留看板所需的 nRF52、墨水屏驱动和 Web Bluetooth 控制页；图片传输、日历/时钟、nRF51、OTA、模拟器及历史文档均已移除。

## 效果与功能

- 针对 400 × 300 的 4.2 英寸墨水屏布局。
- 通过网页蓝牙填写并发送数据，无需重新生成位图。
- 两个数据包均不超过旧设备的 20-byte ATT MTU；仅第二包到达时刷新一次，减少闪屏。
- 使用当前浏览器时区写入刷新时间。
- 固件内置示例数据，首次上电即可看到看板布局。

## 目录

```text
EPD/       墨水屏驱动与 BLE 看板协议
GUI/       看板绘制、字体与图形缓冲
Keil/      nRF52811 的 Keil 工程
SDK/       Nordic nRF5 SDK 17.1（构建依赖）
html/      精简的 Web Bluetooth 控制台
main.c     nRF52811 固件入口
```

## 使用

### 1. 编译并烧录固件

安装 Keil µVision 与 Nordic `nRF_DeviceFamilyPack` 后，打开 [Keil/EPD-nRF52.uvprojx](Keil/EPD-nRF52.uvprojx)，选择 `nRF52811_xxAA`，按 `F7` 编译并通过 J-Link 或 CMSIS-DAP 烧录。

工程也保留了 GCC 构建文件：安装 Arm GNU Toolchain 后，在 `SDK/17.1.0_ddde560/components/toolchain/gcc/Makefile.windows` 设置 `GNU_INSTALL_ROOT`，再执行：

```bash
make -f Makefile.nRF52
```

首次刷入裸芯片时，请另行按你的硬件/调试器流程刷写 S112 SoftDevice；项目中不再包含 OTA 或烧录工具。

### 2. 打开控制页

在项目根目录运行：

```bash
python -m http.server 8000
```

然后使用 Chrome 或 Edge 打开 <http://localhost:8000/html/>。Web Bluetooth 只能在 HTTPS 或 localhost 下工作，不能直接双击 HTML 文件。

点击 **连接 NRF_EPD**，浏览器中选择设备后，填写用量数据，最后点击 **发送到墨水屏**。

页面默认使用 4.2 英寸 SSD1619 配置（模型 `0x02`），与本项目的 nRF52811 默认引脚配置配套；若你的硬件不是这块同款屏幕，需要在固件中调整 `EPD_CFG_52811` 或显示模型。

## 数据来源与限制

看板只负责显示：它不会登录、读取或上传 Codex 账户信息。将 Codex 用量页、账单数据或你自己的统计脚本得到的数值填入网页即可。

| 字段 | 存储单位 | 最大值 |
| --- | --- | --- |
| Token | 0.1M | 6553.5M |
| 请求数 | 1 | 65535 |
| 费用 | 美分 | $655.35 |

协议使用 BLE 服务 `62750001-d828-918d-fb46-b6c11c675aec`、特征 `62750002-d828-918d-fb46-b6c11c675aec`：

- `0x01`：初始化显示驱动。
- `0x20`：同步时间并启用看板模式。
- `0x22, 0x00`：发送摘要数据。
- `0x22, 0x01`：发送 7 日趋势数据并触发刷新。

## 许可与来源

本仓库保留原项目的 [GPL-3.0 许可证](LICENSE)。硬件底层驱动与 Nordic SDK 依赖源自 [YCD12/EPD-nRF5_DYC](https://github.com/YCD12/EPD-nRF5_DYC)；本分支将其收敛为 Codex 用量看板用途。
