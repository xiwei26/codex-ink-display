const EPD_SERVICE = '62750001-d828-918d-fb46-b6c11c675aec';
const EPD_CHARACTERISTIC = '62750002-d828-918d-fb46-b6c11c675aec';
const EPD_CMD_INIT = 0x01;
const EPD_CMD_REFRESH = 0x05;
const EPD_CMD_SET_TIME = 0x20;
const EPD_CMD_SET_DASHBOARD = 0x22;
const EPD_CMD_WRITE_IMAGE = 0x30;
const EPD_APP_VERSION = '62750003-d828-918d-fb46-b6c11c675aec';
const DASHBOARD_FIRMWARE_VERSION = 0x20;

let device;
let characteristic;
let dashboard;
let appVersion = null;

const connectButton = document.getElementById('connectButton');
const refreshButton = document.getElementById('refreshButton');
const sendButton = document.getElementById('sendButton');
const connectionStatus = document.getElementById('connectionStatus');
const usageStatus = document.getElementById('usageStatus');

function setConnectionStatus(message) {
  connectionStatus.textContent = message;
}

function formatTokens(tokens) {
  if (!Number.isFinite(tokens)) return '--';
  if (tokens >= 1_000_000) return `${(tokens / 1_000_000).toFixed(tokens >= 100_000_000 ? 0 : 1)}M`;
  if (tokens >= 1_000) return `${(tokens / 1_000).toFixed(1)}K`;
  return String(tokens);
}

function formatPercent(value) {
  return value === null || value === undefined ? '不可用' : `${value.toFixed(1)}% 已用`;
}

function setText(id, value) {
  document.getElementById(id).textContent = value;
}

function showDashboard(data) {
  const { usage, rateLimits, account } = data;
  setText('todayTokens', formatTokens(usage.todayTokens));
  setText('monthTokens', formatTokens(usage.monthTokens));
  setText('lifetimeTokens', formatTokens(usage.lifetimeTokens));
  setText('peakTokens', formatTokens(usage.peakDailyTokens));
  setText('currentStreak', `${usage.currentStreakDays} 天`);
  setText('longestStreak', `${usage.longestStreakDays} 天`);
  setText('primaryLimit', formatPercent(rateLimits.primaryUsedPercent));
  setText('secondaryLimit', formatPercent(rateLimits.secondaryUsedPercent));
  setText('trend', usage.history.map(formatTokens).join(' · '));
  setText('account', `账号类型：${account.type || '未知'}${account.planType ? ` · 计划：${account.planType}` : ''}`);
}

async function refreshUsage() {
  refreshButton.disabled = true;
  usageStatus.textContent = '正在向本机 Codex 读取数据…';
  try {
    const response = await fetch('/api/usage', { cache: 'no-store' });
    const data = await response.json();
    if (!response.ok) throw new Error(data.error || '读取 Codex 数据失败');
    dashboard = data;
    showDashboard(data);
    usageStatus.textContent = `已刷新：${new Date().toLocaleTimeString()}`;
    sendButton.disabled = !device?.gatt?.connected;
  } catch (error) {
    usageStatus.textContent = `读取失败：${error.message}`;
    sendButton.disabled = true;
  } finally {
    refreshButton.disabled = false;
  }
}

async function write(command, data = []) {
  if (!characteristic) throw new Error('蓝牙设备尚未连接');
  const payload = Uint8Array.from([command, ...data]);
  if (characteristic.writeValueWithResponse) {
    await characteristic.writeValueWithResponse(payload);
  } else {
    await characteristic.writeValue(payload);
  }
}

async function readFirmwareVersion(service) {
  try {
    const versionCharacteristic = await service.getCharacteristic(EPD_APP_VERSION);
    const value = await versionCharacteristic.readValue();
    return value.getUint8(0);
  } catch {
    return null;
  }
}

