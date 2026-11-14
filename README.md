# Ashfall — The Ashen Sanctum

A self-contained isometric hack-and-slash MVP in **C++17 + Raylib 5.5**, built for **desktop and WebAssembly**. Enter a ruined sanctuary, cut down seven hollow and their Ash Warden, and keep your flame alive.

The repository also includes **Sprite Workshop**, a pixel-art and equipment editor built with the same C++17/Raylib stack. Open it from the game’s **SPRITE EDITOR** link, or build the native tool with `make editor`.

All game art is procedural: stone tiles, pillars, rubble, braziers, the portal, characters, equipment, attack trails, particles, and interface. The player uses **generated eight-direction spritesheets** with layered equipment; the original shape renderer remains a fallback. The only font is Raylib's embedded default font. **No external artwork, font downloads, asset packs, JavaScript game engine, or runtime art requests are required.**

## Play

| Input | Action |
| --- | --- |
| **Left click** ground | Navigate with A*; a new click smoothly replaces the route and cancels auto-combat |
| **Left click** monster | Select, chase into melee range, then automatically attack until it dies |
| **Right click / Space** | Manual cleave toward the cursor; hold to repeat at the same attack-speed cadence |
| **Shift** | Dash toward the cursor, with brief invulnerability; 2.4 s cooldown |
| **Q** | Drink a flask: +50 vitality, carry up to three; refill through drops or crafting |
| **1 / 2 / 3** | Swap **owned** Head / Armor / Boots spares; also click the equipment cards |
| **B** | Backpack: compare, equip/unequip, drop, confirm salvage, sort or craft flasks; pauses play |
| **Left click** loot / **E** | Walk to and collect a drop / collect reachable nearby drops |
| **Alt** (hold) | Show all ground-drop names |
| **I** | Show/hide the character panel |
| **U** | Import equipment artwork (`.ashgear`); native: drop the file onto the window |
| **G** | Toggle the navigation grid |
| **Mouse wheel / − / +** | Zoom; the middle zoom button restores the fitted view |
| **Esc / P** | Close the backpack first, otherwise pause/resume |
| **H / F1** | Open/close the field guide |
| **F** | Toggle fullscreen (browser fullscreen requires a user gesture) |
| **K / L** | Save / load one local run slot; a loaded run is paused |
| **R** | Start a fresh run (does not overwrite the saved run) |

The hollow pursue within aggro range and telegraph their melee sector in red. Step or dash out of it, or interrupt the windup with a hit. Every third player strike is a stronger, wider cleave. Death offers a new run; defeating all eight enemies completes the sanctum.

### Save and resume

Press **K** to save your run and **L** to load it. The **Pause**, death and victory dialogs also offer save/load and portable **`.ashrun` export/import**. Saves retain progression, owned/rolled equipment, exact backpack cells, ground loot, RNG, positions, enemies, navigation/target intent and in-flight attack/dash/cooldown state. Original v1 saves migrate without duplicate rewards or free items. Loading validates the whole snapshot before replacing the run, then pauses it; missing/corrupt/incompatible files leave the current run intact. A fresh launch does not automatically load a save, and Restart never overwrites it.

Browser quick saves use local **IndexedDB**, not a server. Native quick saves use the OS user-data directory (or **`ASHFALL_DATA_DIR`**). Native portable export writes `build/saves/run.ashrun`; import by drag/drop or `./build/native/ashfall --load /path/to/run.ashrun`. Run files never replace equipment artwork. See [Save formats, storage locations and editor recovery](docs/persistence.md).

### Click-to-target combat

Click a living monster's **body or feet** once—no held attack key is needed. The player follows a collision-safe path, stops in melee range, faces the target, and repeatedly strikes. A gold ground ring marks the selection; the **upper-center target panel** shows its name, current/max HP, health bar, chase/attack status, and strike interval. It stays centered in the visible play area as the window or inventory changes.

