# Ashfall — The Ashen Sanctum

An asset-free isometric hack-and-slash MVP in **C++17 + Raylib 5.5**, built for **desktop and WebAssembly**. Enter a ruined sanctuary, cut down seven hollow and their Ash Warden, and keep your flame alive.

All art is procedural: stone tiles, pillars, rubble, braziers, the portal, characters, equipment, attack trails, particles, and interface. The only font is Raylib's embedded default font. **No images, spritesheets, font downloads, asset packs, JavaScript game engine, or runtime network requests are required.**

## Play

| Input | Action |
| --- | --- |
| **Left click** ground | Navigate with A*; a new click smoothly replaces the route and cancels auto-combat |
| **Left click** monster | Select, chase into melee range, then automatically attack until it dies |
| **Right click / Space** | Manual cleave toward the cursor; hold to repeat at the same attack-speed cadence |
| **Shift** | Dash toward the cursor, with brief invulnerability; 2.4 s cooldown |
| **Q** | Drink a flask: +50 vitality, three per run, never wasted at full health |
| **1 / 2 / 3** | Cycle **Head / Armor / Boots**; also click the equipment cards |
| **I** | Show/hide the character panel |
| **G** | Toggle the navigation grid |
| **Mouse wheel / − / +** | Zoom; the middle zoom button restores the fitted view |
| **Esc / P** | Pause/resume |
| **H / F1** | Open/close the field guide |
| **F** | Toggle fullscreen (browser fullscreen requires a user gesture) |
| **R** | Start a fresh run |

The hollow pursue within aggro range and telegraph their melee sector in red. Step or dash out of it, or interrupt the windup with a hit. Every third player strike is a stronger, wider cleave. Death offers a new run; defeating all eight enemies completes the sanctum.

### Click-to-target combat

Click a living monster's **body or feet** once—no held attack key is needed. The player follows a collision-safe path, stops in melee range, faces the target, and repeatedly strikes. A gold ground ring marks the selection; the **upper-center target panel** shows its name, current/max HP, health bar, chase/attack status, and strike interval. It stays centered in the visible play area as the window or inventory changes.

Default cadence is **one strike every 2.5 seconds**. Equipment changes attack speed (ASPD): `interval = 2.5 / (1 + sum(ASPD bonuses))`. The current catalog spans **2.0–2.78 seconds** per strike; the character panel displays the multiplier and interval. The short swing animation is separate from recovery, and every third consecutive strike still performs a stronger cleave. Repeated target clicks, switching targets, and changing equipment **do not reset the current cooldown**.

A valid ground command, a dash, or a manual attack cancels auto-combat. Target death, player death, and restarting clear the selection; pausing preserves it without advancing combat timers. Unreachable monsters show “NO PATH” rather than swinging through walls. Enemy recovery has also been paced for the slower player cadence while retaining its windup/telegraph.

### Live equipment

Each slot has three pieces with a distinct tint/silhouette and actual gameplay stats. Total armor reduces each incoming hit by `floor(armor × 0.65)`, with a minimum of one damage. Movement speed is `3.6 + sum(equipment speed bonuses)` tiles/second. Iron sallets/greaves each give −5% ASPD; cinder crowns and ranger leathers +5%; crimson mantles and fleetstriders +10%. The player starts with 120 vitality; kills grant embers and experience, and each 100 XP increases level and damage.

## Build requirements

- CMake **3.20+**, a C++17 compiler, and Make or Ninja.
- Raylib **5.5** is fetched automatically from a **SHA-256-verified release archive**. No manual Raylib installation is needed. The first configure needs internet access.
- For web: an activated **Emscripten SDK**; tested with **6.0.9**.
- For desktop: the usual Raylib graphics/window development libraries.

### Native desktop

On Debian/Ubuntu, install the build/window dependencies first:

```sh
sudo apt-get update
sudo apt-get install build-essential cmake libgl1-mesa-dev libx11-dev \
  libxrandr-dev libxi-dev libxcursor-dev libxinerama-dev
```

Then:

```sh
make native
./build/native/ashfall
```

Or use CMake directly (also works on macOS/Windows):

```sh
cmake -S . -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native --config Release --parallel
```

With a multi-config Windows generator, the executable is `build/native/Release/ashfall.exe`. On macOS install CMake and Xcode's command-line tools; Raylib uses the system frameworks. The desktop window has a 640 × 480 minimum size.

### WebAssembly

