// Real input + actual WASM/IndexedDB. The only injected behaviors are storage
// delay/quota/denial, never mutations of the C++ world or document under test.
import assert from 'node:assert/strict';
import { mkdir, readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { decodePng } from './png_helpers.mjs';
const { chromium } = await import(process.env.PLAYWRIGHT_MODULE || 'playwright');
const artifacts = resolve(process.env.ARTIFACTS || 'build/browser');
await mkdir(artifacts, { recursive: true });
const browser = await chromium.launch({
  headless: true, executablePath: process.env.CHROMIUM_EXECUTABLE || undefined,
  args: ['--enable-unsafe-swiftshader', '--use-angle=swiftshader', ...JSON.parse(process.env.CHROMIUM_ARGS || '[]')],
});
const context = await browser.newContext({ viewport: { width: 1440, height: 900 } });
const game = await context.newPage(), editor = await context.newPage(), errors = [];
for (const page of [game, editor]) {
  page.on('pageerror', e => errors.push(e.message));
  page.on('console', m => { if (m.type() === 'error') errors.push(m.text()); });
  page.on('dialog', d => d.accept()); // Explicitly consent to reload of unexported, autosaved work.
}
await context.addInitScript(() => {
  window.__snapshotOpenDelay = 0; window.__failSnapshotWrites = false; window.__snapshotPuts = 0;
  window.__holdSnapshotOpens = false; window.__heldSnapshotOpens = [];
  window.__releaseSnapshotOpens = function () {
    window.__holdSnapshotOpens = false;
    for (const finish of window.__heldSnapshotOpens.splice(0)) finish();
  };
  const open = IDBFactory.prototype.open;
  IDBFactory.prototype.open = function (name, ...args) {
    const request = open.call(this, name, ...args), delay = window.__snapshotOpenDelay;
    if (name !== 'ashfall-saves' || (!delay && !window.__holdSnapshotOpens)) return request;
    return new Proxy(request, {
      get(target, key) { const value = Reflect.get(target, key, target); return typeof value === 'function' ? value.bind(target) : value; },
      set(target, key, value) {
        if (key === 'onsuccess') return Reflect.set(target, key, event => {
          const finish = () => value(event);
          if (window.__holdSnapshotOpens) window.__heldSnapshotOpens.push(finish);
          else setTimeout(finish, delay);
        }, target);
        return Reflect.set(target, key, value, target);
      },
    });
  };
  const put = IDBObjectStore.prototype.put;
  IDBObjectStore.prototype.put = function (...args) {
    if (this.transaction.db.name === 'ashfall-saves') ++window.__snapshotPuts;
    if (window.__failSnapshotWrites && this.transaction.db.name === 'ashfall-saves') throw new DOMException('Injected quota failure', 'QuotaExceededError');
    return put.apply(this, args);
  };
});
const gameState = () => game.evaluate(() => JSON.parse(Module.UTF8ToString(Module._ashfall_get_state())));
const editorState = () => editor.evaluate(() => JSON.parse(Module.UTF8ToString(Module._ashfall_editor_get_state())));
async function wait(read, predicate, label, timeout = 20000) {
  const deadline = Date.now() + timeout; let last;
  do { last = await read(); if (predicate(last)) return last; await new Promise(r => setTimeout(r, 40)); } while (Date.now() < deadline);
  throw new Error(`${label}: ${JSON.stringify(last)}`);
}
const gameUntil = (predicate, label) => wait(gameState, predicate, label);
const editorUntil = (predicate, label) => wait(editorState, predicate, label);
const pixel = (x, y) => editor.evaluate(([x, y]) => Module._ashfall_editor_pixel(x, y) >>> 0, [x, y]);
function gameplay(s) {
  const { health, potions, armor, head, body, boots, x, y, moving, pathSize, pathNext, kills, enemies, elapsed,
    selectedId, targetAction, attack, attackSerial, attackCooldown, attackElapsed, dashRemaining, dashCooldown, targets } = s;
  return { health, potions, armor, head, body, boots, x, y, moving, pathSize, pathNext, kills, enemies, elapsed,
    selectedId, targetAction, attack, attackSerial, attackCooldown, attackElapsed, dashRemaining, dashCooldown, targets };
}
async function stored(page, key) {
  return page.evaluate(key => new Promise((resolve, reject) => {
    const open = indexedDB.open('ashfall-saves', 1);
    open.onerror = () => reject(open.error);
    open.onsuccess = () => {
      const db = open.result, tx = db.transaction('snapshots', 'readonly'), request = tx.objectStore('snapshots').get(key);
      tx.oncomplete = () => { db.close(); resolve(request.result ? [...request.result] : null); };
      tx.onabort = tx.onerror = () => { db.close(); reject(tx.error); };
    };
  }), key);
}
async function putRecord(page, key, bytes) {
  await page.evaluate(([key, bytes]) => new Promise((resolve, reject) => {
    const open = indexedDB.open('ashfall-saves', 1);
    open.onerror = () => reject(open.error);
    open.onsuccess = () => {
      const db = open.result, tx = db.transaction('snapshots', 'readwrite'), store = tx.objectStore('snapshots');
      if (bytes) store.put(new Uint8Array(bytes), key); else store.delete(key);
      tx.oncomplete = () => { db.close(); resolve(); };
      tx.onabort = tx.onerror = () => { db.close(); reject(tx.error); };
    };
  }), [key, bytes ? [...bytes] : null]);
}
async function recordUntil(page, key, predicate, label) {
  return wait(() => stored(page, key), predicate, label);
}
async function restart() {
  const before = await gameState(); await game.keyboard.press('r');
  return gameUntil(s => s.restartSerial > before.restartSerial, 'restart acknowledgement');
}
async function gameDrawn() {
  await game.evaluate(() => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve))));
}
async function exportRun() {
  await game.bringToFront(); await gameDrawn();
  const pending = game.waitForEvent('download'); await game.mouse.click(600, 479);
  const file = await pending; assert.equal(file.suggestedFilename(), 'run.ashrun');
  return readFile(await file.path());
}
async function cell(x, y) {
  const s = await editorState(); return { x: s.canvas.x + (x + 0.5) * s.zoom, y: s.canvas.y + (y + 0.5) * s.zoom };
}
async function paint(from, to = from) {
  const a = await cell(...from), b = await cell(...to), before = await editorState();
  await editor.mouse.move(a.x, a.y); await editor.mouse.down(); await editor.mouse.move(b.x, b.y, { steps: 3 }); await editor.mouse.up();
  return editorUntil(s => s.stamp > before.stamp && s.dirty, 'paint transaction');
}
async function confirmRecovery() {
  const s = await editorUntil(s => s.modal === 9, 'recovery prompt');
  await editor.mouse.click(s.confirm.x + 40, s.confirm.y + 16);
  return editorUntil(s => s.modal === 0 && s.dirty && !s.recoveryAvailable, 'recover without marking exported');
}
async function fresh() {
  await editor.keyboard.press('Control+n'); await editorUntil(s => s.modal === 1, 'new canvas');
  await editor.keyboard.press('Enter'); await editorUntil(s => s.modal === 0 && !s.dirty && !s.equipment, 'fresh canvas');
}
async function saveEditor() {
  await editor.bringToFront();
  const pending = editor.waitForEvent('download'); await editor.keyboard.press('Control+s');
  const file = await pending; const bytes = await readFile(await file.path());
  await editorUntil(s => !s.dirty, 'explicit exported save point');
  await recordUntil(editor, 'editor', bytes => bytes === null, 'export clears recovery record');
  return { bytes, name: file.suggestedFilename() };
}
const gameUrl = process.env.GAME_URL || 'http://127.0.0.1:8080/index.html';
const editorUrl = process.env.EDITOR_URL || new URL('editor.html', gameUrl).href;
try {
  await game.bringToFront(); await game.goto(gameUrl); await game.waitForFunction(() => !!Module._ashfall_get_state);
  await game.keyboard.press('l');
  await gameUntil(s => s.paused && s.saveStatus === 0, 'missing save handled');
  await game.keyboard.press('p'); await gameUntil(s => !s.paused, 'resume');
  await game.keyboard.press('1'); await gameUntil(s => s.head === 1, 'change actual equipment');
  const target = (await gameState()).targets[0]; await game.mouse.click(target.screenX, target.screenY - 15);
  await gameUntil(s => s.selectedId === target.id && s.moving, 'start pursuit');
  await game.keyboard.press('p'); const before = await gameUntil(s => s.paused, 'freeze save checkpoint');
  await game.keyboard.press('k'); await gameUntil(s => s.saveStatus === 1 && s.saveSerial > 0, 'save commits');
  const saved = Buffer.from(await stored(game, 'run'));
  assert.equal(saved.subarray(0, 8).toString('ascii'), 'ASHRUN\0\0');
  assert.deepEqual(await exportRun(), saved);
  await restart(); await game.keyboard.press('l');
  const loaded = await gameUntil(s => s.loadSerial === before.loadSerial + 1 && s.paused, 'load from local slot');
  assert.deepEqual(gameplay(loaded), gameplay(before)); assert.deepEqual(await exportRun(), saved);
  await game.screenshot({ path: `${artifacts}/saved-run-paused.png` });
  await game.reload(); await game.waitForFunction(() => !!Module._ashfall_get_state);
  assert.equal((await gameState()).loadSerial, 0, 'a saved run never replaces a new descent without Load');
  await game.keyboard.press('l'); await gameUntil(s => s.loadSerial === 1, 'persistent load after reload');
  assert.deepEqual(gameplay(await gameState()), gameplay(before));
  console.log('[PASS] Manual save/load, full navigation/target/stat/timer round trip, portable export, persistent reload and no implicit autoload');

  await restart(); await game.keyboard.press('p'); await gameUntil(s => s.paused, 'pause for file import');
  await gameDrawn();
  const chooser = game.waitForEvent('filechooser'); await game.mouse.click(850, 479);
  await (await chooser).setFiles({ name: 'resume.ashrun', mimeType: 'application/octet-stream', buffer: saved });
  await gameUntil(s => s.loadSerial === 2 && s.paused, 'production file-picker import');
  assert.deepEqual(await exportRun(), saved);
  const intact = gameplay(await gameState()), serial = (await gameState()).loadSerial;
  const corrupt = Buffer.from(saved); corrupt[corrupt.length - 1] ^= 1;
  let eventSerial = (await gameState()).saveEventSerial;
  await game.locator('#run-file').setInputFiles({ name: 'bad.ashrun', mimeType: 'application/octet-stream', buffer: corrupt });
  await gameUntil(s => s.saveEventSerial > eventSerial && s.saveStatus === -3, 'checksum rejection');
  assert.deepEqual(gameplay(await gameState()), intact); assert.equal((await gameState()).loadSerial, serial);
  eventSerial = (await gameState()).saveEventSerial;
  await game.locator('#run-file').setInputFiles({ name: 'huge.ashrun', mimeType: 'application/octet-stream', buffer: Buffer.alloc(131073) });
  await gameUntil(s => s.saveEventSerial > eventSerial && s.saveStatus === -3, 'oversize rejection'); assert.deepEqual(gameplay(await gameState()), intact);
  await putRecord(game, 'run', corrupt); eventSerial = (await gameState()).saveEventSerial; await game.keyboard.press('l');
  await gameUntil(s => s.saveEventSerial > eventSerial && s.saveStatus === -3, 'stored corruption rejection'); assert.deepEqual(gameplay(await gameState()), intact);
  await putRecord(game, 'run', saved);
  await game.evaluate(() => { window.__snapshotOpenDelay = 1600; });
  await game.keyboard.press('l'); await gameUntil(s => s.saveStatus === 3, 'delayed read in flight');
  const freshRun = await restart(); await game.waitForTimeout(2000);
  assert.equal((await gameState()).loadSerial, serial, 'stale read cannot replace an explicitly restarted run');
  assert.equal((await gameState()).restartSerial, freshRun.restartSerial);
  await game.evaluate(() => { window.__snapshotOpenDelay = 0; });
  await game.keyboard.press('p'); await gameUntil(s => s.paused, 'pause after delayed load');
  console.log('[PASS] Real file picker, checksum/size rejection without partial replacement, corrupt slot safety and stale-load cancellation');

  await editor.bringToFront(); await editor.goto(editorUrl); await editor.waitForFunction(() => !!Module._ashfall_editor_get_state);
  await editorUntil(s => s.recoveryReady && !s.recoveryAvailable, 'initial recovery check');
  await fresh();
  const a = await cell(2, 3), b = await cell(10, 3);
  await editor.mouse.move(a.x, a.y); await editor.mouse.down(); await editor.mouse.move(b.x, b.y, { steps: 3 });
  await editorUntil(s => s.autosaveStatus === 1, 'autosave during held stroke');
  assert.equal((await editorState()).undo, 0, 'checkpoint never commits/splits the live transaction');
  const checkpoint = Buffer.from(await stored(editor, 'editor'));
  assert.equal(checkpoint.subarray(0, 8).toString('ascii'), 'ASHDRAFT');
  await editor.mouse.up(); await editorUntil(s => s.undo === 1 && s.dirty, 'stroke still one undo step');
  await editor.reload(); await editor.waitForFunction(() => !!Module._ashfall_editor_get_state);
  await editorUntil(s => s.modal === 9 && !s.dirty, 'startup recovery is a choice, not an overwrite');
  await editor.keyboard.press('Escape'); await editorUntil(s => s.modal === 0 && s.recoveryAvailable, 'defer recovery');
  await paint([20, 20]); await editor.waitForTimeout(2200);
  assert.deepEqual(Buffer.from(await stored(editor, 'editor')), checkpoint, 'new strokes cannot overwrite an undecided draft');
  await editor.keyboard.press('F2'); await confirmRecovery();
  for (let x = 2; x <= 10; ++x) assert.equal(await pixel(x, 3), 0xdab270ff);
  assert.equal(await pixel(20, 20), 0, 'explicit recovery replaces the temporary new canvas');
  assert.equal((await editorState()).undo, 0); assert.equal((await editorState()).dirty, true);
  const exported = await saveEditor(), png = decodePng(exported.bytes);
  assert.equal(exported.name, 'untitled.png'); assert.equal(png.pixels.readUInt32BE((3 * 32 + 2) * 4), 0xdab270ff);
  await editor.reload(); await editor.waitForFunction(() => !!Module._ashfall_editor_get_state);
  await editorUntil(s => s.recoveryReady, 'clean startup'); assert.equal((await editorState()).modal, 0);
  console.log('[PASS] Automatic mid-stroke checkpoints, one undo transaction, protected startup/deferred recovery, dirty restoration and explicit PNG-save cleanup');

  await editor.mouse.click(405, 37); await editorUntil(s => s.equipment && s.previewReady, 'built-in gear');
  await editor.mouse.click(178, 188); await editorUntil(s => s.clip === 1, 'walk clip');
  await paint([1, 1], [3, 1]);
  await editor.mouse.click(128, 226); await editorUntil(s => s.frame === 1, 'second frame');
  await paint([8, 2]);
  await editor.mouse.click(420, 226); await editorUntil(s => !s.references, 'solo setting');
  await editorUntil(s => s.autosaveStatus === 1 && !s.autosavePending && s.framesEdited === 2, 'whole-project autosave');
  await editor.reload(); await editor.waitForFunction(() => !!Module._ashfall_editor_get_state); await confirmRecovery();
  let recovered = await editorState();
  assert.equal(recovered.equipment, true); assert.equal(recovered.framesEdited, 2);
  assert.equal(recovered.clip, 1); assert.equal(recovered.frame, 1); assert.equal(recovered.references, false); assert.equal(recovered.undo, 0);
  assert.equal(await pixel(8, 2), 0xdab270ff);
  await editor.mouse.click(94, 226); await editorUntil(s => s.frame === 0, 'recovered first frame');
  assert.equal(await pixel(1, 1), 0xdab270ff);
  const frameDownload = editor.waitForEvent('download'); await editor.mouse.click(140, 264); await frameDownload;
  assert.equal((await editorState()).dirty, true); assert.ok(await stored(editor, 'editor'), 'frame PNG cannot clear whole-project recovery');
  await editor.mouse.click(620, 226); await editorUntil(s => s.applyStatus === 1, 'Apply remains separate from a project export');
  assert.equal((await editorState()).dirty, true); assert.ok(await stored(editor, 'editor'));
  await gameUntil(s => s.artFrames === 2, 'game gets explicitly applied art');
  const art = (await gameState()).artHash;
  await game.bringToFront(); await game.keyboard.press('l'); await gameUntil(s => s.loadSerial > serial, 'game save with independently applied artwork');
  assert.equal((await gameState()).artHash, art, 'loading gameplay never restores or removes equipment artwork');
  const project = await saveEditor(); assert.equal(project.name, 'equipment.ashgear');
  assert.equal(project.bytes.readUInt32LE(24), 2); assert.equal((await editorState()).dirty, false);
  console.log('[PASS] Whole gear project/frame/settings recovery, fresh undo, PNG/Apply/export distinctions and gameplay/artwork separation');

  // Force an earlier draft to arrive after the user has already begun editing.
  await putRecord(editor, 'editor', checkpoint);
  await editor.addInitScript(() => { window.__snapshotOpenDelay = 1700; });
  await editor.reload(); await editor.waitForFunction(() => !!Module._ashfall_editor_get_state);
  await editorUntil(s => !s.recoveryReady, 'recovery read is genuinely delayed');
  await paint([25, 4]);
  await editorUntil(s => s.recoveryReady && s.recoveryAvailable, 'late recovery candidate');
  assert.equal((await editorState()).modal, 0); assert.equal(await pixel(25, 4), 0xdab270ff, 'late I/O never replaces current edits');
  await editor.evaluate(() => { window.__snapshotOpenDelay = 0; });
  assert.deepEqual(Buffer.from(await stored(editor, 'editor')), checkpoint);
  await editor.keyboard.press('F2'); const choice = await editorUntil(s => s.modal === 9, 'late candidate menu');
  await editor.mouse.click(choice.dialog.x + 60, choice.confirm.y + 16); // Explicit discard, not recover.
  await editorUntil(s => !s.recoveryAvailable && s.modal === 0, 'discard prior draft');
  await editorUntil(s => s.autosaveStatus === 1, 'new work can autosave after discard');
  assert.equal(await pixel(25, 4), 0xdab270ff);
  assert.notDeepEqual(Buffer.from(await stored(editor, 'editor')), checkpoint);
  await saveEditor();
  await putRecord(editor, 'editor', Buffer.from([1, 2, 3, 4]));
  await editor.keyboard.press('F2'); await editorUntil(s => s.modal === 9 && s.autosaveStatus === -3, 'corrupt recovery offered safely');
  const badState = await editorState(); await editor.keyboard.press('Enter'); await editor.waitForTimeout(100);
  assert.equal((await editorState()).modal, 9); assert.equal((await editorState()).stamp, badState.stamp);
  await editor.mouse.click(badState.dialog.x + 60, badState.confirm.y + 16);
  await editorUntil(s => !s.recoveryAvailable && s.modal === 0, 'discard unreadable recovery');
  await recordUntil(editor, 'editor', value => value === null, 'corrupt draft removed only by consent');
  console.log('[PASS] Late-read/new-stroke race, explicit discard, safe corrupt-draft handling and no automatic data loss');

  await editor.evaluate(() => { window.__failSnapshotWrites = true; });
  await paint([24, 6]); await editorUntil(s => s.autosaveStatus === -2, 'quota failure reported');
  assert.equal((await editorState()).dirty, true); assert.equal(await stored(editor, 'editor'), null);
  await editor.evaluate(() => { window.__failSnapshotWrites = false; });
  await editor.keyboard.press('F2'); await editorUntil(s => s.recoveryReady && s.autosaveStatus === 1, 'F2 retries failed autosave');
  assert.ok(await stored(editor, 'editor')); await saveEditor();
  // Hold the first write open, queue more snapshots, then explicitly export.
  // Only the active snapshot + latest clear may reach storage; older queued
  // strokes must not reappear after a save or accumulate whole-project copies.
  // Use an explicit I/O latch, not a five-second assumption about software-GPU
  // frame speed: the first job must remain active until every later edit and
  // the export/clear have actually been submitted.
  const puts = await editor.evaluate(() => { window.__holdSnapshotOpens = true; return window.__snapshotPuts; });
  for (let x = 21; x <= 23; ++x) {
    const attempt = (await editorState()).autosaveSerial;
    await paint([x, 7]);
    await editorUntil(s => s.autosaveSerial > attempt && s.autosaveStatus === 3 && !s.autosavePending, 'queued checkpoint during slow storage');
    if (x === 21) await editor.waitForFunction(() => window.__heldSnapshotOpens.length === 1);
  }
  const finalDownload = editor.waitForEvent('download'); await editor.keyboard.press('Control+s'); await finalDownload;
  await editorUntil(s => !s.dirty, 'export during pending writes');
  assert.equal(await editor.evaluate(() => window.__heldSnapshotOpens.length), 1, 'only one active storage operation while edits coalesce');
  await editor.evaluate(() => window.__releaseSnapshotOpens());
  await editorUntil(s => s.autosaveStatus === 2 && !s.autosavePending, 'last queued clear commits');
  assert.equal(await stored(editor, 'editor'), null);
  assert.equal(await editor.evaluate(() => window.__snapshotPuts), puts + 1, 'superseded snapshots are coalesced, not written after the export');
  // At most one active editor owns the recovery writer on browsers with Web Locks.
  const other = await context.newPage(); other.on('dialog', d => d.accept());
  await other.goto(editorUrl); await other.waitForFunction(() => !!Module._ashfall_editor_get_state);
  if (await other.evaluate(() => !!navigator.locks)) {
    await other.waitForFunction(() => JSON.parse(Module.UTF8ToString(Module._ashfall_editor_get_state())).autosaveStatus === -4);
    assert.equal(await stored(other, 'editor'), null);
  }
  await other.close(); await editor.bringToFront();
  await editor.screenshot({ path: `${artifacts}/editor-recovery-workflow.png` });
  console.log('[PASS] Quota failure keeps work dirty, retry succeeds, slow writes coalesce before export/clear and a second editor cannot clobber recovery');

  // Separate browser context so no permission or deliberately denied database
  // can poison the main origin's successful persistence checks above.
  const denied = await browser.newContext({ viewport: { width: 1440, height: 900 } });
  await denied.addInitScript(() => { indexedDB.open = function () { throw new DOMException('Injected denial', 'SecurityError'); }; });
  const blocked = await denied.newPage(); blocked.on('dialog', d => d.accept());
  await blocked.goto(gameUrl); await blocked.waitForFunction(() => !!Module._ashfall_get_state);
  await blocked.keyboard.press('k');
  await blocked.waitForFunction(() => JSON.parse(Module.UTF8ToString(Module._ashfall_get_state())).saveStatus === -2);
  await blocked.goto(editorUrl); await blocked.waitForFunction(() => !!Module._ashfall_editor_get_state);
  await blocked.waitForFunction(() => JSON.parse(Module.UTF8ToString(Module._ashfall_editor_get_state())).autosaveStatus === -1);
  await blocked.mouse.click(612, 498); await blocked.waitForFunction(() => Module._ashfall_editor_is_dirty() === 1);
  const download = blocked.waitForEvent('download'); await blocked.keyboard.press('Control+s');
  assert.equal((await download).suggestedFilename(), 'wayfarer.png');
  await blocked.waitForFunction(() => Module._ashfall_editor_is_dirty() === 0);
  await denied.close();
  assert.deepEqual(errors, []);
  console.log('[PASS] Denied IndexedDB is nonfatal in both apps; portable PNG export still works and only explicit export marks saved');
  console.log('All persistence browser checks passed.');
} catch (error) {
  console.error(error);
  const details = { game: await gameState().catch(() => null), editor: await editorState().catch(() => null), errors };
  console.error(details);
  if (process.env.GITHUB_ACTIONS === 'true') console.log(`::error title=Persistence smoke test::${(error.stack + '\n' + JSON.stringify(details)).replaceAll('%', '%25').replaceAll('\r', '%0D').replaceAll('\n', '%0A')}`);
  await game.screenshot({ path: `${artifacts}/persistence-game-failure.png` }).catch(() => {});
  await editor.screenshot({ path: `${artifacts}/persistence-failure.png` }).catch(() => {});
  process.exitCode = 1;
} finally { await browser.close(); }