Default cadence is **one strike every 2.5 seconds**. Equipment changes attack speed (ASPD): `interval = 2.5 / (1 + sum(ASPD bonuses))`. The unmodified catalog spans **2.0–2.78 seconds** per strike; rolled loot adds ASPD within the overall **0.5–2.0×** multiplier limit. The character panel displays the effective multiplier and interval. The short swing animation is separate from recovery, and every third consecutive strike still performs a stronger cleave. Repeated target clicks, switching targets, and changing equipment **do not reset the current cooldown**.

Ground destinations are tile-based: clicking the **same destination tile again** preserves the active route and its waypoint progress, including the final approach. Clicking the **tile you currently occupy** as a new destination stops in place rather than pulling you back to its center. A genuinely different destination still replaces the route immediately.

A valid ground command (including a stop in the occupied tile), a dash, or a manual attack cancels auto-combat. Target death, player death, and restarting clear the selection; pausing preserves it without advancing combat timers. Unreachable monsters show “NO PATH” rather than swinging through walls. Enemy recovery has also been paced for the slower player cadence while retaining its windup/telegraph.

### Live equipment

Each slot has three pieces with a distinct tint/silhouette and actual gameplay stats. Total armor reduces each incoming hit by `floor(armor × 0.65)`, with a minimum of one damage. Movement speed is `3.6 + sum(equipment speed bonuses)` tiles/second. Iron sallets/greaves each give −5% ASPD; cinder crowns and ranger leathers +5%; crimson mantles and fleetstriders +10%. The player starts with 120 vitality; kills grant experience immediately and drop ember caches/gear on the ground. Each 100 XP increases level and damage.

### Backpack and loot

**B** opens a paused **24-cell backpack** with worn slots, item comparison and equip/unequip, reversible drop, confirmed salvage, sorting and **30-ember flask crafting**. You start with three worn Common items and three named spares, not every catalog piece. Shortcuts **1/2/3** only swap what you own. Full-bag swaps put the old piece into the incoming item's cell; failed pickups/unequips leave items intact.

Each Hollow drops one piece of gear and **15 embers**, with a 25% flask chance. The Warden drops **two pieces (one guaranteed Relic)**, **45 embers** and a flask. Gear has Common / Tempered / Relic rarity with bounded armor, movement and ASPD bonuses; the comparison shows actual rolled stats. Click a marker to walk to it, or press **E** nearby. Pickups require range and clear geometry, never collect through walls, never auto-equip and **do not expire** with their corpse. Victory offers **COLLECT THE SPOILS** so the final drops stay accessible without increasing the clear time.

All gear has stable ownership/IDs and persists with saves; reloading never rerolls rewards. Rarity does not change the artwork catalog or tint: edited frames apply to every owned instance of that base piece, never to its stats. See [Backpack, drop tables, economy and ownership](docs/loot.md).

### Animated player

The player has **eight projected facing directions**, a **4-frame idle**, **8-frame walk**, and **6-frame attack**. Layers share a **96 × 96 logical cell** and fixed foot pivot; walking follows actual distance traveled, and attacks follow the existing swing/damage timeline—not the ASPD cooldown. Head/armor/boots support all 27 catalog combinations as pieces are acquired, and the weapon moves behind or in front as the character turns. Pause/help/backpack freeze both the live sprite and paper doll. Empty worn slots omit that equipment layer.

Original starter sheets are generated once from bundled C++ code, uploaded with nearest filtering, and reused on restart. No PNG loading or web filesystem is needed. The previous procedural hero remains available if initialization fails or `ASHFALL_PLAYER_SPRITES=OFF` is selected. See [Player sprite format, timing, ownership, and fallback](docs/player-sprites.md) for the layout and verification details.

## Sprite Workshop

A separate tool alongside the game, supporting standalone PNGs and game equipment projects. It starts with a procedural 32 × 32 character you can edit, or a new transparent **16, 32, 64, or 128 px** square canvas.

