// Verify both production entry points work with all test diagnostics removed.
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
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
page.on('dialog', dialog => dialog.accept());
const errors = [];
page.on('pageerror', e => errors.push(e.message));
page.on('console', m => { if (m.type() === 'error') errors.push(m.text()); });
async function nextFrames() {
  await page.evaluate(() => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve))));
}
async function record(key) {
  return page.evaluate(key => new Promise((resolve, reject) => {
    const open = indexedDB.open('ashfall-saves', 1);
    open.onerror = () => reject(open.error);
    open.onsuccess = () => {
      const db = open.result, tx = db.transaction('snapshots', 'readonly'), get = tx.objectStore('snapshots').get(key);
      tx.oncomplete = () => { db.close(); resolve(get.result ? [...get.result] : null); };
      tx.onabort = tx.onerror = () => { db.close(); reject(tx.error); };
    };
  }), key);
}
async function waitRecord(key, present) {
  const deadline = Date.now() + 10000;
  do { const bytes = await record(key); if (!!bytes === present) return bytes; await page.waitForTimeout(50); } while (Date.now() < deadline);
  throw new Error(`Persistence record ${key} did not become ${present ? 'present' : 'empty'}`);
}
try {
  await page.goto(process.env.GAME_URL || 'http://127.0.0.1:8080/index.html', { waitUntil: 'networkidle' });
  await page.waitForFunction(() => document.querySelector('#loading').classList.contains('done'));
  assert.equal(await page.evaluate(() => typeof Module._ashfall_get_state), 'undefined');
  assert.equal(await page.evaluate(() => typeof Module._ashfall_get_loot_state), 'undefined');
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
  await nextFrames(); // No mutation/state test hooks in the release build.
  await page.keyboard.press('Enter');
  await nextFrames();
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
  // Explicit gameplay persistence works with no state/debug API. Observe the
  // production file-delivery boundary only to acknowledge the asynchronous Load.
  await page.keyboard.press('k');
  const savedRun = Buffer.from(await waitRecord('run', true));
  assert.equal(savedRun.subarray(0, 8).toString('ascii'), 'ASHRUN\0\0');
  await page.keyboard.press('r'); await nextFrames();
  await page.evaluate(() => {
    window.__runReceived = 0;
    const receive = Module._ashfall_snapshot_receive;
    Module._ashfall_snapshot_receive = function (size) { receive(size); ++window.__runReceived; };
  });
  await page.keyboard.press('l'); await page.waitForFunction(() => window.__runReceived === 1);
  const runDownload = page.waitForEvent('download'); await page.mouse.click(600, 479);
  const run = await runDownload;
  assert.equal(run.suggestedFilename(), 'run.ashrun');
  assert.deepEqual(await readFile(await run.path()), savedRun);
  assert.equal(await page.evaluate(() => window.__gearUploads), 2, 'loading gameplay does not re-upload or remove applied artwork');
  // Inventory and post-victory loot also work without the diagnostic APIs.
  // Observe only the existing storage completion callback, never mutate a World.
  await page.evaluate(() => {
    window.__runWrites = 0;
    const status = Module._ashfall_snapshot_status;
    Module._ashfall_snapshot_status = function (code) { status(code); if (code === 1) ++window.__runWrites; };
  });
  async function importFixture(name) {
    const received = await page.evaluate(() => window.__runReceived);
    const choose = page.waitForEvent('filechooser'); await nextFrames(); await page.mouse.click(850, 479);
    await (await choose).setFiles(resolve('tests/fixtures', name));
    await page.waitForFunction(before => window.__runReceived === before + 1, received); await nextFrames();
  }
  async function saveInventory() {
    const writes = await page.evaluate(() => window.__runWrites);
    await page.keyboard.press('k'); await page.waitForFunction(before => window.__runWrites === before + 1, writes);
    return Buffer.from(await waitRecord('run', true));
  }
  await importFixture('legacy-run-v1.ashrun');
  await page.keyboard.press('b'); await nextFrames();
  assert.equal(await page.locator('#sprite-editor-link').isVisible(), false);
  await page.mouse.click(360, 212); await nextFrames(); // Select the worn crown.
  await page.mouse.click(978, 638); await nextFrames(); // Unequip to the empty bag.
  const inventorySave = await saveInventory();
  assert.equal(inventorySave.readUInt32LE(20), 2, 'saving migrates a v1 run to v2');
  assert.equal(inventorySave.readUInt32LE(104), 0, 'known fixture has no player waypoints');
  assert.deepEqual([...inventorySave.subarray(112, 115)], [255, 2, 2], 'production unequip records an empty head slot');
  assert.equal(inventorySave.readUInt32LE(127), 30, 'moving gear is not a currency transaction');
  await page.keyboard.press('b'); await nextFrames();
  await importFixture('victory-loot-v2.ashrun');
  await page.mouse.click(720, 613); await nextFrames(); // Collect the spoils, not Restart.
  await page.keyboard.press('e'); await nextFrames();
  const spoils = await saveInventory();
  assert.equal(spoils.readUInt32LE(104), 0);
  assert.equal(spoils.readUInt32LE(127), 45, 'production victory pickup grants the Warden ember cache');
  assert.equal(await page.evaluate(() => window.__gearUploads), 2, 'inventory and loot do not re-upload applied artwork');
  assert.equal(await page.evaluate(() => typeof Module._ashfall_get_loot_state), 'undefined');
  await page.getByRole('link', { name: 'Open the Sprite Workshop editor' }).click();
  await page.waitForURL('**/editor.html');
  await page.waitForFunction(() => typeof Module._ashfall_editor_is_dirty === 'function' && !!Module.snapshotStore);
  await page.keyboard.press('Control+n'); await nextFrames();
  await page.keyboard.press('Enter'); await nextFrames();
  await page.mouse.click(612, 498); await page.waitForFunction(() => Module._ashfall_editor_is_dirty() === 1);
  const draft = Buffer.from(await waitRecord('editor', true));
  assert.equal(draft.subarray(0, 8).toString('ascii'), 'ASHDRAFT');
  assert.equal(await page.evaluate(() => Module._ashfall_editor_is_dirty()), 1, 'autosave is not an exported save point');
  await page.reload();
  await page.waitForFunction(() => typeof Module._ashfall_editor_is_dirty === 'function' && !!Module.snapshotStore);
  // Canvas controls deliberately don't add a release-only mutation/diagnostic
  // hook. Enter is harmless until the asynchronous recovery prompt is ready.
  for (let attempt = 0; attempt < 30 && !await page.evaluate(() => Module._ashfall_editor_is_dirty()); ++attempt) {
    await page.keyboard.press('Enter'); await page.waitForTimeout(100);
  }
  await page.waitForFunction(() => Module._ashfall_editor_is_dirty() === 1);
  const recoveredDownload = page.waitForEvent('download'); await page.keyboard.press('Control+s');
  const recovered = await recoveredDownload, recoveredImage = decodePng(await readFile(await recovered.path()));
  assert.equal(recovered.suggestedFilename(), 'untitled.png');
  assert.equal(recoveredImage.pixels.readUInt32BE((16 * 32 + 16) * 4), 0xdab270ff);
  await page.waitForFunction(() => Module._ashfall_editor_is_dirty() === 0);
  await waitRecord('editor', false);
  assert.equal(await page.evaluate(() => typeof Module._ashfall_editor_get_state), 'undefined');
  assert.deepEqual(errors, []);
  console.log('[PASS] Production PNG/project export, Apply/import/GPU overrides, gameplay save/load/export, owned inventory/victory loot and automatic draft recovery; diagnostics absent');
} catch (error) {
  console.error(error);
  process.exitCode = 1;
} finally {
  await browser.close();
}
