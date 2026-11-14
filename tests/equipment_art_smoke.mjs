// End-to-end production file/IndexedDB bridge, driven by real pointer/keyboard
// inputs. The optional WASM diagnostics are read-only observations.
import assert from 'node:assert/strict';
import { mkdir, readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { png, decodePng } from './png_helpers.mjs';
const { chromium } = await import(process.env.PLAYWRIGHT_MODULE || 'playwright');
const artifacts = resolve(process.env.ARTIFACTS || 'build/browser');
await mkdir(artifacts, { recursive: true });
const browser = await chromium.launch({ headless: true, executablePath: process.env.CHROMIUM_EXECUTABLE || undefined,
  args: ['--enable-unsafe-swiftshader', '--use-angle=swiftshader', ...JSON.parse(process.env.CHROMIUM_ARGS || '[]')] });
const context = await browser.newContext({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 1 });
const game = await context.newPage(), page = await context.newPage();
const errors = [];
for (const tab of [game, page]) {
  tab.on('pageerror', e => errors.push(e.message));
  tab.on('console', m => { if (m.type() === 'error') errors.push(m.text()); });
}
await context.addInitScript(() => {
  window.__artTextures = []; window.__failArtUpload = 0;
  const proto = WebGLRenderingContext.prototype, image = proto.texImage2D, destroy = proto.deleteTexture;
  proto.texImage2D = function (...args) {
    if (args.length === 9 && args[3] === 96 && args[4] === 96) {
      const record = { texture: this.getParameter(this.TEXTURE_BINDING_2D), deleted: false };
      window.__artTextures.push(record);
      if (window.__failArtUpload && --window.__failArtUpload === 0) return; // Nonzero id, no allocated storage.
    }
    return image.apply(this, args);
  };
  proto.deleteTexture = function (texture) {
    for (const record of window.__artTextures) if (record.texture === texture) record.deleted = true;
    return destroy.call(this, texture);
  };
});
const editorState = () => page.evaluate(() => JSON.parse(Module.UTF8ToString(Module._ashfall_editor_get_state())));
const gameState = () => game.evaluate(() => JSON.parse(Module.UTF8ToString(Module._ashfall_get_state())));
const pixel = (x, y) => page.evaluate(([x, y]) => Module._ashfall_editor_pixel(x, y) >>> 0, [x, y]);
async function wait(read, predicate, label) {
  const end = Date.now() + 15000;
  let last;
  do {
    last = await read(); if (predicate(last)) return last;
    await page.waitForTimeout(40);
  } while (Date.now() < end);
  throw new Error(`${label}: ${JSON.stringify(last)}`);
}
const until = (predicate, label) => wait(editorState, predicate, label);
const gameUntil = (predicate, label) => wait(gameState, predicate, label);
async function click(x, y, predicate, label = 'editor input') {
  await page.mouse.click(x, y); return until(predicate, label);
}
async function frame(number) { await click(94 + number * 34, 226, s => s.frame === number); }
async function clip(number) { await click(112 + number * 70, 188, s => s.clip === number && s.frame === 0); }
async function slot(number) { await click(120 + number * 86, 150, s => s.slot === number); }
async function facing(number) {
  const s = await editorState();
  const right = s.palette.cols === 7 ? page.viewportSize().width - 296 : page.viewportSize().width - 16;
  await click(304 + (number + 0.5) * (right - 304) / 8, 188, s => s.direction === number);
}
async function drag(from, to) {
  const s = await editorState();
  const position = ([x, y]) => ({ x: s.canvas.x + (x + 0.5) * s.zoom, y: s.canvas.y + (y + 0.5) * s.zoom });
  const a = position(from), b = position(to);
  await page.mouse.move(a.x, a.y); await page.mouse.down(); await page.mouse.move(b.x, b.y, { steps: 3 }); await page.mouse.up();
  await until(s => s.dirty, 'committed artwork');
}
async function confirm(modal) {
  const s = await until(s => s.modal === modal, 'confirmation');
  await click(s.confirm.x + 50, s.confirm.y + 15, s => s.modal === 0);
}
async function saveProject() {
  const pending = page.waitForEvent('download'); await page.keyboard.press('Control+s');
  const download = await pending, path = resolve(artifacts, download.suggestedFilename());
  assert.equal(download.suggestedFilename(), 'equipment.ashgear'); await download.saveAs(path);
  await until(s => !s.dirty, 'whole-project save point');
  return { path, bytes: await readFile(path) };
}
const file = bytes => ({ name: 'equipment.ashgear', mimeType: 'application/octet-stream', buffer: bytes });
function hash(bytes) { let value = 2166136261; for (const byte of bytes) value = Math.imul(value ^ byte, 16777619); return value >>> 0; }
function readProject(bytes) {
  assert.equal(bytes.subarray(0, 8).toString('ascii'), 'ASHGEAR\0');
  assert.equal(bytes.readUInt16LE(8), 1); assert.equal(bytes.readUInt16LE(12), 96);
  assert.equal(bytes.readUInt16LE(14), 48); assert.equal(bytes.readUInt16LE(16), 76);
  const count = bytes.readUInt32LE(24), frames = new Map(); let pos = 37 + bytes[36];
  assert.equal(hash(bytes.subarray(pos)), bytes.readUInt32LE(32));
  for (let f = 0; f < count; ++f) {
    const identity = [...bytes.subarray(pos, pos + 5)].join('/'), encoding = bytes[pos + 5], size = bytes.readUInt32LE(pos + 6);
    pos += 10; const rgba = Buffer.alloc(96 * 96 * 4);
    if (!encoding) bytes.copy(rgba, 0, pos, pos + size);
    else {
      let at = 0;
      for (let run = pos; run < pos + size; run += 6) for (let p = 0; p < bytes.readUInt16LE(run); ++p) {
        bytes.copy(rgba, at, run + 2, run + 6); at += 4;
      }
      assert.equal(at, rgba.length);
    }
    frames.set(identity, rgba); pos += size;
  }
  assert.equal(pos, bytes.length); return frames;
}
async function screenPixel(tab, x, y) {
  return tab.evaluate(([x, y]) => new Promise(resolve => requestAnimationFrame(() => {
    const canvas = Module.canvas, gl = canvas.getContext('webgl'), rgba = new Uint8Array(4);
    gl.readPixels(Math.floor(x), canvas.height - 1 - Math.floor(y), 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, rgba);
    resolve([...rgba]);
  })), [x, y]);
}
function stats(s) { return [s.armor, s.head, s.body, s.boots, s.attackSpeed, s.attackInterval, s.health, s.potions]; }
try {
  const url = new URL(process.env.GAME_URL || 'http://127.0.0.1:8080/index.html');
  await game.goto(url.href); await game.waitForFunction(() => !!Module._ashfall_get_state);
  await game.keyboard.press('p'); const baseline = await gameUntil(s => s.paused, 'pause for art import');
  await page.goto(new URL('editor.html', url).href); await page.waitForFunction(() => !!Module._ashfall_editor_get_state);
  await click(405, 37, s => s.equipment && s.previewReady, 'open built-in equipment');
  const opened = await editorState(); assert.deepEqual([opened.slot, opened.variant], [1, 1]);
  await clip(1); await facing(7); await frame(2); // Armor / Ashguard plate / Walk / SE / Frame 3.
  const beforePreview = await editorState();
  await until(s => s.previewFrame !== beforePreview.previewFrame, 'animated layered preview');
  await click(506, 226, s => !s.playing);
  assert.equal((await editorState()).previewFrame, 2, 'pause previews exactly the selected frame');
  const s = await editorState(); await page.mouse.click(s.hex.x + 40, s.hex.y + 16);
  await page.keyboard.press('Control+a'); await page.keyboard.type('FFFFFFFF'); await page.keyboard.press('Enter');
  await until(s => s.foreground === 0xffffffff, 'raw RGBA color');
  await drag([2, 2], [10, 10]); await until(s => s.framesEdited === 1 && s.undo === 1, 'frame transaction');
  assert.equal(await pixel(2, 2), 0xffffffff);
  await page.keyboard.press('Control+z'); await until(s => !s.dirty && s.framesEdited === 0, 'undo to starter');
  await page.keyboard.press('Control+Shift+z'); await until(s => s.framesEdited === 1, 'redo frame edit');
  await frame(3); assert.equal(await pixel(2, 2), 0);
  await facing(4); assert.equal(await pixel(2, 2), 0);
  await frame(2); await facing(7); assert.equal(await pixel(2, 2), 0xffffffff, 'switching cannot lose artwork');
  await page.mouse.move(1000, 800);
  let active = await editorState();
  assert.deepEqual(await screenPixel(page, active.canvas.x + 2.5 * active.zoom, active.canvas.y + 2.5 * active.zoom), [99, 135, 135, 255], 'catalog tint at an OUTSIDE-CROP edited pixel');
  await click(420, 226, s => !s.references);
  assert.deepEqual(await screenPixel(page, active.canvas.x + 2.5 * active.zoom, active.canvas.y + 2.5 * active.zoom), [255, 255, 255, 255], 'solo view shows raw RGBA');
  await click(420, 226, s => s.references);
  console.log('[PASS] Built-in item/clip/facing/frame selection, gapless paint, undo/redo, animated/tinted preview, full 96px registration');

  // Replace only idle armor frames with a diagnostic corner patch; every other
  // layer stays generated and the marked outer pixels must also reach the game.
  const marker = Buffer.alloc(96 * 96 * 4);
  for (let y = 0; y < 12; ++y) for (let x = 0; x < 12; ++x) marker.writeUInt32BE(0xffffffff, (y * 96 + x) * 4);
  await clip(0);
  for (let f = 0; f < 4; ++f) {
    await frame(f); await page.locator('#png-file').setInputFiles({ name: 'frame.png', mimeType: 'image/png', buffer: png(96, 96, marker) });
    await confirm(2); await until(s => s.framesEdited === f + 2, 'frame PNG is included in project');
  }
  assert.equal(await pixel(0, 0), 0xffffffff);
  const pngDownload = page.waitForEvent('download'); await page.mouse.click(140, 264);
  const pngFile = await pngDownload, image = decodePng(await readFile(await pngFile.path()));
  assert.equal(image.width, 96); assert.equal(image.height, 96); assert.deepEqual(image.pixels, marker);
  assert.equal((await editorState()).dirty, true, 'frame PNG cannot mark the whole project saved');
  await page.mouse.click(36, 37); await until(s => s.modal === 3, 'whole-project leave guard');
  await page.keyboard.press('Escape'); await until(s => s.modal === 0, 'cancel dialog');
  const serial = (await editorState()).applySerial;
  await page.mouse.click(620, 226);
  await until(s => s.applySerial > serial && s.applyStatus === 1, 'explicit browser apply');
  assert.equal((await editorState()).dirty, true, 'applying is not a portable save point');
  await gameUntil(s => s.artFrames === 5, 'live game tab receives explicit apply');
  assert.deepEqual(stats(await gameState()), stats(baseline), 'artwork cannot change stats, potions, health or equipped identities');
  const saved = await saveProject(), frames = readProject(saved.bytes);
  assert.equal(frames.size, 5); assert.equal(frames.get('1/1/1/7/2').readUInt32BE((2 * 96 + 2) * 4), 0xffffffff);
  assert.deepEqual(frames.get('1/1/0/7/3'), marker);
  assert.ok(saved.bytes.length < 5 * 96 * 96 * 4 / 4, 'sparse project is losslessly compressed');
  await page.screenshot({ path: `${artifacts}/equipment-edited.png` });
  console.log('[PASS] Frame PNG/project distinction, all-frame dirty guard, portable .ashgear save, explicit live apply without stat mutations');

  await click(405, 37, s => s.equipment && !s.framesEdited, 'fresh built-in project');
  await page.locator('#png-file').setInputFiles(saved.path);
  await until(s => s.framesEdited === 5 && !s.dirty, 'project round-trip');
  await clip(1); await frame(2); assert.equal(await pixel(2, 2), 0xffffffff);
  // Independent reference equipment choices: all nine items, clips and facings.
  for (let sl = 0; sl < 3; ++sl) {
    await slot(sl);
    for (let item = 0; item < 3; ++item) {
      const previous = (await editorState()).variant;
      await click(620, 150, s => s.variant === (previous + 1) % 3);
    }
  }
  for (let c = 0; c < 3; ++c) { await clip(c); for (let d = 0; d < 8; ++d) await facing(d); }
  assert.equal((await editorState()).dirty, false, 'selection and animation are read-only');
  await slot(1); await clip(1); await frame(2);
  await drag([14, 2], [14, 5]);
  const importSerial = (await editorState()).importSerial;
  await page.locator('#png-file').setInputFiles(saved.path);
  await until(s => s.modal === 8 && s.importSerial > importSerial, 'project-wide replace guard');
  await page.keyboard.press('Escape'); await until(s => s.modal === 0, 'cancel dialog'); assert.equal(await pixel(14, 2), 0xffffffff);
  await page.mouse.click(280, 264); await confirm(8); // Open Applied Gear, same protected path.
  assert.equal(await pixel(14, 2), 0); assert.equal((await editorState()).dirty, false);
  const bad = Buffer.from(saved.bytes); bad[bad.length - 1] ^= 1;
  const beforeBad = await editorState(); await page.locator('#png-file').setInputFiles(file(bad));
  await until(s => s.importSerial > beforeBad.importSerial, 'corrupt project rejected');
  assert.equal((await editorState()).framesEdited, 5); assert.equal((await editorState()).dirty, false);
  await page.setViewportSize({ width: 880, height: 680 });
  await until(s => s.palette.cols === 14, 'compact equipment layout');
  await clip(1); await facing(7); await frame(2); assert.equal(await pixel(2, 2), 0xffffffff);
  await page.screenshot({ path: `${artifacts}/equipment-compact.png` });
  console.log('[PASS] Project import/validation, protected Open Applied, all nine items/three clips/eight directions, compact frame selection');

  await game.reload(); await game.waitForFunction(() => !!Module._ashfall_get_state);
  await gameUntil(s => s.artFrames === 5 && s.artHash === hash(saved.bytes), 'persistent game reload');
  await game.mouse.move(1350, 700);
  assert.deepEqual(await screenPixel(game, 1216, 178), [99, 135, 135, 255], 'game draws edited outer-frame pixels with identical tint');
  await game.screenshot({ path: `${artifacts}/equipment-in-game.png` });
  await game.keyboard.press('p'); await gameUntil(s => s.paused, 'pause before invalid import');
  const gameBefore = await gameState(); await game.locator('#equipment-file').setInputFiles(file(bad));
  await gameUntil(s => s.artSerial > gameBefore.artSerial, 'game invalid import acknowledged');
  assert.equal((await gameState()).artHash, hash(saved.bytes)); assert.deepEqual(stats(await gameState()), stats(gameBefore));
  const originalUploads = await game.evaluate(() => window.__artTextures.length);
  await game.evaluate(() => { window.__failArtUpload = 3; });
  const serialBeforeFailure = (await gameState()).artSerial;
  await game.locator('#equipment-file').setInputFiles(saved.path);
  await gameUntil(s => s.artSerial > serialBeforeFailure, 'failed GPU import acknowledged');
  assert.equal((await gameState()).artHash, hash(saved.bytes));
  assert.deepEqual(await game.evaluate(() => window.__artTextures.map(t => t.deleted)), [...Array(originalUploads).fill(false), true, true, true], 'atomic upload failure keeps old textures and deletes EVERY partial allocation');
  await game.locator('#equipment-file').setInputFiles(saved.path);
  await gameUntil(s => s.artSerial > serialBeforeFailure + 1, 'regular game file import');
  assert.equal(await game.evaluate(() => window.__artTextures.filter(t => !t.deleted).length), 5, 'replacement releases the previous project');
  const allocations = await game.evaluate(() => window.__artTextures.length);
  await game.keyboard.press('r'); const restarted = await gameUntil(s => s.restartSerial > gameBefore.restartSerial, 'restart');
  assert.equal(restarted.artFrames, 5); await game.waitForTimeout(350);
  assert.equal(await game.evaluate(() => window.__artTextures.length), allocations, 'animation/restart never allocates GPU frames');
  await game.mouse.click(1360, 600); // Inventory Restore Art button.
  await gameUntil(s => s.artFrames === 0 && s.artName === 'built-in', 'restore built-in art');
  await game.waitForTimeout(150); await game.reload(); await game.waitForFunction(() => !!Module._ashfall_get_state);
  await gameUntil(s => s.artSerial > 0, 'storage restore completed'); assert.equal((await gameState()).artFrames, 0);
  assert.equal((await gameState()).spriteUploads, 5);
  console.log('[PASS] Real game rendering outside original crops, persistence/reload, safe corrupt/GPU failure, replacement cleanup, restart/cache, built-in restoration');

  // Storage denial must never discard work or report it saved.
  await page.evaluate(() => { indexedDB.open = function () { throw new Error('intentional storage denial'); }; });
  await page.keyboard.press('b'); await drag([18, 2], [18, 3]);
  const failedSerial = (await editorState()).applySerial;
  await page.mouse.click(620, 226); await until(s => s.applySerial > failedSerial && s.applyStatus === 2, 'honest storage failure');
  assert.equal((await editorState()).dirty, true);
  assert.deepEqual(errors, [], 'browser errors');
  console.log('[PASS] Denied browser storage preserves all edits and does not claim a project save');
  console.log('All equipment artwork browser checks passed.');
} catch (error) {
  console.error(error);
  const details = { editor: await editorState().catch(() => null), game: await gameState().catch(() => null), errors };
  console.error(details);
  if (process.env.GITHUB_ACTIONS === 'true') console.log(`::error title=Equipment art smoke test::${(error.stack + '\n' + JSON.stringify(details)).replaceAll('%', '%25').replaceAll('\r', '%0D').replaceAll('\n', '%0A')}`);
  await page.screenshot({ path: `${artifacts}/equipment-failure.png` }).catch(() => {});
  process.exitCode = 1;
} finally { await browser.close(); }