- **Pencil, eraser, flood fill, color picker, straight line, and outline rectangle.** Fast drags are connected without gaps; one stroke is one undo step.
- **28-color palette** (including transparent), plus custom **RGB/RGBA hex** (`#RRGGBB` or `#RRGGBBAA`). Alpha is stored exactly, not flattened onto the checkerboard.
- **Undo/redo** with a bounded 64-step history, horizontal mirror painting, horizontal/vertical flips, and lossless clockwise rotation. Rectangular imports rotate without resampling.
- **Nearest-neighbor zoom**, cursor-anchored wheel zoom, grid, fit-to-canvas, Space/middle-button panning, and a live preview on larger windows. Compact windows use a bottom palette; click the foreground swatch to edit hex or the footer filename to rename.
- **Transparent PNG import/export at original pixel resolution**, independent of canvas zoom/grid. Imports accept 1–128 pixels per side and at most 8 MiB; invalid files leave your artwork intact.
- **Automatic draft recovery** for standalone sprites and whole gear projects, including active frame/selection and drawing settings. Checkpoints do not split undo strokes or count as exports; recovery prompts on reload and keeps restored work dirty. **F2** opens recovery/retries failed storage.
- New/import/leave confirmations protect unexported work; desktop exports also ask before replacing an existing file. Clear and transforms are undoable.

| Input | Action |
| --- | --- |
| **B / E / F / I / L / R** | Pencil / eraser / fill / picker / line / rectangle |
| **Right drag / Alt-click** | Erase / sample a color |
| **Ctrl/Cmd Z / Shift Z / Y** | Undo / redo (keep Ctrl/Cmd held for redo) |
| **Ctrl/Cmd N / O / S** | New canvas / open PNG or project / save PNG or whole equipment project |
| **G / M / 0** | Toggle grid / horizontal mirror / fit |
| **Wheel / − / +** | Zoom |
| **Space-drag / middle drag** | Pan without painting |
| **Delete / H or F1** | Clear (undoable) / help |
| **F2** | Open draft recovery / retry unavailable storage |
| **Enter / Esc** | Confirm / cancel the active dialog or text field |

**Browser:** use **OPEN FILE** or drop a PNG/`.ashgear` onto the canvas. Files stay local, with no server upload. The back arrow returns to Ashfall. Modified work is automatically checkpointed to a separate recovery draft. On reload, choose Recover or Discard; Esc defers the decision without overwriting it. Recovery starts a fresh undo history and remains unexported. Downloads are still your portable backups; final tab-shutdown writes are best-effort.

**Desktop:** drop a PNG or `.ashgear` onto the window to import; the Open button explains this workflow without adding a platform-specific file-dialog dependency. Export writes `build/sprites/<name>.png` relative to the working directory. The back arrow or window close asks before leaving unexported work. Automatic recovery uses the same OS user-data directory as game saves; PNG/gear export still uses `build/sprites`. Run the executable from the repository root for the documented export location.

### Edit and apply equipment

Click **BUILT-IN GEAR → ARMOR → Ashguard plate → WALK → SE → 3**. (The item-name button cycles the three catalog pieces.) Select **Head / Armor / Boots**, any item, any clip/facing/frame, and edit its full **96 × 96** transparent layer. **REFERENCE** shows the body and other gear; **SOLO / RAW** shows exact pixels. The animated preview uses the game's actual catalog tint, fixed foot pivot and front/back weapon order. It remains available in compact layouts.

**SAVE GEAR / Ctrl/Cmd S** saves **all edited frames**, including other items and directions, as a portable **`.ashgear`** project. **FRAME PNG** saves just the current raw layer and does **not** mark the project saved. Switching frames commits their pixels; undo history is bounded to the active frame. Open/import/new/leave guards protect the whole project.

- **Browser:** **APPLY TO GAME** explicitly stores a local IndexedDB snapshot and updates same-origin game tabs; new/reloaded games restore it. **OPEN APPLIED GEAR** reopens that snapshot for further editing. Save a `.ashgear` backup—storage may be unavailable or cleared.
- **Game:** press **U** / **IMPORT ART [U]** in the character panel, or drop `.ashgear`, to apply a project. **RESTORE ART** returns to the built-ins; restarting a run keeps applied art. Artwork does not change equipment names, stats, loadout, ASPD, damage, simulation or cooldowns.
- **Native:** save to `build/sprites/<name>.ashgear`, then drop it onto the game, or run `./build/native/ashfall --art build/sprites/<name>.ashgear`. No hidden preferences file is written.

