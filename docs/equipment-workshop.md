# Equipment artwork in Sprite Workshop

Sprite Workshop is a C++17/Raylib editor, with the same tools on native desktop and WebAssembly. It supports both independent PNG sprites and **editable game equipment**. The body and weapon remain read-only references; equipment names, stats, attack speed, damage and simulation are never imported from artwork files.

## Edit the built-in armor

1. Open **SPRITE EDITOR** in the game, or open `editor.html`. Native: `make editor && ./build/native/ashfall_sprite_editor`.
2. Click **BUILT-IN GEAR**. If another sprite/project has unsaved changes, save it first or confirm replacement.
3. Select **ARMOR → Ashguard plate → WALK → SE → 3**. The long item-name button cycles that slot's three catalog items. Frame numbers start at **1** in the UI.
4. Paint with the existing pencil, eraser, fill, line, rectangle, palette, hex, mirror and transform tools. You are changing **only that equipment layer, in that one frame**.
5. **SAVE GEAR** / **Ctrl/Cmd S** downloads the whole equipment project as `equipment.ashgear` (rename it in the project-name field/footer). Save before leaving, even if you have applied the art to the game.
6. Use **APPLY TO GAME** in the browser, or import the saved `.ashgear` file in the game as described below.

All three **Head / Armor / Boots** slots and all nine catalog pieces are available. Each piece has **Idle (4), Walk (8), Attack (6)** frames in **S, SW, W, NW, N, NE, E, SE**. No complete atlas needs to fit in the single-frame editor.

### Canvas and preview

- Every equipment frame is a full **96 × 96 RGBA** layer. The turquoise cross is the fixed foot pivot **(48, 76)**. Pixels outside the starter art's original packed crop are preserved and render in the game, too.
- **REFERENCE** shows the body, weapon and other gear, using the **same renderer, catalog tint and direction-dependent weapon ordering** as the game. Change the head/boots selections to inspect different reference combinations. Only the currently selected slot/item/frame receives paint.
- Toggle **REFERENCE → SOLO / RAW** to see the exact stored, untinted pixels. Catalog tint multiplies raw RGB, just as in the game; start with neutral/light colors if you want the catalog color to dominate. The picker samples raw editable-layer pixels, not the composited body/reference.
- The separate **GEAR PREVIEW** animates the selected clip and facing. **PAUSE / ANIMATE** freezes it on the selected frame or resumes playback. The editing canvas never advances underneath your stroke. Walk preview uses the same distance sampler at a demo travel rate; attack preview uses the shared damage-marker sampler, not the gameplay cooldown. Actual game animation remains simulation-driven.
- Reference choices, preview playback and canvas navigation are view state, not changes to the game's loadout or project artwork. Compact windows retain a small character preview and bottom palette; the desktop editor minimum is 800 × 640.

### Switching, undo and resets

A stroke is one undo entry. **Switching frames commits the current frame into the project**; returning to it restores your pixels. Undo/redo history is bounded to **64 edits on the active frame** and starts fresh when changing frames, to avoid thousands of large histories. The frame buttons display `*` for overrides. The footer's `*` and leave/new/import/before-unload guards account for **the entire project**, including edits in other clips/items/directions.

**RESET FRAME** restores only the selected frame to the built-in generator and is undoable. **CLEAR** makes only that gear frame transparent and is undoable. **NEW BUILT-IN** starts a fresh equipment project, asking first if the current sprite/project is unsaved. **NEW** creates an independent PNG canvas instead.

## PNG versus equipment project

| Operation | Result |
| --- | --- |
| Open/drop a PNG in standalone mode | New independent 1–128 px document, with replacement protection |
| Open/drop a PNG in equipment mode | Must be **exactly 96 × 96**; confirm replacement of only the current gear frame; undo remains available |
| **FRAME PNG** | Exports one **raw, untinted, transparent gear layer**, not a screenshot or flattened character |
| **SAVE GEAR** | Exports **every modified frame across all items, clips and directions** as `.ashgear` |
| Open/drop `.ashgear` | Validates and opens a whole equipment project; asks before replacing unsaved work |
| **OPEN APPLIED GEAR** (browser) | Opens the last explicitly applied project from this browser, using the same replacement guard |

Exporting one frame PNG or applying a project **does not mark the whole project saved**. Saving `.ashgear` does. Unchanged frames are regenerated from the bundled versioned starter art. PNG imports are limited to **8 MiB**; projects to **50 MiB**, at most **1,296** edited frames. Invalid/truncated/unsupported files leave current artwork untouched.

## Apply in the browser game

**APPLY TO GAME** explicitly replaces the browser's applied artwork project. It stores a local binary snapshot in **IndexedDB**, not on a server. Other open game tabs on the **same origin** update via BroadcastChannel when supported. A newly opened or reloaded `index.html` restores that snapshot automatically. If live tab messaging is unavailable, reload the game. Applying a new project replaces, rather than merges with, the previous one; frames absent from the new project use the built-ins.

