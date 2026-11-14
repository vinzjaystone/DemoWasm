# Game saves and editor recovery

Both applications remain **C++17 + Raylib**, built natively or as filesystem-free WebAssembly. Serialization/validation is graphics-independent C++; JavaScript only moves bytes between WASM, local files and IndexedDB. Nothing is uploaded to a server.

## Save and resume a run

- **K** saves the current run to **one local slot**, replacing that slot. Saving does not pause gameplay; pause first for an exact stationary checkpoint.
- **L** loads that slot, replacing the current run **only after full validation**. The result is always paused: **P / Esc** or **RETURN TO THE SANCTUM** resumes an ordinary run; cleared runs offer **COLLECT THE SPOILS** before restarting. A missing, corrupt, oversized or incompatible save leaves the current run intact.
- **Pause**, death and victory dialogs also provide **SAVE RUN**, **LOAD RUN**, **EXPORT .ASHRUN** and **IMPORT .ASHRUN** buttons. The field guide lists the shortcuts.
- **EXPORT .ASHRUN** downloads `run.ashrun` in the browser, or writes `build/saves/run.ashrun` on desktop. Native export deliberately replaces that fixed export file, just as K replaces the quick-save slot. Keep separately named copies for multiple checkpoints.
- Browser **IMPORT .ASHRUN** opens a local file picker; dropping a `.ashrun` onto the game works too. Desktop: drop the file, or run `./build/native/ashfall --load /path/to/run.ashrun`.

A save includes health/flasks, XP/embers, worn item identities/rarities/rolls and empty slots, exact 24-cell backpack contents, all ground gear/ember/flask records, the loot RNG/next ID/click-to-loot intent, player/enemy positions and knockback, navigation and waypoint cursors, stable enemy/target IDs and pursuit intent, kills, elapsed run time, dash direction/remaining time, invulnerability, enemy windup/hit/recovery state and player swing/combo/cooldown/attack serials. A swing that already hit an enemy cannot hit it a second time just because it was loaded. Mid-dash and mid-chase saves continue instead of resetting their cooldowns or routes. Dead and cleared runs can also be saved/loaded.

Particles, damage labels, notices, backpack-open/item-inspection state, camera/UI preferences and the independent directional-sprite animator clocks are not restored. The short simulation hit-stop timer is restored. The animator is resynchronized to the loaded gameplay state. **Equipment artwork is separate:** loading a run never replaces applied/imported art or reads stats from an artwork project.

K is explicit saving, **not automatic gameplay autosaving**. A fresh launch starts a fresh descent until L is pressed. Restart, death, victory and portable-file import do not overwrite the quick-save slot. A pending browser Load is invalidated by a newer Save/Load/import, Restart or opening the backpack to manage items, so an old asynchronous result cannot replace an explicitly restarted run.

## Automatic editor drafts

The workshop checkpoints modified standalone sprites **or the entire equipment project**, not just the visible gear frame:

- Changes are coalesced after about **0.6 seconds** of inactivity, with checkpoints during continuous painting about every **2 seconds** while the app is running. Focus loss/hidden-tab events also request an immediate checkpoint. Capturing an in-flight pencil/eraser stroke does **not** commit/split its live undo transaction.
- A draft includes original-resolution RGBA (including hidden RGB under zero alpha), all edited gear frames, selected slot/item/clip/facing/frame, committed filename, foreground RGBA, grid/mirror and reference/preview-playing settings.
- On the next editor launch/reload, **RECOVER EDITOR DRAFT?** offers **RECOVER DRAFT** or **DISCARD DRAFT**. **Esc** defers the decision; **F2** / the footer recovery button opens it again. A late storage read never replaces work the user already started.
- Until that decision, **autosave waits** instead of overwriting the undecided draft. Discard deletes the old stored draft, not the current canvas. Recover explicitly replaces the current canvas/project; export any newer work first.
- Recovered work remains **unexported/dirty**. Undo/redo history starts fresh; no old exported save point is invented. **EXPORT PNG** saves a standalone sprite; **SAVE GEAR** saves the whole gear project. Only those explicit exports clear the corresponding draft when the document/project becomes clean. Undoing all changes back to an existing save point clears it too.
- A **FRAME PNG** export or **APPLY TO GAME** is **not** a whole-project save and does not clear gear recovery. Applied artwork still uses the separate existing `ashfall-equipment` database; automatic drafts never apply themselves to the game.
- New/open/built-in replacement still requires the existing dirty-work confirmation. Confirming a replacement and leaving its fresh document unmodified clears the previous draft. Back/Leave first flushes recovery; browser Back waits for queued writes before navigating. Native writes complete before exiting.

