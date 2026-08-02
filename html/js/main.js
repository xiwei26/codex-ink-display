const EPD_SERVICE = '62750001-d828-918d-fb46-b6c11c675aec';
const EPD_CHARACTERISTIC = '62750002-d828-918d-fb46-b6c11c675aec';
const EPD_CMD_INIT = 0x01;
const EPD_CMD_SET_TIME = 0x20;
const EPD_CMD_SET_DASHBOARD = 0x22;

let device;
let characteristic;
let dashboard;

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
      sendButton.disabled = true;
      connectButton.textContent = '连接 NRF_EPD';
      setConnectionStatus('已断开');
    });
    const server = await device.gatt.connect();
    const service = await server.getPrimaryService(EPD_SERVICE);
    characteristic = await service.getCharacteristic(EPD_CHARACTERISTIC);
    await write(EPD_CMD_INIT, [0x02]);
    sendButton.disabled = !dashboard;
    connectButton.textContent = '断开连接';
    setConnectionStatus(`已连接：${device.name}`);
  } catch (error) {
    characteristic = null;
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

async function sendDashboard() {
  if (!dashboard) {
    setConnectionStatus('请先刷新 Codex 数据。');
    return;
  }
  try {
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

    sendButton.disabled = true;
    setConnectionStatus('正在刷新墨水屏…');
    await write(EPD_CMD_SET_TIME, timePayload());
    await write(EPD_CMD_SET_DASHBOARD, summary);
    await write(EPD_CMD_SET_DASHBOARD, history);
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