Equipment-mode PNG imports must be **96 × 96** and replace only the selected frame, with confirmation and undo. Project files are validated and bounded to **50 MiB / 1,296 frames**; bad files or failed GPU allocation preserve the current art. New pixels outside the starter's cropped bounds are not clipped. Body/weapon authoring and arbitrary atlas PNG import/export remain outside this equipment workflow.

See the [equipment workshop guide and project format](docs/equipment-workshop.md) for detailed editing, persistence, recovery, native/browser differences and verification.

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

# Build/run just the companion editor:
make editor
./build/native/ashfall_sprite_editor
```

Or use CMake directly (also works on macOS/Windows):

```sh
cmake -S . -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native --config Release --parallel
```

With a multi-config Windows generator, the executables are `build/native/Release/ashfall.exe` and `build/native/Release/ashfall_sprite_editor.exe`. On macOS install CMake and Xcode's command-line tools; Raylib uses the system frameworks. The game window has a 640 × 480 minimum size; the editor minimum is 800 × 640.

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
# Game: http://localhost:8080/index.html
# Editor: http://localhost:8080/editor.html

# Or serve with the editor as the landing page:
make serve-editor
```

Equivalent commands without Make:

```sh
emcmake cmake -S . -B build/web -DCMAKE_BUILD_TYPE=Release -DASHFALL_BUILD_TESTS=OFF
cmake --build build/web --parallel
python3 tools/serve.py --directory build/web --port 8080
```

Serve **all six files together** from `build/web`: `index.html`, `index.js`, `index.wasm`, `editor.html`, `editor.js`, and `editor.wasm`. The relative links between apps also work when hosted in a subdirectory. Do not open the HTML with `file://`. The included server binds to `0.0.0.0`, sends the correct WASM MIME type, and allows iframe previews and arbitrary preview hosts. Static hosting is sufficient; there is no backend. The browser needs WebGL and WebAssembly, not cross-origin isolation or SharedArrayBuffer.

Release output is approximately **493 KiB for the game** and **520 KiB for the editor**, uncompressed (toolchain dependent). Each app starts with 32 MiB of WASM linear memory and allows growth; navigating between them unloads the other app. Threads, Asyncify, asset preloading, and Emscripten's filesystem are not used. Audio is intentionally not part of this MVP.

### Existing/offline Raylib

Use an existing source checkout without fetching:

```sh
cmake -S . -B build/native -DFETCHCONTENT_SOURCE_DIR_RAYLIB=/absolute/path/to/raylib-5.5
```

An installed Raylib package is also supported with `CMAKE_PREFIX_PATH`. Set `-DASHFALL_FETCH_RAYLIB=OFF` to require that package instead. Do not give the web build a native Raylib binary; use web-compiled Raylib or let CMake build it from source.

The renderer supports the Raylib **5.5 and 6.0** gradient APIs: 5.5 takes integer `x, y` arguments to `DrawCircleGradient`, while 6.0 takes a `Vector2` center. A compile-time adapter selects the signature declared by the selected header; no version override or downgrade is needed. The default fetched release remains 5.5. CI compiles the application against both releases' actual headers.

## Tests

### Display-independent game and sprite tests

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

Coverage includes both gradient API call shapes (without a graphics dependency), projection round trips/negative picking, collision and dash tunneling, A* invalid/unreachable/same-tile cases, optimal routing, no diagonal corner cutting, waypoint overshoot, continuous repeated rerouting with swept-corner clearance, same-destination spam vs. single-click trajectory equivalence, occupied-tile stopping, blocked/displaced route invalidation, equipment cycling/stats/attack speed, single-click target pursuit, cooldown preservation and stable-ID target cleanup, exact attack-sector intersection, cooldowns/invulnerability, one hit per swing, AI pursuit/telegraphs/dodging, death/removal, a complete eight-enemy playthrough, full-level connectivity, and a 100-second simulation stress run with collision and memory-budget invariants.

Character tests additionally verify projected facing/hysteresis, idle/walk/attack timing, distance-driven gait, pause/reset, every generated sheet cell and its padding/registration, all 27 equipment combinations, distinct directional bodies, and simulation equivalence with/without animation.