Alternatively, in the game press **U** or click **IMPORT ART [U]** in the character panel and choose a `.ashgear` file. Dropping a project onto the game canvas also imports it. The import command pauses play; resume with **Esc / P**. Imports are presentation-only and work while paused.

**RESTORE ART** in the character panel removes the applied overrides and browser snapshot, returning to the generated artwork without changing equipped pieces or stats. Restarting a run does **not** clear artwork or recreate its textures. Browser storage is origin-specific: a different host/port/browser does not share it. Private browsing, embedded contexts, quotas or permission settings can prevent persistence; a message explains failure instead of claiming success. A direct file import still applies for the current game session if persistence fails. **Keep a downloaded `.ashgear` backup.** In-progress strokes now have separate [automatic editor recovery](persistence.md#automatic-editor-drafts); autosave never changes the explicitly applied artwork or counts as a portable project save.

## Apply in the native game

Native **SAVE GEAR** / **SAVE TO APPLY** writes `build/sprites/<project-name>.ashgear` relative to the working directory, with overwrite protection. Drop that file onto the running game window to apply it. **U** / **IMPORT ART** explains the drop workflow; there is no platform-specific file-dialog dependency.

You can also load an artwork project at startup:

```sh
./build/native/ashfall --art build/sprites/equipment.ashgear
```

**RESTORE ART** reverts to the built-ins for that process. There is no hidden native preferences file or automatic overwrite of source art; restart with `--art` or drop the project again in a later session. Browser and native projects use the identical portable format. Run the executables from the repository root for the documented output path.

## Implementation and file contract

- `equipment_art.*` is graphics-independent: stable slot/item/clip/direction/frame identities, immutable full-frame overrides, project save points, active-frame document history, and bounded serialization/validation. It links the existing generator and pixel-document cores, not Raylib.
- `.ashgear` **file version 1 / starter catalog version 1** has magic `ASHGEAR\0`. All integers are explicitly little-endian, never native struct dumps. The header checks 96px dimensions, foot pivot, direction/clip/slot/variant counts, payload length/count and an FNV-1a-32 corruption checksum. The checksum detects accidental damage; it is **not** a cryptographic signature.
- The 37-byte fixed header is: magic (8), file/catalog versions (2 each), cell/pivot x/pivot y (2 each), six one-byte counts (`8,4,8,6,3,3`), frame count/payload length/checksum (4 each), name length (1). A 1–28 character ASCII project name follows, then frame records. Names allow only letters, numbers, `_` and `-`; they cannot specify paths or gameplay fields.
- Each sorted, unique record contains five one-byte identities (**slot, variant, clip, direction, zero-based frame**), encoding (1), encoded length (4), then full-frame pixels. Encoding 0 is 36,864 raw RGBA bytes. Encoding 1 is repeated **u16 run length + RGBA**; runs must fill exactly 9,216 pixels. The encoder chooses raw for noisy frames, so compression cannot inflate a frame. No zlib/JSON/parser dependency is needed. Changing the generator recipe or catalog identities requires a catalog-version migration or bump.
- `character::Sprites` owns override textures. Imports stage/validate every new 96px RGBA allocation with framebuffer completeness before swapping; on failure all partial allocations are deleted and the old art stays active. Full frames bypass only the affected gear cell's built-in crop, retaining its common pivot, tint and draw order. A single edited frame updates incrementally in the workshop. No texture uploads occur during game animation, gear cycling or restart. Replacement, restoration and shutdown release old textures.
- `equipment_browser.h` is just asynchronous file/IndexedDB/broadcast glue. C++ validates the bytes and controls rendering. The Emscripten filesystem remains disabled (`-sFILESYSTEM=0`), and no external art, runtime asset server, or npm runtime dependency is introduced. If player sprites are disabled or their initialization fails, the game keeps its procedural fallback and rejects project application with an explanation.

Project files do not author the body/weapon, add catalog items, change clip lengths, alter animation timing, edit stats, or permanently rewrite generated C++ art. Onion skinning, arbitrary layers and full-atlas PNG import/export are not part of this workflow.

## Verification

`make test` / `make sanitize` include project identity/registration tests for all 1,296 gear cells, exact RGBA/raw/RLE round trips, transparent-frame overrides, immutable save points, frame switching, undoable replacement, bounds, duplicate identities, truncation, bad metadata/checksum, and a deterministic malformed-file corpus.

With `ASHFALL_WEB_TEST_API=ON`, run `node tests/equipment_art_smoke.mjs` against the built apps. It uses real input to check selection, painting, undo, tint/raw rendering, animation, project/PNG save distinction, file round trips, all item/clip/direction selectors, compact layout, explicit apply, live game-tab updates, persistence, stat invariance, corruption rejection, genuine partial GPU allocation failure cleanup, replacement/restoration, restart caching and denied browser storage. `node tests/release_smoke.mjs` also verifies PNG and equipment operation with diagnostics removed. All screenshots and exported test projects stay in ignored `build/browser/`.
