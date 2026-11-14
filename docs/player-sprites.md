# Directional player sprites — first milestone

The game now draws a genuine eight-direction body with **idle, walk, and attack** clips. The starter art is original procedural pixel art bundled as C++ source, not a downloaded asset pack. `character_sprites.cpp` generates the RGBA sheets once after the graphics context opens; no PNG file, asset server, Emscripten filesystem, build-time art tool, or network request is needed.

## Logical sheet format

Every frame is authored on a **96 × 96 transparent canvas**, with its foot pivot at **(48, 76)**. Facing is quantized *after* the 2:1 isometric projection, with a five-degree hysteresis band to prevent row flicker. For example, tile-space `+X` faces screen **SE**, and tile-space `(+1,+1)` faces screen **S**.

| Clip | Frames | Logical columns | Playback |
| --- | ---: | --- | --- |
| Idle | 4 | 0–3 | 1.2-second breathing loop |
| Walk | 8 | 4–11 | One cycle per 2 tiles of actual travel |
| Attack | 6 | 12–17 | Windup, three damage-window poses, two recovery poses |

Rows, in order: **S, SW, W, NW, N, NE, E, SE**. Columns concatenate the clips; they do not contain unused placeholder frames. `ClipInfo`, `SheetInfo`, and `Pose` are the metadata source of truth.

### Synchronized layers and GPU packing

Five sheets are resident on the GPU: **body, boots, armor, head, weapon**. Equipment sheets contain three variants, with row index `variant × 8 + direction`. Equipment layers use neutral shading multiplied by the live catalog tint. Their shapes also differ: hood/sallet/crown, leathers/plate/mantle, and wraps/greaves/fleetstriders. All **27 catalog combinations** remain renderable as owned pieces are acquired. Rarity/rolled bonuses belong to the gameplay instance and never change the atlas key/tint or generator. Empty worn slots skip their gear layer; body and weapon still draw.

To avoid wasting GPU memory on empty borders, each layer uses the same fixed crop for **every** frame, row, and variant:

| Layer | Logical crop (x, y, width, height) | Packed texture | Variants |
| --- | --- | --- | ---: |
| Body | 20, 8, 56, 74 | 1008 × 592 | 1 |
| Boots | 24, 50, 48, 36 | 864 × 864 | 3 |
| Armor | 20, 24, 56, 56 | 1008 × 1344 | 3 |
| Head | 30, 6, 36, 42 | 648 × 1008 | 3 |
| Weapon | 0, 0, 96, 96 | 1728 × 768 | 1 |

A cell is positioned at `feet + (cropOrigin − footPivot) × scale`. Trimming therefore **never changes the anchor**. Empty padding prevents neighboring-cell bleed; textures use nearest filtering and clamp wrapping, with no mipmaps. Every texture fits WebGL 1's minimum supported 2048-pixel limit. Total RGBA texture storage is about **17.85 MiB**; the largest temporary CPU sheet is about **5.2 MiB**, and CPU copies are freed immediately after upload.

Draw order is rear weapon (when facing/sweeping away), body, boots, armor, head, then front weapon. World shadow, selection ring, depth ordering, trails, and damage effects remain separate. The paper doll uses the same sheets, layers, and pivot, with an idle SE pose rather than mirroring combat animation. Dash uses a held lunge pose and translucent sprite afterimages. Death retains the procedural fallen-body visualization; separate hurt/death clips are later work.

## Animation follows gameplay

`character::Animator` only reads `const Player&`. It does not move the player, alter facing, advance attack timers, trigger hits, or reset cooldowns.

- Walking uses accumulated **actual continuous displacement** after simulation updates. Faster equipment naturally plays the gait faster; hitting a wall does not march in place. Teleports/restarts rebase the visual clock instead of spinning the walk cycle.
- Attack poses sample the existing `attack.elapsed` and `attack.duration` timeline. Frame 0 anticipates the hit, frames 1–3 span the shared **0.075–0.24 s** damage markers, and frames 4–5 cover recovery. Both 0.36 s normal and 0.43 s third-combo swings retain those damage markers. The **2.5 / ASPD** cooldown is not an animation duration.
- Attack direction stays locked to `attack.direction`, even if movement resumes during recovery. Eight-direction quantization affects art only; the world-space collision sector and attack trail keep their true continuous direction.
- Idle has its own simulation-following clock; it is not tied to `Player::animation`, which historically advances only during movement.
- Pause, help, tab-hide pause, and hit-stop freeze the visual clocks. Restart resets animation but does **not** recreate textures.
- Character positions and foot-Y depth sorting are unchanged. Texture filtering never rounds or snaps simulation movement.

## Ownership and fallback

`character::Sprites` is non-copyable and owns the five textures, hit-flash shader, and any explicitly imported full-cell equipment overrides. Imports stage and validate new textures before replacing old ones; failure releases the staged batch and preserves the old project. Replacing/restoring art also releases its old GPU overrides. It attempts loading only once. A failed texture upload or shader initialization releases partial resources and leaves the original procedural renderer active. Desktop application destruction releases all GPU resources **before** `CloseWindow()`; browser navigation/context destruction releases the browser's resources.

The original renderer can also be selected explicitly, without altering simulation or the Sprite Workshop:

```sh
cmake -S . -B build/native -DASHFALL_PLAYER_SPRITES=OFF
cmake --build build/native --parallel

# Or for the browser build:
emcmake cmake -S . -B build/web -DASHFALL_PLAYER_SPRITES=OFF
cmake --build build/web --parallel
```

Set `ASHFALL_PLAYER_SPRITES=ON` to restore sprites (the default on a fresh configure).

## Verification and equipment editing

`make test` and `make sanitize` include graphics-independent animation/art tests: direction quantization, hysteresis, all clip frames, exact damage markers, distance cadence, pause/reset, all **1,584 layer/frame/direction/variant cells**, alpha/padding/crop bounds, shared registration, eight distinct body silhouettes, all 27 gear combinations, and simulation equivalence with/without animation updates.

With `ASHFALL_WEB_TEST_API=ON`, run `node tests/character_sprite_smoke.mjs` alongside the other browser suites. It drives real input, observes actual WebGL atlas uploads/filtering, checks all directions, gear changes, walking, three consecutive combo swings, independent cooldowns, and pause/help/restart. For a fallback build run `SPRITES_EXPECTED=0 node tests/character_sprite_smoke.mjs`. On a normal sprite build, `SPRITE_SHADER_FAILURE=1 node tests/character_sprite_smoke.mjs` injects an invalid shader at the browser's graphics boundary and verifies all five textures are actually deleted before continuing with the procedural renderer. No gameplay state is mutated by test hooks. Diagnostics remain read-only and are excluded from production builds.

**Sprite Workshop** now opens built-in equipment with slot/item/clip/facing/frame selectors, edits full 96px layers against the body/other-gear reference, and previews with this same renderer. Portable `.ashgear` projects and explicit browser/native game imports override only edited equipment cells, without clipping their outer pixels or changing gameplay. See the [equipment workshop guide](equipment-workshop.md). Standalone PNG editing remains supported; arbitrary layer/timeline authoring, onion skin and full-atlas PNG import/export remain separate work.
