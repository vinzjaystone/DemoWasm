// Small, dependency-free PNG fixtures/reader for browser integration tests.
import assert from 'node:assert/strict';
import { deflateSync, inflateSync } from 'node:zlib';
function crc(bytes) {
  let value = 0xffffffff;
  for (const byte of bytes) {
    value ^= byte;
    for (let i = 0; i < 8; ++i) value = (value >>> 1) ^ ((value & 1) ? 0xedb88320 : 0);
  }
  return (value ^ 0xffffffff) >>> 0;
}
function chunk(kind, bytes) {
  const type = Buffer.from(kind), length = Buffer.alloc(4), sum = Buffer.alloc(4);
  length.writeUInt32BE(bytes.length); sum.writeUInt32BE(crc(Buffer.concat([type, bytes])));
  return Buffer.concat([length, type, bytes, sum]);
}
export function png(width, height, rgba) {
  const header = Buffer.alloc(13); header.writeUInt32BE(width); header.writeUInt32BE(height, 4); header[8] = 8; header[9] = 6;
  const rows = [];
  for (let y = 0; y < height; ++y) rows.push(Buffer.from([0]), rgba.subarray(y * width * 4, (y + 1) * width * 4));
  return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', header),
    chunk('IDAT', deflateSync(Buffer.concat(rows))), chunk('IEND', Buffer.alloc(0))]);
}
export function decodePng(bytes) {
  assert.deepEqual([...bytes.subarray(0, 8)], [137, 80, 78, 71, 13, 10, 26, 10]);
  let width, height;
  const data = [];
  for (let pos = 8; pos < bytes.length;) {
    const size = bytes.readUInt32BE(pos), type = bytes.toString('ascii', pos + 4, pos + 8);
    if (type === 'IHDR') {
      width = bytes.readUInt32BE(pos + 8); height = bytes.readUInt32BE(pos + 12);
      assert.equal(bytes[pos + 16], 8); assert.equal(bytes[pos + 17], 6); // Lossless RGBA8.
    }
    if (type === 'IDAT') data.push(bytes.subarray(pos + 8, pos + 8 + size));
    pos += 12 + size;
  }
  const packed = inflateSync(Buffer.concat(data)), stride = width * 4, pixels = Buffer.alloc(stride * height);
  for (let y = 0; y < height; ++y) {
    const filter = packed[y * (stride + 1)];
    for (let x = 0; x < stride; ++x) {
      const i = y * stride + x, a = x >= 4 ? pixels[i - 4] : 0, b = y ? pixels[i - stride] : 0;
      const c = y && x >= 4 ? pixels[i - stride - 4] : 0;
      let predict = 0;
      if (filter === 1) predict = a;
      else if (filter === 2) predict = b;
      else if (filter === 3) predict = Math.floor((a + b) / 2);
      else if (filter === 4) {
        const p = a + b - c, pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c);
        predict = pa <= pb && pa <= pc ? a : (pb <= pc ? b : c);
      } else assert.equal(filter, 0);
      pixels[i] = (packed[y * (stride + 1) + x + 1] + predict) & 255;
    }
  }
  return { width, height, pixels };
}