Sprite editor tests additionally cover RGBA/hex handling and bounds, connected lines, stroke transactions, saved revisions, undo/redo branching, mirrored paint, bounded fill, transparency, rectangular transforms, the 64-entry history limit, PNG header/dimension preflight, and the procedural starter. `ASHFALL_BUILD_GAME=OFF` disables **both** graphics applications, keeping all six core suites free of Raylib. Project tests additionally cover all gear identities, raw/RLE round trips, full-frame registration, save points, frame switching and a malformed-file corpus. Persistence tests cover exact gameplay continuation, save/recovery codecs and malformed input, dirty/history semantics, autosave scheduling, bounded native reads and atomic file replacement. The loot suite adds over 290,000 assertions for ownership, rarity bounds, capacity-neutral swaps, full-bag failures, pickup geometry, economy/cooldowns, v1 migration, hostile v2 records and mixed-transaction checkpoint replay.

### Real browser smoke tests (optional)

Neither application has an npm runtime dependency. Playwright is only needed for these development tests:

```sh
emcmake cmake -S . -B build/web -DASHFALL_BUILD_TESTS=OFF -DASHFALL_WEB_TEST_API=ON
cmake --build build/web --parallel
npm install --no-save --package-lock=false playwright
npx playwright install chromium

# In one terminal:
make serve
# In another:
node tests/browser_smoke.mjs
node tests/character_sprite_smoke.mjs
node tests/sprite_editor_smoke.mjs
node tests/equipment_art_smoke.mjs
node tests/persistence_smoke.mjs
node tests/loot_smoke.mjs
```

This drives real mouse/keyboard inputs, checks gear/ASPD stats, blocked-tile picking, repeated mid-tile route replacement, same-destination/occupied-tile click spam without recentering or waypoint resets, body/feet target picking, a single-click chase and kill at the expected strike interval, live target HP, HUD click-through prevention, cancellation, dodging, pause, guide, restarting, death/removal cleanup, and target-panel layout after canvas resizing. Screenshots go into ignored `build/browser/`. `GAME_URL` overrides the game URL; `EDITOR_URL` overrides the editor URL. `PLAYWRIGHT_MODULE`, `CHROMIUM_EXECUTABLE`, and `CHROMIUM_ARGS` support an existing browser installation.

The editor test uses real input and the actual C++/WASM app to verify gapless strokes, erasing, picker, mirror, line/rectangle/fill, custom RGBA/name input, PNG downloads and pixel/alpha round-trips, the file picker and drag/drop, destructive-operation guards, rectangular rotation, invalid PNG rejection, responsive layout, anchored zoom, and panning.

The character browser test also observes actual WebGL uploads and nearest/clamp filtering, checks directional body/weapon poses, gear reuse, distance-driven walking, normal/third-combo swing timing, and frozen animation during pause/help. CI verifies shader-failure cleanup with `SPRITE_SHADER_FAILURE=1`, then runs it again with `ASHFALL_PLAYER_SPRITES=OFF` (`SPRITES_EXPECTED=0`) to exercise the explicit procedural fallback.

The equipment browser test verifies real frame painting/tinting, project/PNG export, all catalog selectors, protected imports, live game application, origin-local persistence, unchanged stats, full-cell GPU rendering, partial-allocation failure cleanup, restart/restoration, compact previews and denied storage.

The persistence browser test verifies actual IndexedDB run saves and whole-project recovery, file export/import, mid-stroke checkpoints, dirty/save-point invariants, corrupt/oversized inputs, delayed-I/O/Restart and late-recovery races, quota/retry, multi-tab writer ownership and denied storage.

The loot browser test covers actual combat drops/pickups, paused backpack UI, owned-only shortcuts, item comparisons, empty gear layers, confirmed salvage, crafting, capacity/duplicates, sorting, exact v2 persistence, real-file v1 migration and post-victory loot. Synthetic capacity/victory fixtures use the normal portable file picker, not mutation hooks. CI also exercises inventory with sprites disabled.