**Drafts are a safety net, not a portable backup.** A browser kill, crash or power loss can interrupt the latest write. A long stall can delay checkpoint scheduling. Browser/tab shutdown flushes are best-effort; keep downloaded PNG/`.ashgear` files. Partially entered text, unfinished line/rectangle previews, undo histories, zoom/pan and GPU resources are not stored.

If storage is denied/full/blocked, the footer switches to **RETRY [F2]** and a notice explains the problem. Artwork remains editable and dirty; PNG/project export still works. F2 retries storage. Corrupt drafts are kept until explicitly discarded; they cannot partially replace the current artwork. There is no automatic retry/error-notice storm.

## Where data lives

### Browser

IndexedDB **`ashfall-saves` version 1**, object store **`snapshots`**:

| Key | Bytes | Purpose |
| --- | --- | --- |
| `run` | `.ashrun` | Explicit quick-save slot |
| `editor` | `.ashdraft` | Automatic editor recovery slot |

Storage is origin-local: changing host, port, HTTP/HTTPS, browser or profile uses different data. Private/embedded contexts, quotas, browser data clearing and eviction can prevent or remove persistence. No cookies, credentials, Emscripten virtual filesystem, localStorage-sized payloads or network backend are used.

Reads wait for earlier queued writes; operation epochs ignore stale completions. Writes are serialized/coalesced, retaining at most **one active plus one latest pending snapshot**, so a slow database cannot accumulate unbounded project copies or resurrect an old stroke after a clear/export. Success is reported only after transaction commit.

Where **Web Locks** are available, one editor tab owns the recovery writer for this origin. A second tab can still edit/export but displays an ownership warning instead of silently overwriting the first tab's recovery. Close the owner and press F2 in the other tab to retry. On browsers without Web Locks, use one editor tab: that shared draft is last-completed-writer-wins. Game quick saves are intentionally a shared, explicitly overwritten slot; there is no cross-tab automatic gameplay load.

### Native

Run and draft files are `run.ashrun` / `editor.ashdraft` under:

1. **`ASHFALL_DATA_DIR`**, if set (useful for portable installs and tests).
2. Windows: `%LOCALAPPDATA%/Ashfall`, then `%APPDATA%/Ashfall`.
3. macOS: `$HOME/Library/Application Support/Ashfall`.
4. Linux/other Unix: absolute `$XDG_DATA_HOME/ashfall`, then `$HOME/.local/share/ashfall`.
5. If none is available: `build/saves` relative to the working directory.

Directories are created on first write, not startup. Reads check size before allocation. Writes stage in the same directory, close/check the complete file and atomically rename/replace the destination (Windows uses `MoveFileExW`); a failed stage/replace does not truncate the previous save. Temporary files are cleaned after handled failures. This is not a promise of full filesystem/power-loss durability. Use one native editor process per data directory; concurrent processes are last-writer-wins. Native artwork import/export behavior is unchanged and does not become a preferences file.

## Versioned file contracts

`run_save.*`, `editor_recovery.*` and the small shared `snapshot_codec.h` own the CPU-only formats. Do not dump native structs, pointers, `size_t`, strings or container memory to disk.

Both envelopes have a **20-byte header**: eight-byte magic, little-endian **u32 version (1)**, payload byte length and **FNV-1a-32 checksum** of the payload. Floats are explicit IEEE-754 binary32 bits; booleans are exactly 0 or 1. Lengths must consume the file exactly. The checksum detects accidental corruption, **not tampering or cheating**.

### `.ashrun`: envelope v1, current rules revision 2

