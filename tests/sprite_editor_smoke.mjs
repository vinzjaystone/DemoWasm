// Real-input integration test for the C++/Raylib sprite editor. Requires
// ASHFALL_WEB_TEST_API=ON; diagnostics below are read-only, never mutation hooks.
import assert from 'node:assert/strict';
import { mkdir, readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { png, decodePng } from './png_helpers.mjs';
const { chromium } = await import(process.env.PLAYWRIGHT_MODULE || 'playwright');
const artifacts = resolve(process.env.ARTIFACTS || 'build/browser');
await mkdir(artifacts, { recursive: true });
const browser = await chromium.launch({
  headless: true, executablePath: process.env.CHROMIUM_EXECUTABLE || undefined,
  args: ['--enable-unsafe-swiftshader', '--use-angle=swiftshader', ...JSON.parse(process.env.CHROMIUM_ARGS || '[]')],
});
const page = await browser.newPage({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 1 });
const errors = [];
page.on('pageerror', e => errors.push(e.message));
page.on('console', m => { if (m.type() === 'error') errors.push(m.text()); });
const state = () => page.evaluate(() => JSON.parse(Module.UTF8ToString(Module._ashfall_editor_get_state())));
const pixel = (x, y) => page.evaluate(([x, y]) => Module._ashfall_editor_pixel(x, y) >>> 0, [x, y]);
async function until(predicate, label) {
  const end = Date.now() + 10000;
  let last;
  do {
    last = await state();
    if (predicate(last)) return last;
    await page.waitForTimeout(40);
  } while (Date.now() < end);
  throw new Error(`${label}: ${JSON.stringify(last)}`);
}
async function cell(x, y) {
  const s = await state();
  return { x: s.canvas.x + (x + 0.5) * s.zoom, y: s.canvas.y + (y + 0.5) * s.zoom };
}
async function clickPixel(x, y, button = 'left') {
  const p = await cell(x, y);
  await page.mouse.click(p.x, p.y, { button });
}
async function drag(from, to, button = 'left') {
  const a = await cell(...from), b = await cell(...to);
  await page.mouse.move(a.x, a.y); await page.mouse.down({ button });
  await page.mouse.move(b.x, b.y, { steps: 3 }); await page.mouse.up({ button });
}
async function palette(index, expected) {
  const { palette: p } = await state();
  await page.mouse.click(p.x + (index % p.cols) * p.step + p.size / 2,
    p.y + Math.floor(index / p.cols) * p.step + p.size / 2);
  await until(s => s.foreground === expected, 'palette selection');
}
async function fresh(size = 16) {
  const before = await state();
  await page.keyboard.press('Control+n');
  const s = await until(s => s.modal === 1, 'new sprite dialog');
  const index = [16, 32, 64, 128].indexOf(size);
  await page.mouse.click(s.dialog.x + 40 + index * 107, s.dialog.y + 160);
  await page.mouse.click(s.confirm.x + 80, s.confirm.y + 16);
  await until(s => s.stamp > before.stamp && s.width === size && s.height === size && s.modal === 0, 'create sprite acknowledgement');
  assert.equal((await state()).undo, 0);
}
async function shortcut(key, predicate, label) {
  await page.keyboard.press(key);
  return until(predicate, label);
}
async function exportPng() {
  const pending = page.waitForEvent('download');
  await page.keyboard.press('Control+s');
  const file = await pending;
  const path = resolve(artifacts, file.suggestedFilename()); await file.saveAs(path);
  await until(s => !s.dirty, 'export save point');
  return { path, name: file.suggestedFilename(), ...decodePng(await readFile(path)) };
}
try {
  const url = new URL(process.env.EDITOR_URL || 'http://127.0.0.1:8080/editor.html');
  await page.goto(new URL('index.html', url).href, { waitUntil: 'networkidle' });
  await page.getByRole('link', { name: 'Open the Sprite Workshop editor' }).click();
  await page.waitForURL('**/editor.html');
  await page.waitForFunction(() => typeof Module?._ashfall_editor_get_state === 'function');
  assert.equal((await state()).width, 32);
  assert.equal((await state()).dirty, false);
  assert.equal(await page.locator('#loading').evaluate(e => e.classList.contains('done')), true);
  await page.mouse.move(1000, 500);
  await page.screenshot({ path: `${artifacts}/sprite-desktop.png` });
  console.log('[PASS] Game-to-editor navigation, WebGL/WASM startup and procedural example');

  await fresh();
  const red = 0xcf5841ff, teal = 0x6fbba6ff;
  await palette(9, red);
  await drag([1, 1], [14, 14]);
  await until(s => s.undo === 1, 'one drag is one undo entry');
  for (let i = 1; i <= 14; ++i) assert.equal(await pixel(i, i), red, 'fast diagonal strokes must not leave gaps');
  assert.equal(await pixel(0, 0), 0);
  await drag([4, 4], [5, 5], 'right');
  await until(s => s.undo === 2, 'right-drag eraser');
  assert.equal(await pixel(4, 4), 0);
  await shortcut('Control+z', s => s.undo === 1, 'undo eraser');
  assert.equal(await pixel(4, 4), red);
  await shortcut('Control+Shift+z', s => s.undo === 2, 'redo eraser');
  assert.equal(await pixel(4, 4), 0);
  await shortcut('KeyI', s => s.tool === 3, 'picker tool');
  await clickPixel(1, 1); await until(s => s.tool === 0 && s.foreground === red, 'pixel color picking');
  await shortcut('KeyM', s => s.mirror, 'mirror mode');
  await clickPixel(2, 12); await until(s => s.undo === 3, 'mirrored stroke');
  assert.equal(await pixel(2, 12), red); assert.equal(await pixel(13, 12), red);
  await shortcut('Control+z', s => s.undo === 2, 'undo both mirrored pixels');
  assert.equal(await pixel(2, 12), 0); assert.equal(await pixel(13, 12), 0);
  await shortcut('KeyM', s => !s.mirror, 'mirror off');
  await shortcut('Control+n', s => s.modal === 1, 'new dialog with dirty canvas');
  await shortcut('Escape', s => s.modal === 0, 'cancel new document');
  assert.equal((await state()).undo, 2);
  await shortcut('KeyL', s => s.tool === 4, 'line tool');
  await drag([1, 8], [14, 8]); await until(s => s.undo === 3, 'line commit');
  for (let x = 1; x <= 14; ++x) assert.equal(await pixel(x, 8), red);
  console.log('[PASS] Gapless pencil strokes, eraser, undo/redo, picker, mirror and line tool');

  await fresh();
  await shortcut('KeyR', s => s.tool === 5, 'rectangle tool');
  await drag([2, 2], [12, 12]); await until(s => s.undo === 1, 'rectangle commit');
  assert.equal(await pixel(2, 2), red); assert.equal(await pixel(3, 3), 0);
  await palette(17, teal);
  await shortcut('KeyF', s => s.tool === 2, 'fill tool');
  await clickPixel(3, 3); await until(s => s.undo === 2, 'bounded fill');
  assert.equal(await pixel(8, 8), teal); assert.equal(await pixel(2, 8), red); assert.equal(await pixel(1, 8), 0);
  let s = await state();
  await page.mouse.click(s.hex.x + 30, s.hex.y + 15);
  await page.keyboard.type('3366998F');
  await page.keyboard.press('Backspace'); await page.keyboard.type('0'); await page.keyboard.press('Enter');
  await until(s => s.foreground === 0x33669980, 'custom RGBA hex input');
  await shortcut('KeyB', s => s.tool === 0, 'pencil');
  await clickPixel(0, 0); await until(s => s.undo === 3, 'partial-alpha paint');
  assert.equal(await pixel(0, 0), 0x33669980);
  s = await state();
  await page.mouse.click(s.name.x + 35, s.name.y + 15);
  await page.keyboard.type('Cinder-scout'); await page.keyboard.press('Enter');
  await page.waitForTimeout(80);
  const exported = await exportPng();
  assert.equal(exported.name, 'Cinder-scout.png');
  assert.equal(exported.width, 16); assert.equal(exported.height, 16);
  assert.deepEqual([...exported.pixels.subarray(0, 4)], [51, 102, 153, 128]);
  assert.equal(exported.pixels.readUInt32BE((8 * 16 + 8) * 4), teal);
  assert.equal(exported.pixels.readUInt32BE((1 * 16 + 8) * 4), 0, 'PNG must not include checkerboard, grid or zoom');
  console.log('[PASS] Shape and bounded fill, custom RGBA/name, actual transparent PNG download');

  await clickPixel(1, 1); await until(s => s.dirty, 'dirty before import');
  const chooserEvent = page.waitForEvent('filechooser');
  await page.keyboard.press('Control+o');
  const chooser = await chooserEvent;
  await chooser.setFiles(exported.path);
  await until(s => s.modal === 2, 'unsaved import confirmation');
  await shortcut('Escape', s => s.modal === 0, 'cancel destructive import');
  assert.equal(await pixel(1, 1), 0x33669980);
  await page.locator('#png-file').setInputFiles(exported.path);
  s = await until(s => s.modal === 2, 'repeat import');
  await page.mouse.click(s.confirm.x + 65, s.confirm.y + 16);
  await until(s => s.modal === 0 && !s.dirty && s.undo === 0, 'PNG import acknowledgement');
  assert.equal(await pixel(0, 0), 0x33669980); assert.equal(await pixel(1, 1), 0);
  const tiny = Buffer.alloc(3 * 2 * 4); tiny.writeUInt32BE(red, 0); tiny.writeUInt32BE(0x33669980, 20);
  await page.locator('#png-file').setInputFiles({ name: 'tiny.png', mimeType: 'image/png', buffer: png(3, 2, tiny) });
  await until(s => s.width === 3 && s.height === 2, 'rectangular PNG import');
  await page.mouse.click(1384, 581); // Rotate, in the desktop inspector.
  await until(s => s.width === 2 && s.height === 3, 'lossless clockwise rotation');
  assert.equal(await pixel(1, 0), red); assert.equal(await pixel(0, 2), 0x33669980);
  await shortcut('Control+z', s => s.width === 3 && s.height === 2, 'undo rotation and dimensions');
  const beforeBad = await state();
  const invalid = png(3, 2, tiny); invalid.writeUInt32BE(100000, 16);
  await page.locator('#png-file').setInputFiles({ name: 'too-large.png', mimeType: 'image/png', buffer: invalid });
  await until(s => s.importSerial > beforeBad.importSerial, 'rejected import acknowledgement');
  assert.equal((await state()).stamp, beforeBad.stamp); assert.equal((await state()).width, 3);
  const corrupt = png(3, 2, tiny); corrupt.fill(0, 41, corrupt.length - 12);
  let serial = (await state()).importSerial;
  await page.locator('#png-file').setInputFiles({ name: 'corrupt.png', mimeType: 'image/png', buffer: corrupt });
  await until(s => s.importSerial > serial, 'corrupt PNG decode rejection');
  assert.equal((await state()).stamp, beforeBad.stamp);
  const dropped = png(3, 2, tiny);
  const transfer = await page.evaluateHandle(bytes => {
    const transfer = new DataTransfer();
    transfer.items.add(new File([new Uint8Array(bytes)], 'dropped.png', { type: 'image/png' }));
    return transfer;
  }, [...dropped]);
  serial = (await state()).importSerial;
  await page.dispatchEvent('#canvas', 'drop', { dataTransfer: transfer });
  await until(s => s.importSerial > serial && s.width === 3 && s.undo === 0, 'PNG drag and drop');
  await transfer.dispose();
  console.log('[PASS] File picker, import guard, PNG round-trip/drop, rectangular rotation and invalid input rejection');

  await page.setViewportSize({ width: 640, height: 560 });
  await page.waitForFunction(() => Module.canvas.width === 640 && Module.canvas.height === 560);
  await fresh();
  await palette(17, teal);
  await shortcut('KeyB', s => s.tool === 0, 'compact pencil');
  await clickPixel(5, 5); await until(s => s.undo === 1, 'painting after resize');
  assert.equal(await pixel(5, 5), teal);
  s = await state();
  await page.mouse.click(s.hex.x + 20, s.hex.y + 20);
  await page.keyboard.press('Control+a'); await page.keyboard.type('6FBBA680'); await page.keyboard.press('Enter');
  await until(s => s.foreground === 0x6fbba680, 'compact RGBA field');
  await clickPixel(6, 5); await until(s => s.undo === 2, 'compact partial-alpha paint');
  assert.equal(await pixel(6, 5), 0x6fbba680);
  s = await state(); const anchor = await cell(8, 8);
  await page.mouse.move(anchor.x, anchor.y); await page.mouse.wheel(0, -120);
  const zoomed = await until(next => next.zoom > s.zoom, 'cursor-centered zoom');
  assert.ok(Math.abs((anchor.x - zoomed.canvas.x) / zoomed.zoom - 8.5) < 0.03);
  const stamp = zoomed.stamp;
  await page.keyboard.down('Space'); await page.mouse.down();
  await page.mouse.move(anchor.x + 24, anchor.y + 18, { steps: 3 });
  await page.mouse.up(); await page.keyboard.up('Space');
  await until(next => Math.abs(next.canvas.x - zoomed.canvas.x) > 10, 'panning');
  assert.equal((await state()).stamp, stamp, 'panning cannot paint pixels');
  await shortcut('KeyG', s => !s.grid, 'grid toggle');
  await page.screenshot({ path: `${artifacts}/sprite-compact.png` });
  await page.mouse.click(36, 37); await until(s => s.modal === 3, 'unsaved leave guard');
  await shortcut('Escape', s => s.modal === 0, 'keep editing');
  await exportPng();
  console.log('[PASS] Compact palette/canvas, resized picking, anchored zoom, pan, grid and unsaved-work guard');
  assert.deepEqual(errors, [], 'browser errors');
  console.log('All sprite editor browser checks passed.');
} catch (error) {
  console.error(error);
  const latest = await state().catch(() => null);
  console.error('Editor state:', latest);
  if (process.env.GITHUB_ACTIONS === 'true') {
    const details = `${error.stack || error}\nState: ${JSON.stringify(latest)}`
      .replaceAll('%', '%25').replaceAll('\r', '%0D').replaceAll('\n', '%0A');
    console.log(`::error title=Sprite editor smoke test::${details}`);
  }
  await page.screenshot({ path: `${artifacts}/sprite-failure.png` }).catch(() => {});
  process.exitCode = 1;
} finally {
  await browser.close();
}
