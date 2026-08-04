const assert = require('node:assert/strict');
const {
  blankColorPlane,
  panelKind,
  parsePanelModel,
  ramFlag,
} = require('./html/js/epd-transfer.js');

assert.equal(parsePanelModel(Uint8Array.from([0, 0, 0, 0, 0, 0, 0, 2])), 0x02);
assert.equal(parsePanelModel(Uint8Array.from([0, 0, 0])), null);

assert.equal(panelKind(0x01), 'black-white');
assert.equal(panelKind(0x04), 'black-white');
assert.equal(panelKind(0x02), 'three-color');
assert.equal(panelKind(0x03), 'three-color');
assert.equal(panelKind(null), 'three-color');
assert.equal(panelKind(0x05), 'unsupported');

assert.equal(ramFlag('black', true), 0x0f);
assert.equal(ramFlag('black', false), 0xff);
assert.equal(ramFlag('color', true), 0x00);
assert.equal(ramFlag('color', false), 0xf0);
assert.throws(() => ramFlag('unknown', true), /未知墨水屏图层/);

const blank = blankColorPlane(15000);
assert.equal(blank.length, 15000);
assert.ok(blank.every((byte) => byte === 0xff));

console.log('EPD transfer tests passed');
