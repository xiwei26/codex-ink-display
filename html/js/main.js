const EPD_SERVICE = '62750001-d828-918d-fb46-b6c11c675aec';
const EPD_CHARACTERISTIC = '62750002-d828-918d-fb46-b6c11c675aec';
const EPD_CMD_INIT = 0x01;
const EPD_CMD_SET_TIME = 0x20;
const EPD_CMD_SET_DASHBOARD = 0x22;

let device;
let characteristic;

const connectButton = document.getElementById('connectButton');
const sendButton = document.getElementById('sendButton');
const connectionStatus = document.getElementById('connectionStatus');

function setStatus(message) {
  connectionStatus.textContent = message;
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
    setStatus('当前浏览器不支持 Web Bluetooth，请使用 Chrome 或 Edge。');
    return;
  }
  try {
    setStatus('请选择 NRF_EPD 设备…');
    device = await navigator.bluetooth.requestDevice({
      filters: [{ namePrefix: 'NRF_EPD' }],
      optionalServices: [EPD_SERVICE],
    });
    device.addEventListener('gattserverdisconnected', () => {
      characteristic = null;
      sendButton.disabled = true;
      connectButton.textContent = '连接 NRF_EPD';
      setStatus('已断开');
    });
    const server = await device.gatt.connect();
    const service = await server.getPrimaryService(EPD_SERVICE);
    characteristic = await service.getCharacteristic(EPD_CHARACTERISTIC);
    await write(EPD_CMD_INIT, [0x02]);
    sendButton.disabled = false;
    connectButton.textContent = '断开连接';
    setStatus(`已连接：${device.name}`);
  } catch (error) {
    characteristic = null;
    sendButton.disabled = true;
    setStatus(`连接失败：${error.message}`);
  }
}

function getUint16(id, multiplier = 1) {
  const value = Number(document.getElementById(id).value);
  const scaled = Math.round(value * multiplier);
  if (!Number.isFinite(value) || value < 0 || scaled > 65535) throw new Error(`${id} 的数值无效`);
  return scaled;
}

function putUint16LE(bytes, offset, value) {
  bytes[offset] = value & 0xff;
  bytes[offset + 1] = value >>> 8;
}

function getTrend() {
  const values = document.getElementById('trend').value.split(/[,\s]+/).filter(Boolean).map(item => Math.round(Number(item) * 10));
  if (values.length !== 7 || values.some(value => !Number.isFinite(value) || value < 0 || value > 65535)) {
    throw new Error('近 7 日 Token 必须包含 7 个有效数字');
  }
  return values;
}

function timePayload() {
  const now = Math.floor(Date.now() / 1000);
  const offsetHours = -new Date().getTimezoneOffset() / 60;
  return [(now >>> 24) & 0xff, (now >>> 16) & 0xff, (now >>> 8) & 0xff, now & 0xff, offsetHours & 0xff, 0x03];
}

async function sendDashboard() {
  try {
    const summary = new Uint8Array(17);
    summary[0] = 0;
    putUint16LE(summary, 1, getUint16('todayTokens', 10));
    putUint16LE(summary, 3, getUint16('monthTokens', 10));
    putUint16LE(summary, 5, getUint16('todayRequests'));
    putUint16LE(summary, 7, getUint16('monthRequests'));
    putUint16LE(summary, 9, getUint16('todayCost', 100));
    putUint16LE(summary, 11, getUint16('monthCost', 100));
    putUint16LE(summary, 13, getUint16('claudeTokens', 10));
    putUint16LE(summary, 15, getUint16('codexTokens', 10));

    const history = new Uint8Array(15);
    history[0] = 1;
    getTrend().forEach((value, index) => putUint16LE(history, 1 + index * 2, value));

    sendButton.disabled = true;
    setStatus('正在刷新墨水屏…');
    await write(EPD_CMD_SET_TIME, timePayload());
    await write(EPD_CMD_SET_DASHBOARD, summary);
    await write(EPD_CMD_SET_DASHBOARD, history);
    setStatus(`已更新：${new Date().toLocaleTimeString()}`);
  } catch (error) {
    setStatus(`发送失败：${error.message}`);
  } finally {
    sendButton.disabled = !device?.gatt?.connected;
  }
}

connectButton.addEventListener('click', connect);
sendButton.addEventListener('click', sendDashboard);
