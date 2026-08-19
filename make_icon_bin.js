#!/usr/bin/env node
/* Convert the deterministic 32-bit BMP emitted by macOS sips into the
 * ARGB8888 LVGL resource format accepted by XiaomiVela's launcher. */
const fs = require('fs');

const [input, output] = process.argv.slice(2);
if (!input || !output) {
  throw new Error('usage: make_icon_bin.js input.bmp output.bin');
}

const bmp = fs.readFileSync(input);
if (bmp.toString('ascii', 0, 2) !== 'BM') throw new Error('not a BMP file');
const offset = bmp.readUInt32LE(10);
const width = bmp.readInt32LE(18);
const signedHeight = bmp.readInt32LE(22);
const bitsPerPixel = bmp.readUInt16LE(28);
if (width <= 0 || signedHeight === 0 || bitsPerPixel !== 32) {
  throw new Error('expected a non-empty 32-bit BMP');
}
const height = Math.abs(signedHeight);
const stride = width * 4;
if (offset + stride * height > bmp.length) throw new Error('truncated BMP pixels');

const resource = Buffer.alloc(12 + stride * height);
resource[0] = 0x19;               // LVGL image magic
resource[1] = 0x10;               // LV_COLOR_FORMAT_ARGB8888
resource.writeUInt16LE(width, 4);
resource.writeUInt16LE(height, 6);
resource.writeUInt32LE(stride, 8);
for (let row = 0; row < height; row += 1) {
  const sourceRow = signedHeight < 0 ? row : height - 1 - row;
  bmp.copy(resource, 12 + row * stride, offset + sourceRow * stride,
           offset + (sourceRow + 1) * stride);
}
fs.writeFileSync(output, resource);
