const EpdTransfer = (() => {
  const BLACK_WHITE_420_MODELS = new Set([0x01, 0x04]);
  const THREE_COLOR_420_MODELS = new Set([0x02, 0x03]);

  function panelKind(modelId) {
    // This project targets the nRF52811 Hema 4.2-inch BWR tag, whose default
    // model is 0x02. Keep that safe fallback for stock firmware that does not
    // send its configuration notification.
    if (modelId === null || modelId === undefined) return 'three-color';
    if (BLACK_WHITE_420_MODELS.has(modelId)) return 'black-white';
    if (THREE_COLOR_420_MODELS.has(modelId)) return 'three-color';
    return 'unsupported';
  }

  function parsePanelModel(configBytes) {
    return configBytes?.length >= 8 ? configBytes[7] : null;
  }

  function ramFlag(plane, firstPacket) {
    if (plane === 'black') return firstPacket ? 0x0f : 0xff;
    if (plane === 'color') return firstPacket ? 0x00 : 0xf0;
    throw new Error(`未知墨水屏图层：${plane}`);
  }

  function blankColorPlane(length) {
    return new Uint8Array(length).fill(0xff);
  }

  return { blankColorPlane, panelKind, parsePanelModel, ramFlag };
})();

if (typeof globalThis !== 'undefined') globalThis.EpdTransfer = EpdTransfer;
if (typeof module !== 'undefined' && module.exports) module.exports = EpdTransfer;
