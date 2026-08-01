# Codex 用量看板

本分支新增了 `MODE_CODEX_DASHBOARD`（值为 `3`），用于 4.2 英寸 400×300 墨水屏上的 Codex 用量看板。布局复刻了参考图的结构：今日/月度 Token、请求数、费用、来源拆分，以及近 7 日趋势。

## 控制方式

网页控制台连接设备并完成屏幕驱动初始化后，在页面底部的 **Codex Usage Dashboard** 区填写数值并点击 **Send Codex dashboard**。

本地使用网页时，可在仓库根目录运行 `python -m http.server 8000`，再用 Chrome 打开 `http://localhost:8000/html/`。Web Bluetooth 需要 HTTPS 或 localhost；不要直接双击 `index.html`。

网页会依次发送：

1. `SET_TIME (0x20)`：同步时间并切换到模式 `3`。
2. `SET_DASHBOARD (0x22), section 0`：摘要数据。
3. `SET_DASHBOARD (0x22), section 1`：7 日趋势数据并触发一次完整刷新。

命令被拆成两个小包，因而兼容默认 20-byte ATT MTU 的旧 nRF51 固件。固件只在第二个包到达后刷新，避免重复闪屏。

### 数据格式

所有整数均为 little-endian `uint16_t`：

| 命令 | 负载（不含命令字节） |
| --- | --- |
| `0x22` section `0` | `[0, today_token_0.1M, month_token_0.1M, today_requests, month_requests, today_cost_cents, month_cost_cents, claude_token_0.1M, codex_token_0.1M]` |
| `0x22` section `1` | `[1, day_1_0.1M, day_2_0.1M, day_3_0.1M, day_4_0.1M, day_5_0.1M, day_6_0.1M, day_7_0.1M]` |

因此 Token 最多为 `6553.5M`，费用最多为 `$655.35`。数值保存在运行内存中；设备断电后墨水屏会保留最后一帧，重新上电而尚未收到新数据时则显示内置示例数据。

本实现负责渲染和传输，不会读取或上传 Codex 账户信息；把你从 Codex 用量页或自己的统计脚本得到的数值填写到网页即可。

## 编译与烧录

该仓库沿用商家工程的方式：使用 Keil 5.36 或更低版本打开 [Keil/EPD-nRF52.uvprojx](../Keil/EPD-nRF52.uvprojx)，选择与硬件相符的 Target 编译并下载。首次烧录还需要先按上游的说明写入对应 SoftDevice；详情见 [develop.md](develop.md)。

屏幕的 MCU 型号/尺寸、SPI 引脚及颜色层仍由网页的 **设备控制** 区配置。固件的实际绘制入口是 `EPD/EPD_service.c` 中的 `epd_gui_update()`：它将 BLE 接收的数据放进 `gui_data_t`，调用 `GUI/GUI.c` 的 `DrawGUI()`，再由 EPD 驱动 `refresh()` 写入面板。