- Magic **`ASHRUN\0\0`**, at most **128 KiB**. The envelope version remains 1; the payload's first u32 selects **rules revision 1 or 2**. New saves write 2. Neither the IndexedDB schema nor the `.ashgear` catalog/draft format changes.
- The shared prefix is defined in `RunCodec`: rules revision and bundled-map fingerprint; world timing/counts/enemy-ID generator/target intent; complete player state/navigation/base variants/combat; then bounded enemy records/navigation/combat. Target-goal coordinates use unsigned `coordinate + 1`, with zero representing unset. In revision 2 the three one-byte base variants use **255** for an empty worn slot; they must agree with the full instance records below.
- Revision 2 appends three u32s (**next loot ID, nonzero xorshift32 state, selected ground-loot ID**), three optional worn instances, **24 optional backpack cells**, then a bounded ground-record count and records. Each optional begins with a strict 0/1 byte. Cell positions are preserved, not compacted on load.
- A gear instance is seven little-endian u32s: **ID, slot, variant, rarity, armor roll, movement-hundredths roll, ASPD-percentage-points roll**. A ground record is u32 ID/kind, two binary32 position coordinates, u32 amount, then a gear instance only for kind Gear. Gear/flask amounts are exactly 1; ember-cache amounts are 1–45. Kinds are Gear / Embers / Flask (0 / 1 / 2).
- IDs must be nonzero, unique across **all worn items, backpack cells and ground records**, and less than the next allocator value. A ground gear record's ID equals its nested instance ID. Slots, catalog variants, rarity and rarity-specific roll ranges are validated, as are pickup target existence/liveness and mutual exclusion with selected-enemy intent. Ground positions must fit collision geometry.
- Bounds are **24 backpack cells**, **320 ground records**, at most **six starters + four allocated drops per kill**, and a nonzero next ID no larger than `7 + 4 × kills`. This leaves capacity/ID headroom for future kills; load cannot manufacture an exhausted allocator or force the next death to overwrite a drop.
- **Migration:** rules-v1 files decode their exact three equipped catalog variants into three Common instances (IDs 1–3). They receive **no starter backpack, retroactive ground rewards or additional currency/XP**. Their first future generated drop starts at ID 4, with the default dedicated loot stream; already-dead enemies do not drop again. The next explicit K/export writes v2. Old builds reject v2 instead of silently discarding its inventory. `tests/fixtures/legacy-run-v1.ashrun` is an immutable fixture from the original encoder.
- Both revisions support the **bundled sanctum/catalog only**. The encoder rejects a changed map; the decoder verifies the rules revision and deterministic grid fingerprint. A future level/catalog/rules change needs an explicit version bump or migration, not silent reinterpretation.
- Finite/ranged positions, directions, impulses, timers, counters, health, catalog enums and variants are validated. Positions must fit collision geometry; navigation is capped at **400 tile-center waypoints per actor**, with collision-safe waypoint segments and a valid cursor. Counts are capped at **64 total enemies**; unique nonzero stable IDs, next-ID/count consistency, health/death state, kills/live counts and selected living targets must agree. File size bounds total allocation/work, regardless of encoded counts.
- A fresh World is decoded/validated before replacing the live one. Pathfinder scratch memory and presentation resources are recreated/reused, never serialized. [The loot guide](loot.md) documents transfers, pickup geometry, drop tables and economic rules.

### `.ashdraft` v1

- Magic **`ASHDRAFT`**, at most **50 MiB + 128 bytes**.
- Payload: mode boolean, one-byte name length plus 1–28 ASCII filename characters, four foreground RGBA bytes, four setting booleans, then either standalone dimensions/pixels or selected gear key plus length-prefixed `.ashgear` bytes.
- Standalone dimensions are **1–128** per side; precisely `width × height × 4` raw RGBA bytes follow. Gear drafts use the existing bounded/raw-or-RLE `.ashgear` codec, including its dimensions, registration, catalog revision, unique frame keys and checksum. Project and draft names must match.
- Names allow only letters, digits, `_` and `-`, never paths. Invalid metadata, lengths, enums, duplicate frames, unsupported versions/checksums or trailing data are rejected before applying any pixels.
- Recovery uses `Document::recover` / `Workspace::recover`, which deliberately have no exported save point. Normal file Open still starts from that file's clean save point.

## Verification

`make test` and `make sanitize` run six graphics-independent suites. Persistence adds over 12,000 assertions covering deterministic codec round trips, terminal states, mid-swing one-hit/cooldown preservation, private dash direction, pursuit continuation, repeated checkpoints, dirty/history recovery semantics, scheduling/debounce bounds, native I/O/failed replacement, truncation/checksum/version/size errors and valid-checksum malformed-input mutations.

With a diagnostic WASM build, `node tests/persistence_smoke.mjs` drives real UI inputs and actual IndexedDB: manual saves, persistent reload, portable file picker/export, corruption/oversize safety, stale Load/Restart races, mid-stroke autosave, deferred recovery protection, gear-wide recovery, export/Apply distinctions, artwork invariance, late-read/new-stroke races, corrupt draft consent, quota/retry, multi-tab ownership and denied storage. It does not mutate the C++ world/document via test hooks. `tests/release_smoke.mjs` additionally verifies game saves and editor recovery with all read-only test diagnostics removed.

`ashfall_loot_tests` and `tests/loot_smoke.mjs` additionally cover exact worn/backpack/ground ownership, empty slots, full-bag safety, rarity/RNG, real combat pickups, v1 migration, malformed v2 instance graphs, full-slot saves and post-victory looting. The release smoke verifies production unequip/migration and Warden cache pickup with diagnostics absent.