Install/activate the official [Emscripten SDK](https://github.com/emscripten-core/emsdk) once, then source its environment **in the shell running the build**:

```sh
# From your emsdk checkout:
./emsdk install 6.0.9
./emsdk activate 6.0.9
source ./emsdk_env.sh

# From this repository:
make web
make serve
# Open http://localhost:8080
```

Equivalent commands without Make:

```sh
emcmake cmake -S . -B build/web -DCMAKE_BUILD_TYPE=Release -DASHFALL_BUILD_TESTS=OFF
cmake --build build/web --parallel
python3 tools/serve.py --directory build/web --port 8080
```

Serve **all three files together**: `index.html`, `index.js`, and `index.wasm` from `build/web`. Do not open the HTML with `file://`. The included server binds to `0.0.0.0`, sends the correct WASM MIME type, and allows iframe previews and arbitrary preview hosts. Static hosting is sufficient; there is no backend. The browser needs WebGL and WebAssembly, not cross-origin isolation or SharedArrayBuffer.

A release build is approximately **350 KB total, uncompressed** (toolchain dependent). It starts with 32 MiB of WASM linear memory and allows growth. Threads, Asyncify, asset preloading, and Emscripten's filesystem are not used. Audio is intentionally not part of this MVP.

### Existing/offline Raylib

Use an existing source checkout without fetching:

```sh
cmake -S . -B build/native -DFETCHCONTENT_SOURCE_DIR_RAYLIB=/absolute/path/to/raylib-5.5
```

An installed Raylib package is also supported with `CMAKE_PREFIX_PATH`. Set `-DASHFALL_FETCH_RAYLIB=OFF` to require that package instead. Do not give the web build a native Raylib binary; use web-compiled Raylib or let CMake build it from source.

The renderer supports the Raylib **5.5 and 6.0** gradient APIs: 5.5 takes integer `x, y` arguments to `DrawCircleGradient`, while 6.0 takes a `Vector2` center. A compile-time adapter selects the signature declared by the selected header; no version override or downgrade is needed. The default fetched release remains 5.5. CI compiles the application against both releases' actual headers.

## Tests

### Display-independent simulation tests

No Raylib, SDK, graphics context, window libraries, or downloads are required:

```sh
make test
make sanitize # GCC/Clang: address + undefined-behavior sanitizers
```

Without Make:

```sh
cmake -S . -B build/tests -DASHFALL_BUILD_GAME=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build build/tests --parallel
ctest --test-dir build/tests --output-on-failure
```

Coverage includes both gradient API call shapes (without a graphics dependency), projection round trips/negative picking, collision and dash tunneling, A* invalid/unreachable/same-tile cases, optimal routing, no diagonal corner cutting, waypoint overshoot, continuous repeated rerouting with swept-corner clearance, equipment cycling/stats/attack speed, single-click target pursuit, cooldown preservation and stable-ID target cleanup, exact attack-sector intersection, cooldowns/invulnerability, one hit per swing, AI pursuit/telegraphs/dodging, death/removal, a complete eight-enemy playthrough, full-level connectivity, and a 100-second simulation stress run with collision and memory-budget invariants.

### Real browser smoke test (optional)

The gameplay build has **no npm dependency**. Playwright is only needed to run this development test:

```sh
emcmake cmake -S . -B build/web -DASHFALL_BUILD_TESTS=OFF -DASHFALL_WEB_TEST_API=ON
cmake --build build/web --parallel
npm install --no-save --package-lock=false playwright
npx playwright install chromium

# In one terminal:
make serve
# In another:
node tests/browser_smoke.mjs
```

This drives real mouse/keyboard inputs, checks gear/ASPD stats, blocked-tile picking, repeated mid-tile route replacement, body/feet target picking, a single-click chase and kill at the expected strike interval, live target HP, HUD click-through prevention, cancellation, dodging, pause, guide, restarting, death/removal cleanup, and target-panel layout after canvas resizing. Screenshots go into ignored `build/browser/`. `GAME_URL` overrides the default URL; `PLAYWRIGHT_MODULE`, `CHROMIUM_EXECUTABLE`, and `CHROMIUM_ARGS` support an existing browser installation.

`ASHFALL_WEB_TEST_API` is **off by default**. It adds only a read-only state snapshot for tests; no teleport, invulnerability, damage, or other test-only mutation is exposed. Turn it off for a normal release.

## Architecture

```text
src/
  types.h                  Small graphics-independent vector, tile, color, bounds types
  grid.h / .cpp            2:1 projection, inverse picking, level, swept/substepped collision
  pathfinding.h / .cpp     Reusable eight-connected A* workspace + waypoint following
  equipment.h / .cpp       Three slots × three pieces; armor/movement/ASPD/tint/silhouette
  player.h / .cpp          Navigation, ASPD/combo timers, dash, health, mitigation
  combat.h / .cpp          Enemy state + exact sector/AABB collision geometry
  world.h / .cpp           Target selection/chase/auto-combat, AI, damage/death, effects
  renderer.h / .cpp        Procedural art, stable Y-sort, HUD, paper doll, UI hit testing
  raylib_compat.h          Compile-time drawing API compatibility (Raylib 5.5/6.0)
  main.cpp                 Raylib input, fixed-step driver, native/web lifecycle
web/shell.html             Minimal loading/fullscreen canvas shell; no game logic
tests/core_tests.cpp     Native tests with no third-party test framework
tests/browser_smoke.mjs  Optional actual-browser integration checks
tools/serve.py           Preview-safe static server
```

### Coordinate and depth conventions

- Simulation positions are **tile units**; tile `(x,y)` has center `(x+0.5,y+0.5)`.
- `IsoProjection::worldToScreen` maps onto the untransformed 2:1 isometric plane:
  `screenX = originX + (x-y) × 32`, `screenY = originY + (x+y) × 16`.
- Input first uses Raylib `GetScreenToWorld2D` to undo the camera, then the inverse projection. Tile selection uses **floor**, including negative coordinates.
- Floor/light/telegraphs render first. Obstacles, portal, enemies, and the player share **one stable draw list sorted by projected foot Y**, rather than raw grid Y. Floating effects render afterwards.
- The paper doll draws **Base Body → Boots → Armor → Head**, followed by the weapon. The same function draws the live character preview.

### Navigation, combat, and lifetime

- A* uses an octile heuristic, costs `1` and `sqrt(2)`, no diagonal corner cuts, deterministic tie-breaking, and reused node storage. Paths exclude the start and include the destination's center.
- Reroutes splice the replacement path onto the actor’s **actual position**, not its previous tile center. A swept-circle segment test checks the new turn; re-centering is only a fallback when necessary to avoid a blocked corner. Following consumes a movement budget across multiple waypoints without overshooting. Movement, dash, separation, and knockback all use the **same circle-vs-grid collision routine**, subdividing long motion to prevent tunneling.
- Enemy state: **Idle → Chase → Attack (windup, hit, recovery) → Chase**, with terminal **Dead**. Pursuit uses throttled A* repaths, not movement through obstacles. Attack facing is locked during windup.
- Selected targets use stable enemy IDs, not stored vector pointers. Chase repaths are throttled and only replace the route when its goal tile changes or navigation ends. Auto and manual attacks share one ASPD cooldown; selection/repathing never bypasses recovery.
- Player hitboxes are circular sectors vs enemy world-space AABBs; melee also requires an unobstructed line to the enemy. Intersection checks corners, radial boundary segments, and circle/box-edge crossings; targets on the arc boundary are not missed. An attack serial prevents multiple hits on the same enemy in one swing.
- Hit feedback includes damage numbers, tint flashes, knockback, impact pause, screen shake, and a red screen flash when the player is hurt. Dying enemies fade and are erased; effects have bounded counts and lifetimes.
- The simulation runs at **120 Hz** with a capped accumulator. Paused time is discarded. Web press edges are buffered in addition to Raylib's held states, so a fast tap between two animation frames is still registered. Browser tab hiding pauses the game.
- All ownership uses values and standard RAII containers. There are no owning raw pointers, per-frame sprite allocations, loaded asset resources, blocking gameplay calls, or game-created threads. Desktop teardown releases the application and Raylib context; the browser loop uses `emscripten_set_main_loop`.

## Scope

This is a deliberately small, complete single-arena MVP, not a content-heavy ARPG engine. Level layout and encounters are authored/procedural, loot is represented by immediate embers/XP, and customization cycles the built-in equipment catalog. Persistence, audio, multiplayer, inventory storage, animation spritesheets, and multiple levels are intentionally left out.

## License

The repository's existing **GPL-3.0** license is retained. See [LICENSE](LICENSE). Raylib and toolchain acknowledgments are in [THIRD_PARTY.md](THIRD_PARTY.md).