CI builds both native apps, compiles against actual Raylib 5.5/6.0 headers, runs all six core suites with/without sanitizers and all six browser suites, then rebuilds without diagnostics and runs `node tests/release_smoke.mjs` to verify production startup, PNG/project export, game save/load, owned inventory/victory pickups, editor draft recovery, artwork application and inter-app navigation. The `ashfall-web` artifact contains both complete apps.

`ASHFALL_WEB_TEST_API` is **off by default**. It adds only a read-only state snapshot for tests; no teleport, invulnerability, damage, or other test-only mutation is exposed. Turn it off for a normal release. `make web` explicitly disables it, including after a diagnostic test build.

## Architecture

```text
src/
  types.h                  Small graphics-independent vector, tile, color, bounds types
  grid.h / .cpp            2:1 projection, inverse picking, level, swept/substepped collision
  pathfinding.h / .cpp     Reusable eight-connected A* workspace + waypoint following
  equipment.h / .cpp       Base catalog + owned instances/rarity rolls, optional worn slots and effective stats
  loot.h / .cpp            Bounded backpack, atomic transfers, sorting and deterministic gear rolls
  world_loot.cpp           Enemy tables, stable drop ownership, pickup routes, salvage/crafting
  player.h / .cpp          Navigation, ASPD/combo timers, dash, health, mitigation
  combat.h / .cpp          Enemy state + exact sector/AABB collision geometry
  world.h / .cpp           Target selection/chase/auto-combat, AI, damage/death, effects
  character_animation.*    Read-only directional/clip sampling and distance-based visual clock
  character_sprites.*      Graphics-independent starter sheet generation and packing metadata
  character_visual.*       RAII atlas/shader/override ownership, tinted equipment, anchored quads
  equipment_art.*           Graphics-independent equipment project/frame model + validated portable codec
  equipment_browser.h      Async browser files, IndexedDB and same-origin apply notifications
  run_save.*               Validated versioned gameplay snapshots, no graphics or file I/O
  editor_recovery.*        Whole-document/project recovery codec and autosave scheduling
  snapshot_codec.h         Shared bounded little-endian/checksummed byte envelope
  snapshot_browser.h       Async/coalesced IndexedDB run/draft storage and editor writer lease
  local_store.*            Native bounded reads, data-directory policy and atomic file replacement
  renderer.h / .cpp        World/loot art, stable Y-sort, player sprite/fallback, HUD, paper doll, UI
  inventory_ui.cpp         Responsive paused backpack, actual-stat comparison and owned-item controls
  raylib_compat.h          Compile-time drawing API compatibility (Raylib 5.5/6.0)
  main.cpp                 Raylib input, fixed-step driver, native/web lifecycle
  sprite_document.h / .cpp Graphics-independent RGBA document, tools, history, PNG preflight
  sprite_editor.cpp        Separate Raylib app: editor UI, textures, input, native/web file glue
web/shell.html             Game loading/fullscreen shell + relative editor link
web/editor.html            Editor canvas/loading shell + local PNG/project file input
tests/core_tests.cpp        Native game tests, no third-party test framework
tests/character_tests.cpp   Native animation/art/registration/equipment tests, no graphics dependency
tests/sprite_tests.cpp      Native editor-document tests, no graphics dependency
tests/equipment_art_tests.cpp Native equipment-project/codec/save-point tests, no graphics dependency
tests/persistence_tests.cpp Native save/recovery/hostile-data/scheduling/storage tests
tests/loot_tests.cpp        Native inventory/drop/economy/ownership/migration/stress tests
tests/browser_smoke.mjs     Actual-browser game integration checks
tests/character_sprite_smoke.mjs Player animation, observed GPU uploads and fallback checks
tests/sprite_editor_smoke.mjs Actual-browser editor/PNG integration checks
tests/equipment_art_smoke.mjs Equipment editing/projects/apply/persistence/GPU integration
tests/persistence_smoke.mjs Actual-browser game saves, editor recovery, storage and race checks
tests/loot_smoke.mjs        Actual-browser backpack/loot/rarity/migration/victory checks
tests/fixtures/             Small genuine-v1 and synthetic-v2 portable run test inputs
tests/release_smoke.mjs     Production exports/apply/save/load/recovery without diagnostics
tests/png_helpers.mjs       Small PNG fixtures/reader used only by browser tests
tools/serve.py             Preview-safe static server; optional --page editor.html
```

