// Verify both production entry points work with all test diagnostics removed.
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { decodePng } from './png_helpers.mjs';
const { chromium } = await import(process.env.PLAYWRIGHT_MODULE || 'playwright');
const browser = await chromium.launch({
  headless: true, executablePath: process.env.CHROMIUM_EXECUTABLE || undefined,
  args: ['--enable-unsafe-swiftshader', '--use-angle=swiftshader', ...JSON.parse(process.env.CHROMIUM_ARGS || '[]')],
});
const page = await browser.newPage({ viewport: { width: 1440, height: 900 } });
// Observe real production uploads without enabling any application diagnostic.
await page.addInitScript(() => {
  window.__gearUploads = 0;
  const proto = WebGLRenderingContext.prototype, image = proto.texImage2D;
  proto.texImage2D = function (...args) {
    if (args.length === 9 && args[3] === 96 && args[4] === 96) ++window.__gearUploads;
    return image.apply(this, args);
  };
});
const errors = [];
page.on('pageerror', e => errors.push(e.message));
page.on('console', m => { if (m.type() === 'error') errors.push(m.text()); });
try {
  await page.goto(process.env.GAME_URL || 'http://127.0.0.1:8080/index.html', { waitUntil: 'networkidle' });
  await page.waitForFunction(() => document.querySelector('#loading').classList.contains('done'));
  assert.equal(await page.evaluate(() => typeof Module._ashfall_get_state), 'undefined');
  assert.equal(await page.locator('#canvas').evaluate(e => !!e.getContext('webgl')), true);
  await page.getByRole('link', { name: 'Open the Sprite Workshop editor' }).click();
  await page.waitForURL('**/editor.html');
  await page.waitForFunction(() => document.querySelector('#loading').classList.contains('done') &&
    typeof Module._ashfall_editor_is_dirty === 'function');
  assert.equal(await page.evaluate(() => typeof Module._ashfall_editor_get_state), 'undefined');
  assert.equal(await page.evaluate(() => typeof Module._ashfall_editor_pixel), 'undefined');
  assert.equal(await page.evaluate(() => Module._ashfall_editor_is_dirty()), 0);
  assert.equal(await page.locator('#canvas').evaluate(e => !!e.getContext('webgl')), true);
  await page.keyboard.press('Control+n');
  await page.waitForTimeout(120); // No mutation/state test hooks in the release build.
  await page.keyboard.press('Enter');
  await page.waitForTimeout(120);
  await page.mouse.click(612, 498);
  await page.waitForFunction(() => Module._ashfall_editor_is_dirty() === 1);
  const pending = page.waitForEvent('download');
  await page.keyboard.press('Control+s');
  const download = await pending;
  assert.equal(download.suggestedFilename(), 'untitled.png');
  const image = decodePng(await readFile(await download.path()));
  assert.equal(image.width, 32); assert.equal(image.height, 32);
  assert.equal(image.pixels.readUInt32BE((16 * 32 + 16) * 4), 0xdab270ff);
  assert.equal(image.pixels.readUInt32BE(0), 0, 'background stays transparent');
  await page.waitForFunction(() => Module._ashfall_editor_is_dirty() === 0);
  await page.mouse.click(405, 37); // Built-in Gear, from a saved standalone PNG.
  await page.waitForFunction(() => window.__gearUploads > 0);
  await page.mouse.click(430, 401); // Full 96px cell, outside the old armor crop.
  await page.waitForFunction(() => Module._ashfall_editor_is_dirty() === 1);
  const projectDownload = page.waitForEvent('download'); await page.keyboard.press('Control+s');
  const project = await projectDownload, bytes = await readFile(await project.path());
  assert.equal(project.suggestedFilename(), 'equipment.ashgear');
  assert.equal(bytes.subarray(0, 8).toString('ascii'), 'ASHGEAR\0');
  assert.equal(bytes.readUInt32LE(24), 1, 'one modified equipment frame, exported without diagnostics');
  await page.waitForFunction(() => Module._ashfall_editor_is_dirty() === 0);
  await page.mouse.click(620, 226); // Explicit Apply to Game.
  await page.waitForFunction(expected => new Promise(resolve => {
    const open = indexedDB.open('ashfall-equipment', 1);
    open.onerror = () => resolve(false);
    open.onsuccess = () => {
      const db = open.result, tx = db.transaction('art', 'readonly'), get = tx.objectStore('art').get('applied');
      get.onsuccess = () => resolve(get.result && new Uint8Array(get.result).length === expected.length &&
        expected.every((v, i) => new Uint8Array(get.result)[i] === v));
      get.onerror = () => resolve(false); tx.oncomplete = () => db.close();
    };
  }), [...bytes]);
  assert.equal(await page.evaluate(() => Module._ashfall_editor_is_dirty()), 0);
  await page.mouse.click(36, 37); // Saved canvas returns to the game without a guard.
  await page.waitForURL('**/index.html');
  await page.waitForFunction(() => document.querySelector('#loading').classList.contains('done'));
  await page.waitForFunction(() => window.__gearUploads === 1); // Applied full-cell gear actually reached the renderer.
  assert.equal(await page.evaluate(() => typeof Module._ashfall_get_state), 'undefined');
  const picker = page.waitForEvent('filechooser'); await page.keyboard.press('u');
  await (await picker).setFiles(await project.path());
  await page.waitForFunction(() => window.__gearUploads === 2); // Production explicit file-import path, too.
  assert.deepEqual(errors, []);
  console.log('[PASS] Production game/editor startup, PNG/project downloads, explicit apply/import, real GPU overrides and navigation; diagnostics absent');
} catch (error) {
  console.error(error);
  process.exitCode = 1;
} finally {
  await browser.close();
}
