#!/usr/bin/env node
/* Generate the separate Settings resource; never replace the Launcher icon. */
const fs = require("fs");
const path = require("path");
const [source, ...outputs] = process.argv.slice(2);
if (!source || !outputs.length) throw new Error("usage: make_settings_icon.js source.bin output.bin [output.bin ...]");
const input = fs.readFileSync(source);
if (input.length < 12 || input[0] !== 0x19 || input[1] !== 0x10)
  throw new Error("expected LVGL ARGB8888 resource");
const width = input.readUInt16LE(4), height = input.readUInt16LE(6);
const stride = input.readUInt32LE(8);
if (!width || !height || stride !== width * 4 || input.length !== 12 + stride * height)
  throw new Error("invalid source dimensions or stride");
for (const output of outputs) {
  if (path.resolve(output) === path.resolve(source) ||
      path.basename(output) !== "shellpp_ii_settings_icon.bin")
    throw new Error("output must be the separate shellpp_ii_settings_icon.bin resource");
}
const size = 64;
const result = Buffer.alloc(12 + size * size * 4);
result[0] = 0x19; result[1] = 0x10;
result.writeUInt16LE(size, 4); result.writeUInt16LE(size, 6);
result.writeUInt32LE(size * 4, 8);
/* Area averaging in premultiplied alpha avoids dark transparent edges. */
for (let y = 0; y < size; y++) {
  for (let x = 0; x < size; x++) {
    const left = x * width / size, right = (x + 1) * width / size;
    const top = y * height / size, bottom = (y + 1) * height / size;
    let alpha = 0, weight = 0;
    const channels = [0, 0, 0];
    for (let sy = Math.floor(top); sy < Math.ceil(bottom); sy++) {
      for (let sx = Math.floor(left); sx < Math.ceil(right); sx++) {
        const w = (Math.min(right, sx + 1) - Math.max(left, sx)) *
          (Math.min(bottom, sy + 1) - Math.max(top, sy));
        const offset = 12 + sy * stride + sx * 4;
        const a = input[offset + 3];
        weight += w; alpha += a * w;
        for (let c = 0; c < 3; c++) channels[c] += input[offset + c] * a * w;
      }
    }
    const offset = 12 + (y * size + x) * 4;
    for (let c = 0; c < 3; c++) result[offset + c] = alpha ? Math.round(channels[c] / alpha) : 0;
    result[offset + 3] = Math.round(alpha / weight);
  }
}
for (const output of outputs) {
  fs.writeFileSync(output, result);
  if (!fs.readFileSync(output).equals(result)) throw new Error("resource verification failed");
  console.log(`${output}: 64x64 ARGB8888, ${result.length} bytes`);
}
