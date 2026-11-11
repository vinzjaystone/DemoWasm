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
async function clickWorld(x, y) {
  const s = await state();
  await page.mouse.click(s.cameraX + ((x - y) * 32 - s.targetX) * s.zoom,
    s.cameraY + ((x + y) * 16 - s.targetY) * s.zoom);
}
try {
  await page.goto(process.env.GAME_URL || 'http://127.0.0.1:8080', { waitUntil: 'networkidle' });
  await page.waitForFunction(() => typeof window.Module?._ashfall_get_state === 'function', null, { timeout: 30000 });
  assert.equal((await state()).health, 120);
  assert.equal((await state()).enemies, 8);
  assert.equal((await state()).armor, 8);
  assert.equal(await page.locator('#loading').evaluate(el => el.classList.contains('done')), true);
  console.log('[PASS] WebGL / WASM startup, eight enemies, initial stats');

  await page.mouse.click(1280, 366); // Head slot, not the ground underneath it.
  await until(s => s.head === 1 && s.armor === 11, 'head slot click');
  assert.equal((await state()).moving, false);
  await page.mouse.click(1280, 442);
  await until(s => s.body === 2 && s.armor === 8, 'armor slot click');
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
  await clickWorld(9.5, 12.5);
  await until(s => Math.hypot(s.x - 9.5, s.y - 12.5) < 0.06 && !s.moving, 'click-to-move arrival');
  console.log('[PASS] Obstacle rejection, isometric picking, smooth path arrival');

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
  await page.keyboard.press('KeyR');
  await until(s => s.health === 120 && s.head === 0 && s.kills === 0, 'restart');
  await page.keyboard.press('KeyQ');
  assert.equal((await state()).potions, 3); // Full-health flasks must not be consumed.
  console.log('[PASS] Attack, dodge, pause clock, guide, restart, full-health flask');

  // Play through a kill using ordinary inputs. Diagnostics are read-only.
  await clickWorld(6.5, 16.5);
  await page.keyboard.down('Space');
  const deadline = Date.now() + 24000;
  let combatScreenshot = false;
  while (Date.now() < deadline) {
    const s = await state();
    if (s.kills >= 1) break;
    assert.ok(s.health > 0, 'player should survive the first encounter');
    const targets = s.targets.filter(e => e.health > 0)
      .sort((a, b) => Math.hypot(a.x - s.x, a.y - s.y) - Math.hypot(b.x - s.x, b.y - s.y));
    const target = targets[0];
    await page.mouse.move(target.screenX, target.screenY);
    const distance = Math.hypot(target.x - s.x, target.y - s.y);
    if (distance > 1.5 && !s.moving) await clickWorld(target.x, target.y);
    if (s.health < 65 && s.potions > 0) await page.keyboard.press('KeyQ');
    if (!combatScreenshot && target.health < 65 && s.attack) {
      await page.screenshot({ path: `${artifacts}/combat.png` });
      combatScreenshot = true;
    }
    await page.waitForTimeout(90);
  }
  await page.keyboard.up('Space');
  const afterCombat = await until(s => s.kills >= 1, 'enemy death', 1000);
  assert.ok(afterCombat.enemies <= 7);
  console.log('[PASS] Pursuit, melee damage, combo attacks, enemy death and quest progress');

  await page.keyboard.press('KeyR');
  await until(s => s.kills === 0 && s.health === 120, 'final reset');
  await page.mouse.move(1085, 180);
  await page.screenshot({ path: `${artifacts}/desktop.png` });
  await page.setViewportSize({ width: 960, height: 720 });
  await page.waitForFunction(() => Module.canvas.width === 960 && Module.canvas.height === 720);
  await page.screenshot({ path: `${artifacts}/compact.png` });
  await page.setViewportSize({ width: 640, height: 560 });
  await page.keyboard.press('KeyI'); // Hide the overlay inventory on narrow screens.
  await until(s => !s.inventory, 'compact inventory');
  await page.waitForFunction(() => Module.canvas.width === 640 && Module.canvas.height === 560);
  await clickWorld(9.5, 12.5);
  await until(s => Math.hypot(s.x - 9.5, s.y - 12.5) < 0.06, 'resized picking');
  await page.screenshot({ path: `${artifacts}/small.png` });
  console.log('[PASS] Canvas resizing, compact UI, picking after resize');
  assert.deepEqual(errors, [], 'browser errors');
  console.log(`All browser checks passed. Screenshots: ${artifacts}`);
} catch (error) {
  console.error(error);
  await page.screenshot({ path: `${artifacts}/failure.png` }).catch(() => {});
  process.exitCode = 1;
} finally {
  await browser.close();
}
