// Actual C++/WASM gameplay and inventory input. Diagnostics are READ ONLY.
// Synthetic high-capacity/legacy/victory snapshots enter through the ordinary
// portable file picker; no teleport, damage, inventory or timer test API exists.
import assert from 'node:assert/strict';
import { mkdir, readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
const { chromium } = await import(process.env.PLAYWRIGHT_MODULE || 'playwright');
const artifacts = resolve(process.env.ARTIFACTS || 'build/browser');
await mkdir(artifacts, { recursive: true });
const browser = await chromium.launch({ headless: true,
  executablePath: process.env.CHROMIUM_EXECUTABLE || undefined,
  args: ['--enable-unsafe-swiftshader', '--use-angle=swiftshader', ...JSON.parse(process.env.CHROMIUM_ARGS || '[]')],
});
const page = await browser.newPage({ viewport: { width: 1440, height: 900 } });
// Deterministic storage latch, not a wall-clock sleep: exercise newer item
// intent overtaking a Load, and preserve the acknowledgement of a pending Save.
await page.addInitScript(() => {
  window.__ioHeld = false; window.__ioReleases = []; window.__ioClosed = 0;
  window.__releaseIO = () => { window.__ioHeld = false; for (const finish of window.__ioReleases.splice(0)) finish(); };
  window.__fileHeld = false; window.__fileReleases = [];
  window.__releaseFileIO = () => { window.__fileHeld = false; for (const finish of window.__fileReleases.splice(0)) finish(); };
  const readFile = File.prototype.arrayBuffer;
  File.prototype.arrayBuffer = function (...args) {
    const bytes = readFile.apply(this, args);
    if (!window.__fileHeld || !this.name.endsWith('.ashrun')) return bytes;
    return bytes.then(value => new Promise(resolve => window.__fileReleases.push(() => resolve(value))));
  };
  const close = IDBDatabase.prototype.close;
  IDBDatabase.prototype.close = function (...args) { if (this.name === 'ashfall-saves') ++window.__ioClosed; return close.apply(this, args); };
  const open = IDBFactory.prototype.open;
  IDBFactory.prototype.open = function (name, ...args) {
    const request = open.call(this, name, ...args);
    if (name !== 'ashfall-saves' || !window.__ioHeld) return request;
    return new Proxy(request, {
      get(target, key) { const value = Reflect.get(target, key, target); return typeof value === 'function' ? value.bind(target) : value; },
      set(target, key, value) {
        return Reflect.set(target, key, key === 'onsuccess' ? event => {
          const finish = () => value(event); if (window.__ioHeld) window.__ioReleases.push(finish); else finish();
        } : value, target);
      },
    });
  };
});
const errors = [];
page.on('pageerror', e => errors.push(e.message));
page.on('console', m => { if (m.type() === 'error') errors.push(m.text()); });
const state = () => page.evaluate(() => JSON.parse(Module.UTF8ToString(Module._ashfall_get_state())));
const loot = () => page.evaluate(() => JSON.parse(Module.UTF8ToString(Module._ashfall_get_loot_state())));
async function until(read, predicate, label, timeout = 20000) {
  const deadline = Date.now() + timeout;
  let s;
  do { s = await read(); if (predicate(s)) return s; await page.waitForTimeout(50); } while (Date.now() < deadline);
  throw new Error(`${label}: ${JSON.stringify(s)}`);
}
const gameUntil = (p, label, timeout) => until(state, p, label, timeout);
const lootUntil = (p, label, timeout) => until(loot, p, label, timeout);
const drawn = () => page.evaluate(() => new Promise(r => requestAnimationFrame(() => requestAnimationFrame(r))));
async function clickRect(r) { await drawn(); await page.mouse.click(r.x + r.width / 2, r.y + r.height / 2); await drawn(); }
async function button(name) { await clickRect((await loot()).buttons[name]); }
async function pick(id) {
  const l = await loot(), item = [...l.bag, ...l.worn].find(i => i.id === id);
  assert.ok(item, `owned item ${id}`); await clickRect(item.rect);
  await lootUntil(s => s.selectedItem === id, 'item selection');
}
async function bag(open) {
  if ((await loot()).open !== open) await page.keyboard.press('b');
  await lootUntil(s => s.open === open, open ? 'backpack opens' : 'backpack closes'); await drawn();
}
async function pause() {
  if (!(await state()).paused) { await page.keyboard.press('p'); await gameUntil(s => s.paused, 'pause'); }
  await drawn();
}
async function restart() {
  const serial = (await state()).restartSerial;
  await page.keyboard.press('r'); await gameUntil(s => s.restartSerial === serial + 1, 'restart acknowledgement'); await drawn();
}
async function importRun(path) {
  await bag(false); await pause();
  const serial = (await state()).loadSerial, picker = page.waitForEvent('filechooser');
  await page.mouse.click(850, 479); await (await picker).setFiles(path);
  await gameUntil(s => s.loadSerial === serial + 1 && s.paused, 'portable run file import'); await drawn();
}
function itemData(item) { const { rect, cell, ...data } = item; return data; }
function snapshot(l) {
  return { bag: l.bag.map(({ rect, ...item }) => item), worn: l.worn.map(itemData),
    ground: l.ground.map(({ screenX, screenY, reachable, ...drop }) => drop),
    embers: l.embers, potions: l.potions, experience: l.experience, lootTarget: l.lootTarget, cleared: l.cleared };
}
function unique(l) {
  const ids = [...l.bag.map(i => i.id), ...l.worn.map(i => i.id), ...l.ground.map(i => i.dropId)];
  assert.equal(new Set(ids).size, ids.length, 'one ownership location per stable ID');
  assert.equal(l.count, l.bag.length); assert.ok(l.count <= 24);
}
async function record() {
  return Buffer.from(await page.evaluate(() => new Promise((resolve, reject) => {
    const request = indexedDB.open('ashfall-saves', 1);
    request.onerror = () => reject(request.error);
    request.onsuccess = () => {
      const db = request.result, tx = db.transaction('snapshots', 'readonly'), get = tx.objectStore('snapshots').get('run');
      get.onsuccess = () => resolve([...new Uint8Array(get.result)]); get.onerror = () => reject(get.error);
      tx.oncomplete = () => db.close();
    };
  })));
}
try {
  await page.goto(process.env.GAME_URL || 'http://127.0.0.1:8080/index.html', { waitUntil: 'networkidle' });
  await page.waitForFunction(() => typeof Module?._ashfall_get_loot_state === 'function', null, { timeout: 30000 });
  let l = await loot(); unique(l);
  assert.deepEqual(l.worn.map(i => i.id), [1, 2, 3]); assert.deepEqual(l.bag.map(i => i.id), [4, 5, 6]);
  const spare = itemData(l.bag[0]), uploads = (await state()).spriteUploads;
  await page.mouse.click(610, 40); await lootUntil(s => s.open, 'backpack header button');
  assert.equal(await page.locator('#sprite-editor-link').isVisible(), false, 'HTML navigation cannot cover the backpack');
  const frozen = await state(); await page.waitForTimeout(250); const still = await state();
  assert.equal(still.elapsed, frozen.elapsed); assert.equal(still.spriteIdleSeconds, frozen.spriteIdleSeconds);
  await pick(5); await button('equip');
  l = await loot(); assert.equal(l.worn.find(i => i.slot === 1).id, 5);
  assert.equal(l.bag.find(i => i.id === 2).cell, 1, 'old armor goes in the swapped cell');
  assert.equal(l.count, 3); assert.equal((await state()).armor, 5); unique(l);
  await pick(5); await button('equip'); // Worn item -> unequip.
  assert.equal((await state()).body, -1); assert.equal((await loot()).count, 4);
  await page.screenshot({ path: `${artifacts}/backpack-empty-slot.png` });
  await pick(2); await button('equip'); assert.equal((await state()).armor, 8);
  await pick(4); await button('drop');
  l = await loot(); assert.equal(l.count, 2); assert.ok(l.ground.some(i => i.dropId === 4)); unique(l);
  await bag(false); await page.keyboard.press('e');
  await lootUntil(s => s.bag.some(i => i.id === 4), 'dropped instance is recollected');
  assert.deepEqual(itemData((await loot()).bag.find(i => i.id === 4)), spare);
  assert.equal((await state()).groundSerial, 0, 'inventory clicks never issue ground movement');
  assert.equal((await state()).spriteUploads, uploads, 'inventory changes reuse resident artwork');
  console.log('[PASS] Backpack header/input capture, frozen simulation, atomic swaps, empty layer rendering and reversible ID-preserving drops');

  await bag(true); await pick(4); await button('salvage');
  l = await loot(); assert.equal(l.confirmSalvage, 4); assert.equal(l.embers, 0); assert.ok(l.bag.some(i => i.id === 4));
  await pick(5); assert.equal((await loot()).confirmSalvage, 0, 'selection change cancels salvage consent');
  await pick(4); await button('salvage'); await bag(false); await bag(true);
  assert.equal((await loot()).confirmSalvage, 0, 'closing the backpack cancels salvage consent');
  await pick(4); await button('salvage'); await button('salvage');
  l = await loot(); assert.equal(l.embers, spare.salvage); assert.ok(!l.bag.some(i => i.id === 4));
  await button('salvage'); assert.equal((await loot()).embers, spare.salvage, 'stale clicks cannot pay twice');
  await bag(false); await page.keyboard.press('1'); await drawn();
  assert.equal((await state()).head, 0, 'a shortcut cannot conjure salvaged or unowned headgear');
  assert.equal((await loot()).ground.length, 0);
  await restart();
  console.log('[PASS] Explicit salvage confirmation/cancellation, ember payout once and owned-only shortcuts');

  await pause();
  let writeSerial = (await state()).saveSerial;
  await page.keyboard.press('k'); await gameUntil(s => s.saveSerial === writeSerial + 1, 'pre-race checkpoint');
  const beforeRead = (await state()).loadSerial;
  const closed = await page.evaluate(() => { window.__ioHeld = true; return window.__ioClosed; });
  await page.keyboard.press('l'); await page.waitForFunction(() => window.__ioReleases.length === 1);
  await bag(true); await pick(4); await button('equip');
  assert.equal((await state()).saveStatus, 0, 'cancelled Load must not remain stuck at Loading');
  await page.evaluate(() => window.__releaseIO());
  await page.waitForFunction(before => window.__ioClosed > before, closed);
  assert.equal((await state()).loadSerial, beforeRead, 'stale Load cannot replace newer backpack intent');
  assert.ok((await loot()).worn.some(i => i.id === 4));
  writeSerial = (await state()).saveSerial;
  await page.evaluate(() => { window.__ioHeld = true; });
  await page.keyboard.press('k'); await page.waitForFunction(() => window.__ioReleases.length === 1);
  await bag(false); // This must cancel reads only, not suppress this Save's status.
  await page.evaluate(() => window.__releaseIO());
  await gameUntil(s => s.saveSerial === writeSerial + 1 && s.saveStatus === 1, 'Save acknowledgement survives backpack toggle');
  await page.evaluate(() => { window.__fileHeld = true; });
  const filePicker = page.waitForEvent('filechooser'); await drawn(); await page.mouse.click(850, 479);
  await (await filePicker).setFiles(resolve('tests/fixtures/legacy-run-v1.ashrun'));
  await page.waitForFunction(() => window.__fileReleases.length === 1);
  await bag(true); await pick(5); await button('equip');
  await page.evaluate(() => window.__releaseFileIO()); await drawn();
  assert.equal((await state()).loadSerial, beforeRead, 'stale portable import cannot replace newer gear');
  assert.ok((await loot()).worn.some(i => i.id === 5));
  assert.equal((await state()).saveStatus, 0);
  await restart();
  console.log('[PASS] Newer backpack intent cancels stale loads/imports without suppressing unrelated Save completion');

  if (process.env.LOOT_INVENTORY_ONLY !== '1') {
    const initial = await state(), enemy = initial.targets.find(e => e.id === 6);
    await page.mouse.click(enemy.screenX, enemy.screenY - 26 * initial.zoom);
    await gameUntil(s => s.selectedId === 6, 'real monster body selection');
    const deadline = Date.now() + 50000;
    while ((await state()).kills === 0 && Date.now() < deadline) {
      const s = await state(); assert.ok(s.health > 0, 'survive the actual encounter');
      if (s.health < 65 && s.potions) await page.keyboard.press('q');
      await page.waitForTimeout(80);
    }
    await gameUntil(s => s.kills === 1, 'real combat creates drops');
    await gameUntil(s => !s.targets.some(e => e.id === 6), 'corpse removal does not remove drops');
    l = await loot(); assert.equal(l.embers, 0); assert.equal(l.experience, 20);
    const gear = l.ground.find(i => i.kind === 0), coin = l.ground.find(i => i.kind === 1);
    assert.ok(gear && coin); assert.equal(coin.amount, 15);
    await page.mouse.move(gear.screenX, gear.screenY - 7 * (await state()).zoom);
    await page.screenshot({ path: `${artifacts}/equipment-drops.png` });
    await page.mouse.click(gear.screenX, gear.screenY - 7 * (await state()).zoom);
    await lootUntil(s => s.bag.some(i => i.id === gear.id), 'click-to-walk gear pickup');
    const picked = (await loot()).bag.find(i => i.id === gear.id);
    for (const field of ['slot', 'variant', 'rarity', 'armorRoll', 'speedRoll', 'hasteRoll']) assert.equal(picked[field], gear[field]);
    const cache = (await loot()).ground.find(i => i.dropId === coin.dropId);
    await page.mouse.click(cache.screenX, cache.screenY - 7 * (await state()).zoom);
    await lootUntil(s => s.embers === 15 && !s.ground.some(i => i.dropId === coin.dropId), 'explicit currency pickup');
    await pause();
    const savedOwnership = snapshot(await loot()), saveSerial = (await state()).saveSerial;
    await page.keyboard.press('k'); await gameUntil(s => s.saveSerial === saveSerial + 1, 'v2 local save transaction');
    const saved = await record(); assert.equal(saved.readUInt32LE(20), 2);
    await bag(true); await pick(gear.id); await button('equip'); await button('sort');
    const loadSerial = (await state()).loadSerial;
    await page.keyboard.press('l'); await gameUntil(s => s.loadSerial === loadSerial + 1, 'v2 local load');
    assert.deepEqual(snapshot(await loot()), savedOwnership, 'bag cells, worn rolls, drops, target and economy round-trip exactly');
    const download = page.waitForEvent('download'); await drawn(); await page.mouse.click(600, 479);
    const file = await download; assert.equal(file.suggestedFilename(), 'run.ashrun');
    assert.deepEqual(await readFile(await file.path()), saved);
    await page.reload(); await page.waitForFunction(() => typeof Module?._ashfall_get_loot_state === 'function');
    assert.equal((await loot()).ground.length, 0, 'new launch is not an implicit load');
    await page.keyboard.press('l'); await gameUntil(s => s.loadSerial === 1, 'saved ownership survives reload');
    assert.deepEqual(snapshot(await loot()), savedOwnership);
    console.log('[PASS] Real combat gear/ember drops, corpse lifetime, click-to-walk pickup and exact v2 save/load/export/reload');
  }

  await importRun(resolve('tests/fixtures/full-backpack-v2.ashrun'));
  l = await loot(); assert.equal(l.count, 24); assert.ok(l.ground.some(i => i.kind === 0)); unique(l);
  await bag(true);
  const relic = (await loot()).bag.find(i => i.rarity === 2); assert.ok(relic);
  const old = (await loot()).worn.find(i => i.slot === relic.slot), cellIndex = relic.cell;
  await pick(relic.id); await button('equip');
  l = await loot(); assert.equal(l.count, 24); assert.equal(l.worn.find(i => i.slot === relic.slot).id, relic.id);
  assert.equal(l.bag.find(i => i.id === old.id).cell, cellIndex); unique(l);
  assert.equal((await state()).armor, l.worn.reduce((sum, i) => sum + i.armor, 0), 'rolled bonuses, not only catalog stats, feed combat');
  await button('equip'); // No capacity to unequip, so the whole operation must fail intact.
  assert.equal((await loot()).worn.find(i => i.slot === relic.slot).id, relic.id);
  await button('drop'); assert.ok((await loot()).worn.some(i => i.id === relic.id), 'worn item cannot be dropped by a stale/disabled button');
  await page.screenshot({ path: `${artifacts}/backpack-full-relic.png` });
  await bag(false); await page.keyboard.press('p'); await gameUntil(s => !s.paused, 'resume full-bag run');
  const groundCount = (await loot()).ground.length;
  await page.keyboard.press('e'); await drawn(); l = await loot();
  assert.equal(l.count, 24); assert.equal(l.ground.length, groundCount, 'full bag/flasks leave every unclaimed drop intact');
  await bag(true);
  const sacrificed = (await loot()).bag[0]; await pick(sacrificed.id); await button('drop');
  assert.equal((await loot()).count, 23);
  await bag(false); await page.keyboard.press('e'); await lootUntil(s => s.count === 24, 'free cell accepts one waiting gear drop');
  unique(await loot());
  await bag(true);
  const sortedId = (await loot()).bag.at(-1).id; await pick(sortedId); await button('sort');
  assert.equal((await loot()).selectedItem, sortedId, 'sort retains stable selection, not an old cell index');
  const consumed = (await loot()).bag.find(i => i.id === sortedId), embers = (await loot()).embers;
  await button('salvage'); assert.ok((await loot()).bag.some(i => i.id === sortedId));
  await button('salvage'); assert.equal((await loot()).embers, embers + consumed.salvage);
  await pick(relic.id); await button('equip'); // Now there is one free cell.
  l = await loot(); assert.equal(l.count, 24); assert.ok(!l.worn.some(i => i.slot === relic.slot)); unique(l);
  const emptySlot = snapshot(l), saveSerial = (await state()).saveSerial;
  await page.keyboard.press('k'); await gameUntil(s => s.saveSerial === saveSerial + 1, 'save a full backpack plus empty equipment slot');
  const loadSerial = (await state()).loadSerial; await page.keyboard.press('l');
  await gameUntil(s => s.loadSerial === loadSerial + 1, 'reload empty equipment slot');
  assert.deepEqual(snapshot(await loot()), emptySlot);
  console.log('[PASS] Full-bag failure safety, rarity bonuses, capacity-neutral swap, sort selection, salvage, no duplication and empty-slot persistence');

  await importRun(resolve('tests/fixtures/legacy-run-v1.ashrun'));
  l = await loot(); assert.equal(l.count, 0); assert.equal(l.ground.length, 0);
  assert.equal(l.embers, 30); assert.equal(l.potions, 1); assert.equal(l.experience, 140);
  assert.deepEqual(l.worn.map(i => i.variant), [2, 2, 2]);
  assert.ok(l.worn.every(i => i.rarity === 0));
  await bag(true); await button('craft');
  assert.equal((await loot()).potions, 2); assert.equal((await loot()).embers, 0);
  await button('craft'); assert.equal((await loot()).potions, 2, 'insufficient currency is never spent partially');
  await page.setViewportSize({ width: 640, height: 480 });
  await page.waitForFunction(() => Module.canvas.width === 640 && Module.canvas.height === 480); await drawn();
  l = await loot();
  for (const name of ['equip', 'drop', 'salvage', 'sort', 'craft', 'close']) {
    const r = l.buttons[name], panel = l.buttons.panel;
    assert.ok(r.x >= panel.x && r.y >= panel.y && r.x + r.width <= panel.x + panel.width &&
      r.y + r.height <= panel.y + panel.height && r.x >= 0 && r.y >= 0 && r.x + r.width <= 640 && r.y + r.height <= 480,
    `compact ${name} stays inside the panel and canvas`);
  }
  await page.screenshot({ path: `${artifacts}/backpack-compact.png` });
  await button('close'); await lootUntil(s => !s.open, 'compact close button');
  assert.equal(await page.locator('#sprite-editor-link').isVisible(), true);
  await page.setViewportSize({ width: 1440, height: 900 });
  await page.waitForFunction(() => Module.canvas.width === 1440); await drawn();
  console.log('[PASS] Authentic v1 migration without free spares/duplicate rewards, flask crafting and compact input/layout');

  await importRun(resolve('tests/fixtures/victory-loot-v2.ashrun'));
  l = await loot(); assert.ok(l.cleared && !l.victoryAcknowledged); assert.equal(l.ground.length, 4);
  const victoryTime = (await state()).elapsed;
  await button('victory'); await lootUntil(s => s.victoryAcknowledged, 'collect spoils victory action');
  await page.keyboard.press('e'); await lootUntil(s => s.embers === 45, 'last Warden drops remain collectible after victory');
  l = await loot(); assert.equal(l.count, 5); assert.equal(l.ground.length, 1); assert.equal(l.ground[0].kind, 2);
  assert.ok(l.bag.some(i => i.rarity === 2), 'Warden guarantees an owned relic after pickup'); unique(l);
  await bag(true); await pick(l.bag.find(i => i.rarity === 2).id);
  await page.screenshot({ path: `${artifacts}/victory-spoils.png` });
  assert.equal((await state()).elapsed, victoryTime, 'looting does not inflate the clear time');
  assert.deepEqual(errors, []);
  console.log('[PASS] Victory dismissal, post-clear pickups/backpack, guaranteed Warden relic and frozen clear time');
  console.log(`All loot browser checks passed${process.env.LOOT_INVENTORY_ONLY === '1' ? ' (procedural inventory path)' : ''}.`);
} catch (error) {
  console.error(error);
  console.error({ game: await state().catch(() => null), loot: await loot().catch(() => null), errors });
  if (process.env.GITHUB_ACTIONS === 'true') {
    const details = `${error.stack || error}\nLoot: ${JSON.stringify(await loot().catch(() => null))}`
      .replaceAll('%', '%25').replaceAll('\r', '%0D').replaceAll('\n', '%0A');
    console.log(`::error title=Loot browser smoke::${details}`);
  }
  await page.screenshot({ path: `${artifacts}/loot-failure.png` }).catch(() => {});
  process.exitCode = 1;
} finally { await browser.close(); }