async function connect() {
  if (device?.gatt?.connected) {
    device.gatt.disconnect();
    return;
  }
  if (!navigator.bluetooth) {
    setConnectionStatus('当前浏览器不支持 Web Bluetooth，请使用 Chrome 或 Edge。');
    return;
  }
  try {
    setConnectionStatus('请选择 NRF_EPD 设备…');
    device = await navigator.bluetooth.requestDevice({
      filters: [{ namePrefix: 'NRF_EPD' }],
      optionalServices: [EPD_SERVICE],
    });
    device.addEventListener('gattserverdisconnected', () => {
      characteristic = null;
      appVersion = null;
      sendButton.disabled = true;
      connectButton.textContent = '连接 NRF_EPD';
      setConnectionStatus('已断开');
    });
    const server = await device.gatt.connect();
    const service = await server.getPrimaryService(EPD_SERVICE);
    characteristic = await service.getCharacteristic(EPD_CHARACTERISTIC);
    await write(EPD_CMD_INIT, [0x02]);
    appVersion = await readFirmwareVersion(service);
    sendButton.disabled = !dashboard;
    connectButton.textContent = '断开连接';
    const mode = appVersion === DASHBOARD_FIRMWARE_VERSION ? '看板协议' : '原厂图片协议';
    const version = appVersion === null ? '未知' : `0x${appVersion.toString(16).padStart(2, '0')}`;
    setConnectionStatus(`已连接：${device.name} · 固件 ${version} · ${mode}`);
  } catch (error) {
    characteristic = null;
    appVersion = null;
    sendButton.disabled = true;
    setConnectionStatus(`连接失败：${error.message}`);
  }
}

function putUint16LE(bytes, offset, value) {
  const safeValue = Math.max(0, Math.min(65535, Math.round(value)));
  bytes[offset] = safeValue & 0xff;
  bytes[offset + 1] = safeValue >>> 8;
}

function tokensToTenthM(tokens) {
  return tokens / 100_000;
}

function percentToHundredths(percent) {
  return percent === null || percent === undefined ? 0 : percent * 100;
}

function timePayload() {
  const now = Math.floor(Date.now() / 1000);
  const offsetHours = -new Date().getTimezoneOffset() / 60;
  return [(now >>> 24) & 0xff, (now >>> 16) & 0xff, (now >>> 8) & 0xff, now & 0xff, offsetHours & 0xff, 0x03];
}

function drawLine(context, x1, y1, x2, y2) {
  context.beginPath();
  context.moveTo(x1, y1);
  context.lineTo(x2, y2);
  context.stroke();
}

function drawText(context, text, x, y, font = '12px sans-serif', color = '#000') {
  context.font = font;
  context.fillStyle = color;
  context.fillText(text, x, y);
}

function dashboardRaster() {
  const { usage, rateLimits } = dashboard;
  const canvas = document.createElement('canvas');
  canvas.width = 400;
  canvas.height = 300;
  const context = canvas.getContext('2d');
  context.fillStyle = '#fff';
  context.fillRect(0, 0, canvas.width, canvas.height);
  context.strokeStyle = '#000';
  context.lineWidth = 1;

  drawText(context, 'CODEX USAGE', 14, 19, 'bold 14px sans-serif');
  drawText(context, new Date().toLocaleDateString(), 305, 19, '11px sans-serif');
  drawLine(context, 12, 26, 388, 26);
  drawLine(context, 208, 35, 208, 144);

  drawText(context, 'TODAY TOKEN', 14, 50, '12px sans-serif');
  drawText(context, formatTokens(usage.todayTokens), 14, 88, 'bold 31px serif');
  drawText(context, `STREAK ${usage.currentStreakDays} DAYS`, 14, 108, '12px sans-serif');
  drawLine(context, 14, 116, 195, 116);
  drawText(context, `LIFETIME ${formatTokens(usage.lifetimeTokens)}`, 14, 132, '12px sans-serif');
  drawText(context, `BEST ${usage.longestStreakDays} DAYS`, 14, 145, '12px sans-serif');

  drawText(context, 'MONTH TOKEN', 220, 50, '12px sans-serif');
  drawText(context, formatTokens(usage.monthTokens), 220, 82, 'bold 24px serif');
  drawText(context, `5H USED  ${formatPercent(rateLimits.primaryUsedPercent)}`, 220, 104, '12px sans-serif');
  drawText(context, `WEEK USED  ${formatPercent(rateLimits.secondaryUsedPercent)}`, 220, 122, '12px sans-serif');
  drawText(context, `PEAK ${formatTokens(usage.peakDailyTokens)}`, 220, 140, '12px sans-serif');

  drawLine(context, 12, 157, 388, 157);
  drawText(context, 'LAST 7 DAYS', 14, 176, 'bold 12px sans-serif');
  const chartLeft = 20;
  const chartRight = 380;
  const chartTop = 188;
  const chartBottom = 254;
  const highest = Math.max(1, ...usage.history);
  drawLine(context, chartLeft, chartBottom, chartRight, chartBottom);
  context.strokeStyle = '#000';
  context.fillStyle = '#fff';
  usage.history.forEach((tokens, index) => {
    const x = chartLeft + ((chartRight - chartLeft) * index) / 6;
    const y = chartBottom - ((chartBottom - chartTop) * tokens) / highest;
    if (index > 0) {
      const previous = usage.history[index - 1];
      const previousX = chartLeft + ((chartRight - chartLeft) * (index - 1)) / 6;
      const previousY = chartBottom - ((chartBottom - chartTop) * previous) / highest;
      drawLine(context, previousX, previousY, x, y);
    }
    context.beginPath();
    context.arc(x, y, 2.5, 0, Math.PI * 2);
    context.fill();
    context.stroke();
    drawText(context, String(index + 1), x - 3, 271, '10px sans-serif');
  });
  drawLine(context, 12, 282, 388, 282);
  drawText(context, `UPDATED ${new Date().toLocaleTimeString()}`, 14, 295, '10px sans-serif');
  return canvas;
}