### Coordinate and depth conventions

- Simulation positions are **tile units**; tile `(x,y)` has center `(x+0.5,y+0.5)`.
- `IsoProjection::worldToScreen` maps onto the untransformed 2:1 isometric plane:
  `screenX = originX + (x-y) × 32`, `screenY = originY + (x+y) × 16`.
- Input first uses Raylib `GetScreenToWorld2D` to undo the camera, then the inverse projection. Tile selection uses **floor**, including negative coordinates.
- Floor/light/telegraphs render first. Obstacles, portal, loot, enemies, and the player share **one stable draw list sorted by projected foot Y**, rather than raw grid Y. Floating effects render afterwards.
- The player and paper doll share synchronized **Body → Boots → Armor → Head** sprite layers, with a rear/front weapon pass chosen by projected facing/swing. Fixed logical foot pivots preserve continuous position and foot-Y sorting; trimmed atlas cells only remove shared empty margins.

### Navigation, combat, and lifetime

- A* uses an octile heuristic, costs `1` and `sqrt(2)`, no diagonal corner cuts, deterministic tie-breaking, and reused node storage. Paths exclude the start and include the destination's center.
- Reroutes splice the replacement path onto the actor’s **actual position**, not its previous tile center. A swept-circle segment test checks the new turn; re-centering is only a fallback when necessary to avoid a blocked corner. Following consumes a movement budget across multiple waypoints without overshooting. Movement, dash, separation, and knockback all use the **same circle-vs-grid collision routine**, subdividing long motion to prevent tunneling.
- Enemy state: **Idle → Chase → Attack (windup, hit, recovery) → Chase**, with terminal **Dead**. Pursuit uses throttled A* repaths, not movement through obstacles. Attack facing is locked during windup.
- Selected targets use stable enemy IDs, not stored vector pointers. Chase repaths are throttled and only replace the route when its goal tile changes or navigation ends. Auto and manual attacks share one ASPD cooldown; selection/repathing never bypasses recovery.
- Player hitboxes are circular sectors vs enemy world-space AABBs; melee also requires an unobstructed line to the enemy. Intersection checks corners, radial boundary segments, and circle/box-edge crossings; targets on the arc boundary are not missed. An attack serial prevents multiple hits on the same enemy in one swing.
- Hit feedback includes damage numbers, tint flashes, knockback, impact pause, screen shake, and a red screen flash when the player is hurt. Dying enemies fade and are erased; effects have bounded counts and lifetimes.
- The simulation runs at **120 Hz** with a capped accumulator. Paused time is discarded. Web press edges are buffered in addition to Raylib's held states, so a fast tap between two animation frames is still registered. Browser tab hiding pauses the game.
- Ownership uses values and RAII. Five generated character textures and one hit-flash shader are loaded once; temporary CPU sheets are freed after upload. Failed initialization releases partial resources and uses the procedural fallback. There are no per-frame sprite allocations, runtime asset downloads or game-created threads. Persistence I/O is separate from the simulation; browser database operations are asynchronous. Desktop teardown releases GPU resources before the Raylib context; the browser loop uses `emscripten_set_main_loop`.

## Scope

This is a deliberately small, complete single-arena MVP, not a content-heavy ARPG engine. Authored encounters now support a bounded backpack, owned equipment, Common/Tempered/Relic drops, ground ember/flask pickups, item comparison, salvage and flask crafting. Gameplay save/load (including v1 migration) and automatic editor recovery support the bundled sanctum/catalog. Audio, multiplayer, extra levels/world editing, editable body/weapon layers, arbitrary timelines, onion skinning, full-atlas PNG authoring and additional hurt/death clips remain for later feature PRs. Merchants, durability, quest items and weapon loot are not implied by this equipment-loot system.

## License

The repository's existing **GPL-3.0** license is retained. See [LICENSE](LICENSE). Raylib and toolchain acknowledgments are in [THIRD_PARTY.md](THIRD_PARTY.md).
