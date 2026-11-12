// Optional integration test. Build with ASHFALL_WEB_TEST_API=ON, serve build/web,
// then run: npm install --no-save --package-lock=false playwright &&
//          npx playwright install chromium && node tests/browser_smoke.mjs
import assert from 'node:assert/strict';
import { mkdir } from 'node:fs/promises';
import { resolve } from 'node:path';
const { chromium } = await import(process.env.PLAYWRIGHT_MODULE || 'playwright');
const artifacts = resolve(process.env.ARTIFACTS || 'build/browser');
await mkdir(artifacts, { recursive: true });
const browser = await chromium.launch({
  headless: true,
  executablePath: process.env.CHROMIUM_EXECUTABLE || undefined,
  args: ['--enable-unsafe-swiftshader', '--use-angle=swiftshader',
    ...JSON.parse(process.env.CHROMIUM_ARGS || '[]')],
});
const page = await browser.newPage({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 1 });
const errors = [];
page.on('pageerror', error => errors.push(error.message));
page.on('console', msg => { if (msg.type() === 'error') errors.push(msg.text()); });
const state = () => page.evaluate(() => JSON.parse(Module.UTF8ToString(Module._ashfall_get_state())));
async function until(predicate, label, timeout = 8000) {
  const deadline = Date.now() + timeout;
  let latest;
  do {
    latest = await state();
    if (predicate(latest)) return latest;
    await page.waitForTimeout(65);
  } while (Date.now() < deadline);
  throw new Error(`${label}: ${JSON.stringify(latest)}`);
}
async function restart(label) {
  const serial = (await state()).restartSerial;
  await page.keyboard.press('KeyR');
  // Default health/selection values can already be true before the queued key
  // is processed. Wait for an acknowledgement before reading sprite positions.
  const fresh = await until(s => s.restartSerial > serial, label);
  assert.equal(fresh.restartSerial, serial + 1);
  assert.equal(fresh.health, 120);
  assert.equal(fresh.kills, 0);
  assert.equal(fresh.selectedId, 0);
  assert.equal(fresh.attackSerial, 0);
  assert.equal(fresh.moving, false);
  assert.equal(fresh.x, 8.5);
  assert.equal(fresh.y, 12.5);
  return fresh;
}
async function clickWorld(x, y) {
  const s = await state();
  await page.mouse.click(s.cameraX + ((x - y) * 32 - s.targetX) * s.zoom,
    s.cameraY + ((x + y) * 16 - s.targetY) * s.zoom);
}
try {
  await page.goto(process.env.GAME_URL || 'http://127.0.0.1:8080/index.html', { waitUntil: 'networkidle' });
  await page.waitForFunction(() => typeof window.Module?._ashfall_get_state === 'function', null, { timeout: 30000 });
  assert.equal((await state()).health, 120);
  assert.equal((await state()).enemies, 8);
  assert.equal((await state()).armor, 8);
  assert.equal(await page.locator('#loading').evaluate(el => el.classList.contains('done')), true);
  // Even identical initial states must acknowledge each processed restart.
  await restart('initial restart acknowledgement');
  await restart('repeat restart acknowledgement');
  console.log('[PASS] WebGL / WASM startup, initial stats, repeated restart acknowledgements');

  await page.mouse.click(1280, 366); // Head slot, not the ground underneath it.
  await until(s => s.head === 1 && s.armor === 11, 'head slot click');
  assert.ok((await state()).attackInterval > 2.5, 'heavy headgear slows attacks');
  assert.equal((await state()).moving, false);
  await page.mouse.click(1280, 442);
  await until(s => s.body === 2 && s.armor === 8, 'armor slot click');
  assert.ok((await state()).attackInterval < 2.5, 'light armor speeds attacks');
  await page.keyboard.press('Digit3');
  await until(s => s.boots === 1 && s.armor === 10, 'boots shortcut');
  console.log('[PASS] Paper-doll slot clicks, shortcuts, live stats, no click-through');

  await page.keyboard.press('KeyI');
  await until(s => !s.inventory, 'inventory toggle');
  await page.keyboard.press('KeyI');
  await until(s => s.inventory, 'inventory restore');
  await page.keyboard.press('KeyG');
  await until(s => s.grid, 'grid overlay');
  await page.keyboard.press('KeyG');
  await clickWorld(5.5, 6.5); // Blocked column.
  assert.equal((await state()).moving, false);
  let previous = await state();
  for (let click = 0; click < 6; ++click) {
    await clickWorld(click % 2 === 0 ? 15.5 : 16.5, 12.5);
    const next = await until(s => s.elapsed > previous.elapsed + 0.025, 'reroute advances simulation');
    assert.ok(next.x > previous.x + 0.01, 'repeated reroutes must keep moving forward');
    assert.ok(Math.abs(next.y - 12.5) < 0.03, 'rerouting must not displace the actor sideways');
    previous = next;
  }
  await clickWorld(9.5, 12.5); // Reverse direction and arrive using only the newest command.
  await until(s => Math.hypot(s.x - 9.5, s.y - 12.5) < 0.06 && !s.moving, 'replacement route arrival');
  console.log('[PASS] Obstacle rejection, isometric picking, repeated mid-tile route replacement');

  await page.keyboard.down('Space');
  await until(s => s.attack, 'attack input');
  await page.keyboard.up('Space');
  await page.keyboard.press('ShiftLeft');
  await until(s => s.dashCooldown > 1.0, 'dash input');
  await page.keyboard.press('Escape');
  await until(s => s.paused, 'pause input');
  const pausedTime = (await state()).elapsed;
  await page.waitForTimeout(250);
  assert.equal((await state()).elapsed, pausedTime);
  await page.screenshot({ path: `${artifacts}/paused.png` });
  await page.keyboard.press('Escape');
  await until(s => !s.paused, 'resume input');
  await page.keyboard.press('KeyH');
  await until(s => s.help, 'field guide');
  await page.mouse.click(720, 620); // Guide's primary button at 1440 x 900.
  await until(s => !s.help, 'field guide button');
  await restart('restart');
  assert.equal((await state()).head, 0);
  await page.keyboard.press('KeyQ');
  assert.equal((await state()).potions, 3); // Full-health flasks must not be consumed.
  console.log('[PASS] Attack, dodge, pause clock, guide, restart, full-health flask');

  // Sprite-body picking, HUD input capture, and cancellation use real clicks.
  let snapshot = await state();
  let target = snapshot.targets.find(e => e.id === 6);
  await page.mouse.click(target.screenX, target.screenY - 26 * snapshot.zoom);
  let selected = await until(s => s.selectedId === target.id, 'monster body selection');
  assert.equal(selected.targetAction, 1); // Chasing, not swinging out of range.
  assert.equal(selected.attackSerial, 0);
  assert.ok(selected.targetHud.width > 0);
  assert.ok(Math.abs(selected.targetHud.x + selected.targetHud.width / 2 - 570) < 1);
  assert.ok(selected.targetHud.y < 120, 'target HP must be at the upper center');
  await page.mouse.click(selected.targetHud.x + 140, selected.targetHud.y + 38);
  await page.waitForTimeout(80);
  assert.equal((await state()).selectedId, target.id, 'target HUD must not click through to the floor');
  await clickWorld(8.5, 11.5);
  await until(s => s.selectedId === 0 && s.targetHud.width === 0, 'floor click cancels auto-combat');
  await restart('reset target state');
  console.log('[PASS] Body picking, upper-center target HUD, input capture, floor cancellation');

  // One click must chase, stop in melee range, and finish the kill. No held key,
  // repeated command, or diagnostic mutation may assist this encounter.
  snapshot = await state();
  target = snapshot.targets.find(e => e.id === 6);
  await page.mouse.click(target.screenX, target.screenY - 26 * snapshot.zoom);
  await until(s => s.selectedId === target.id && s.moving, 'single-click pursuit');
  await page.mouse.move(35, 90); // Facing and aim must remain locked to the selected monster.
  const first = await until(s => s.attackSerial === 1, 'first automatic strike');
  const liveTarget = first.targets.find(e => e.id === target.id);
  assert.ok(Math.hypot(first.x - liveTarget.x, first.y - liveTarget.y) < 1.45);
  assert.equal(first.moving, false, 'stop to attack in melee range');
  await until(s => s.selectedHealth < s.selectedMaxHealth, 'selected HP changes after damage');
  await page.screenshot({ path: `${artifacts}/target-combat.png` });
  const second = await until(s => s.attackSerial === 2, 'second automatic strike');
  assert.ok(second.elapsed - first.elapsed >= 2.3 && second.elapsed - first.elapsed <= 2.85,
    `base ASPD should produce ~2.5s strikes, got ${second.elapsed - first.elapsed}`);
  await page.keyboard.press('Escape');
  const combatPaused = await until(s => s.paused, 'pause auto-combat');
  await page.waitForTimeout(250);
  const combatStill = await state();
  assert.equal(combatStill.selectedId, target.id);
  assert.equal(combatStill.attackCooldown, combatPaused.attackCooldown);
  assert.equal(combatStill.attackSerial, combatPaused.attackSerial);
  await page.keyboard.press('Escape');
  await until(s => !s.paused, 'resume auto-combat');
  const afterCombat = await until(s => s.kills >= 1, 'automatic enemy death', 12000);
  assert.ok(afterCombat.health > 0);
  assert.ok(afterCombat.enemies <= 7);
  assert.equal(afterCombat.selectedId, 0);
  assert.equal(afterCombat.targetHud.width, 0);
  await until(s => !s.targets.some(e => e.id === target.id), 'dead target removal');
  console.log('[PASS] Single-click chase, auto-attack cadence, live HP, pause, death/removal cleanup');

  await restart('final reset');
  await page.mouse.move(1085, 180);
  await page.screenshot({ path: `${artifacts}/desktop.png` });
  await page.setViewportSize({ width: 960, height: 720 });
  await page.waitForFunction(() => Module.canvas.width === 960 && Module.canvas.height === 720);
  snapshot = await state();
  target = snapshot.targets.find(e => e.id === 6);
  await page.mouse.click(target.screenX, target.screenY - 26 * snapshot.zoom);
  selected = await until(s => s.selectedId === target.id, 'compact target picking');
  assert.ok(Math.abs(selected.targetHud.x + selected.targetHud.width / 2 - 347) < 1);
  await page.screenshot({ path: `${artifacts}/compact.png` });
  await page.setViewportSize({ width: 640, height: 560 });
  await page.keyboard.press('KeyI'); // Hide the overlay inventory on narrow screens.
  await until(s => !s.inventory, 'compact inventory');
  await page.waitForFunction(() => Module.canvas.width === 640 && Module.canvas.height === 560);
  await restart('small view reset');
  await clickWorld(9.5, 12.5);
  await until(s => Math.hypot(s.x - 9.5, s.y - 12.5) < 0.06, 'resized picking');
  snapshot = await state();
  target = snapshot.targets.find(e => e.id === 6);
  await page.mouse.click(target.screenX, target.screenY); // Feet picking works too.
  selected = await until(s => s.selectedId === target.id, 'small target feet picking');
  assert.ok(Math.abs(selected.targetHud.x + selected.targetHud.width / 2 - 320) < 1);
  assert.ok(selected.targetHud.y < 100 && selected.targetHud.x >= 0);
  await page.screenshot({ path: `${artifacts}/small.png` });
  console.log('[PASS] Canvas resizing, compact UI, picking after resize');
  assert.deepEqual(errors, [], 'browser errors');
  console.log(`All browser checks passed. Screenshots: ${artifacts}`);
} catch (error) {
  console.error(error);
  if (process.env.GITHUB_ACTIONS === 'true') {
    // Keep the failing assertion and state accessible in check annotations even
    // when the runner's downloadable log/archive service is unavailable.
    const snapshot = await state().catch(() => null);
    const details = `${error.stack || error}\nState: ${JSON.stringify(snapshot)}`
      .replaceAll('%', '%25').replaceAll('\r', '%0D').replaceAll('\n', '%0A');
    console.log(`::error title=Browser smoke test::${details}`);
  }
  await page.screenshot({ path: `${artifacts}/failure.png` }).catch(() => {});
  process.exitCode = 1;
} finally {
  await browser.close();
}