function canvasToMonochrome(canvas) {
  const context = canvas.getContext('2d');
  const pixels = context.getImageData(0, 0, canvas.width, canvas.height).data;
  const bytesPerRow = Math.ceil(canvas.width / 8);
  const output = new Uint8Array(bytesPerRow * canvas.height);
  for (let y = 0; y < canvas.height; y += 1) {
    for (let x = 0; x < canvas.width; x += 1) {
      const pixel = (y * canvas.width + x) * 4;
      const luminance = pixels[pixel] * 0.299 + pixels[pixel + 1] * 0.587 + pixels[pixel + 2] * 0.114;
      if (luminance >= 140) output[y * bytesPerRow + Math.floor(x / 8)] |= 1 << (7 - (x % 8));
    }
  }
  return output;
}

async function sendRasterDashboard() {
  const image = canvasToMonochrome(dashboardRaster());
  const chunkSize = 18;
  for (let offset = 0; offset < image.length; offset += chunkSize) {
    const packetIndex = Math.floor(offset / chunkSize) + 1;
    const packetCount = Math.ceil(image.length / chunkSize);
    setConnectionStatus(`正在传输看板图片：${packetIndex}/${packetCount}`);
    const ramFlags = offset === 0 ? 0x0f : 0xff;
    await write(EPD_CMD_WRITE_IMAGE, [ramFlags, ...image.slice(offset, offset + chunkSize)]);
  }
  setConnectionStatus('正在刷新墨水屏…');
  await write(EPD_CMD_REFRESH);
}

async function sendDashboardProtocol() {
  const { usage, rateLimits } = dashboard;
  const summary = new Uint8Array(17);
  summary[0] = 0;
  putUint16LE(summary, 1, tokensToTenthM(usage.todayTokens));
  putUint16LE(summary, 3, tokensToTenthM(usage.monthTokens));
  putUint16LE(summary, 5, usage.currentStreakDays);
  putUint16LE(summary, 7, usage.longestStreakDays);
  putUint16LE(summary, 9, percentToHundredths(rateLimits.primaryUsedPercent));
  putUint16LE(summary, 11, percentToHundredths(rateLimits.secondaryUsedPercent));
  putUint16LE(summary, 13, tokensToTenthM(usage.lifetimeTokens));
  putUint16LE(summary, 15, tokensToTenthM(usage.peakDailyTokens));

  const history = new Uint8Array(15);
  history[0] = 1;
  usage.history.forEach((tokens, index) => putUint16LE(history, 1 + index * 2, tokensToTenthM(tokens)));
  await write(EPD_CMD_SET_TIME, timePayload());
  await write(EPD_CMD_SET_DASHBOARD, summary);
  await write(EPD_CMD_SET_DASHBOARD, history);
}

async function sendDashboard() {
  if (!dashboard) {
    setConnectionStatus('请先刷新 Codex 数据。');
    return;
  }
  try {
    sendButton.disabled = true;
    if (appVersion === DASHBOARD_FIRMWARE_VERSION) {
      setConnectionStatus('正在通过看板协议刷新墨水屏…');
      await sendDashboardProtocol();
    } else {
      await sendRasterDashboard();
    }
    setConnectionStatus(`已推送：${new Date().toLocaleTimeString()}`);
  } catch (error) {
    setConnectionStatus(`推送失败：${error.message}`);
  } finally {
    sendButton.disabled = !device?.gatt?.connected || !dashboard;
  }
}

connectButton.addEventListener('click', connect);
refreshButton.addEventListener('click', refreshUsage);
sendButton.addEventListener('click', sendDashboard);
refreshUsage();
