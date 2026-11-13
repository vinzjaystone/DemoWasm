// Actual C++/WASM player animation, real input, and observed WebGL uploads.
// Build with ASHFALL_WEB_TEST_API=ON. SPRITES_EXPECTED=0 also exercises the
// supported ASHFALL_PLAYER_SPRITES=OFF procedural fallback build.
import assert from 'node:assert/strict';
import { mkdir } from 'node:fs/promises';
import { resolve } from 'node:path';
const { chromium } = await import(process.env.PLAYWRIGHT_MODULE || 'playwright');
const shaderFailure = process.env.SPRITE_SHADER_FAILURE === '1';
const expected = process.env.SPRITES_EXPECTED !== '0' && !shaderFailure;
const attemptedUploads = expected || shaderFailure ? 5 : 0;
const artifacts = resolve(process.env.ARTIFACTS || 'build/browser');
await mkdir(artifacts, { recursive: true });
const browser = await chromium.launch({ headless: true,
  executablePath: process.env.CHROMIUM_EXECUTABLE || undefined,
  args: ['--enable-unsafe-swiftshader', '--use-angle=swiftshader', ...JSON.parse(process.env.CHROMIUM_ARGS || '[]')],
});
const page = await browser.newPage({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 1 });
const errors = [], requests = [];
page.on('pageerror', e => errors.push(e.message));
page.on('console', m => { if (m.type() === 'error') errors.push(m.text()); });
page.on('request', r => requests.push(r.url()));
// Observe the real driver calls. Optional shader fault injection exercises
// cleanup of a real initialization failure without a game mutation API.
await page.addInitScript(({ shaderFailure }) => {
  window.__characterTextures = [];
  const proto = WebGLRenderingContext.prototype;
  const image = proto.texImage2D, parameter = proto.texParameteri;
  const destroy = proto.deleteTexture, shaderSource = proto.shaderSource;
  proto.deleteTexture = function (texture) {
    const record = window.__characterTextures.find(t => t.texture === texture);
    if (record) record.deleted = true;
    return destroy.call(this, texture);
  };
  proto.shaderSource = function (shader, source) {
    return shaderSource.call(this, shader, shaderFailure && source.includes('hitFlash') ? 'intentional invalid shader for cleanup test' : source);
  };
  proto.texImage2D = function (...args) {
    const result = image.apply(this, args);
    if (args.length === 9 && args[3] >= 512 && args[4] >= 512) {
      window.__characterTextures.push({ texture: this.getParameter(this.TEXTURE_BINDING_2D),
        width: args[3], height: args[4], parameters: {}, deleted: false });
    }
    return result;
  };
  proto.texParameteri = function (...args) {
    const result = parameter.apply(this, args);
    const texture = this.getParameter(this.TEXTURE_BINDING_2D);
    const record = window.__characterTextures.find(t => t.texture === texture);
    if (record) record.parameters[args[1]] = args[2];
    return result;
  };
}, { shaderFailure });
const state = () => page.evaluate(() => JSON.parse(Module.UTF8ToString(Module._ashfall_get_state())));
async function until(predicate, label, timeout = 8000) {
  const deadline = Date.now() + timeout;
  let s;
  do {
    s = await state();
    if (predicate(s)) return s;
    await page.waitForTimeout(20);
  } while (Date.now() < deadline);
  throw new Error(`${label}: ${JSON.stringify(s)}`);
}
async function restart() {
  const serial = (await state()).restartSerial;
  await page.keyboard.press('KeyR');
  return until(s => s.restartSerial > serial, 'restart acknowledgement');
}
async function clickWorld(x, y) {
  const s = await state();
  await page.mouse.click(s.cameraX + ((x - y) * 32 - s.targetX) * s.zoom,
    s.cameraY + ((x + y) * 16 - s.targetY) * s.zoom);
  return until(t => t.groundSerial > s.groundSerial, 'ground acknowledgement');
}
const uploads = () => page.evaluate(() => window.__characterTextures.map(({ width, height, parameters, deleted }) => ({ width, height, parameters, deleted })));
function checkAttackFrame(s) {
  const e = s.attackElapsed, recovery = s.attackDuration - 0.24;
  const boundaries = [0.075, 0.13, 0.185, 0.24, 0.24 + recovery / 2];
  // JSON rounds elapsed to 4 decimals; don't misclassify an exact marker.
  if (boundaries.some(t => Math.abs(e - t) < 0.0002)) return;
  const frame = e < 0.075 ? 0 : e <= 0.24 ? 1 + Math.min(2, Math.floor((e - 0.075) / 0.055)) : e < 0.24 + recovery / 2 ? 4 : 5;
  assert.equal(s.spriteFrame, frame, 'sprite samples the simulation swing, not the 2.5s cooldown');
}
try {
  await page.goto(process.env.GAME_URL || 'http://127.0.0.1:8080/index.html', { waitUntil: 'networkidle' });
  await page.waitForFunction(() => typeof window.Module?._ashfall_get_state === 'function');
  const initial = await state();
  assert.equal(initial.spriteReady, expected);
  assert.equal(initial.spriteUploads, attemptedUploads);
  const gpu = await uploads();
  assert.equal(gpu.length, attemptedUploads, 'actual atlas uploads');
  for (const sheet of gpu) {
    assert.equal(sheet.deleted, shaderFailure, 'failed initialization releases every partial texture');
    assert.ok(sheet.width <= 2048 && sheet.height <= 2048, 'WebGL 1 minimum texture size support');
    assert.equal(sheet.parameters[0x2800], 0x2600, 'nearest magnification');
    assert.equal(sheet.parameters[0x2801], 0x2600, 'nearest minification');
    assert.equal(sheet.parameters[0x2802], 0x812f, 'clamp S');
    assert.equal(sheet.parameters[0x2803], 0x812f, 'clamp T');
  }
  assert.equal(requests.some(url => /\.(png|jpg|jpeg|webp|data)(\?|$)/i.test(url)), false, 'no runtime art or filesystem dependency');
  console.log(`[PASS] ${expected ? 'Five generated RGBA atlases, nearest/clamp filtering' : shaderFailure ? 'Failed shader initialization frees all five textures and uses procedural fallback' : 'Procedural fallback, no atlas uploads'}`);

  await restart();
  const directions = [['S',0,1],['SW',-1,1],['W',-1,0],['NW',-1,-1],['N',0,-1],['NE',1,-1],['E',1,0],['SE',1,1]];
  for (const [name, dx, dy] of directions) {
    const s = await state();
    await page.mouse.move(s.screenX + dx * 90, s.screenY + dy * 90);
    const facing = await until(t => t.spriteDirection === name, `projected ${name} facing`);
    assert.equal(facing.x, 8.5); assert.equal(facing.y, 12.5);
    assert.equal(facing.spriteClip, 'idle');
    assert.equal(facing.spriteWeaponBehind, ['NW','N','NE'].includes(name));
  }
  const idle = (await state()).spriteFrame;
  await until(s => s.spriteFrame !== idle, 'idle breathing loop');
  console.log('[PASS] Eight projected body directions, front/back weapon order, independent idle loop');

  // Shared registration under both live and paper-doll equipment changes.
  for (const [key, field] of [['Digit1','head'], ['Digit2','body'], ['Digit3','boots']]) {
    const variants = new Set([(await state())[field]]);
    for (let i = 0; i < 3; ++i) {
      const before = await state();
      await page.keyboard.press(key);
      const changed = await until(s => s[field] !== before[field], `${field} layer change`);
      variants.add(changed[field]);
      assert.equal(changed.x, 8.5); assert.equal(changed.y, 12.5);
      assert.equal(changed.spriteClip, 'idle');
    }
    assert.equal(variants.size, 3);
  }
  assert.deepEqual(await uploads(), gpu, 'gear cycles use resident sheets');
  await page.screenshot({ path: `${artifacts}/${expected ? 'character-sprites' : 'character-fallback'}.png` });
  console.log('[PASS] All head/armor/boots variants remain live; no sprite reallocation');

  await restart();
  await clickWorld(8.5, 10.5); // Clear corridor: no monster interception.
  const walkFrames = new Set();
  const moving = await until(s => s.walking && s.spriteClip === 'walk', 'walk transition');
  assert.equal(moving.selectedId, 0);
  let latest = moving;
  const walkDeadline = Date.now() + 6000;
  while (latest.moving && Date.now() < walkDeadline) {
    if (latest.walking) {
      walkFrames.add(latest.spriteFrame);
      const phase = (latest.spriteWalkDistance % 2) * 4;
      if (Math.abs(phase - Math.round(phase)) > 0.0001) assert.equal(latest.spriteFrame, Math.floor(phase));
    }
    await page.waitForTimeout(20); latest = await state();
  }
  assert.equal(latest.moving, false, 'route completes within the deadline');
  assert.ok(walkFrames.size >= 3, 'walk frames advance with continuous displacement');
  assert.ok(Math.hypot(latest.x - 8.5, latest.y - 10.5) < 0.01);
  assert.equal(latest.spriteClip, 'idle');
  assert.ok(latest.spriteWalkDistance > 1.8);
  console.log('[PASS] Continuous navigation, distance-driven walk cadence, return to idle');

  await restart();
  for (let swing = 0; swing < 3; ++swing) {
    // A throttled software GPU can take much longer than 2.5 wall seconds to
    // advance 2.5 simulation seconds with the game's capped accumulator.
    await until(s => s.attackCooldown === 0, 'shared strike cooldown', 20000);
    const before = await state();
    await page.mouse.move(before.screenX, before.screenY - 90);
    await until(s => s.spriteDirection === 'N', 'attack aim');
    await page.mouse.click(before.screenX, before.screenY - 90, { button: 'right' });
    let attack = await until(s => s.attackSerial > before.attackSerial && s.attack, 'attack animation');
    assert.equal(attack.spriteClip, 'attack');
    assert.equal(attack.spriteDirection, 'N');
    assert.equal(attack.attackDuration, swing === 2 ? 0.43 : 0.36);
    const frames = new Set(), attackDeadline = Date.now() + 2500;
    // Changing the cursor cannot swivel a locked swing toward the new aim.
    await page.mouse.move(attack.screenX, attack.screenY + 90);
    do {
      if (attack.attack) {
        assert.equal(attack.spriteClip, 'attack'); assert.equal(attack.spriteDirection, 'N');
        checkAttackFrame(attack); frames.add(attack.spriteFrame);
      }
      await page.waitForTimeout(16); attack = await state();
    } while (attack.attack && Date.now() < attackDeadline);
    assert.equal(attack.attack, false, 'swing completes within the deadline');
    assert.ok(frames.size >= 2);
    assert.equal(attack.spriteClip, 'idle');
    assert.ok(attack.attackCooldown > 1.5, 'swing ends well before the ASPD cooldown');
  }
  console.log('[PASS] Six-frame attacks, locked facing, damage markers, third-combo timing, independent ASPD recovery');

  for (const [key, flag] of [['KeyP','paused'], ['KeyH','help']]) {
    await page.keyboard.press(key);
    const frozen = await until(s => s[flag], `${flag} acknowledgement`);
    await page.mouse.move(frozen.screenX + 90, frozen.screenY);
    await page.waitForTimeout(400);
    const after = await state();
    for (const field of ['spriteClip','spriteFrame','spriteDirection','spriteWalkDistance','spriteIdleSeconds','previewFrame','attackCooldown'])
      assert.equal(after[field], frozen[field], `${flag} freezes ${field}`);
    await page.keyboard.press(key); await until(s => !s[flag], `close ${flag}`);
  }
  await restart();
  const reset = await state();
  assert.equal(reset.spriteWalkDistance, 0);
  assert.ok(reset.spriteIdleSeconds < 0.25);
  assert.equal(reset.spriteClip, 'idle');
  assert.equal(reset.spriteUploads, attemptedUploads);
  assert.deepEqual(await uploads(), gpu, 'restart retains GPU sheets');
  assert.deepEqual(errors, [], 'no WebGL/shader/browser errors');
  console.log('[PASS] Pause/help freeze live and preview poses; restart resets animation without reuploading');
} catch (error) {
  console.error(error);
  const snapshot = await state().catch(() => null);
  console.error('State:', snapshot);
  if (process.env.GITHUB_ACTIONS === 'true') {
    // Keep failures inspectable through the Checks API even if GitHub's
    // downloadable logs/artifact storage is temporarily inaccessible.
    const details = `${error.stack || error}\nSprites expected: ${expected}; shader failure: ${shaderFailure}\nState: ${JSON.stringify(snapshot)}`
      .replaceAll('%', '%25').replaceAll('\r', '%0D').replaceAll('\n', '%0A');
    console.log(`::error title=Character browser smoke test::${details}`);
  }
  await page.screenshot({ path: `${artifacts}/character-failure.png` }).catch(() => {});
  process.exitCode = 1;
} finally {
  await browser.close();
}
