# Game saves and editor recovery

Both applications remain **C++17 + Raylib**, built natively or as filesystem-free WebAssembly. Serialization/validation is graphics-independent C++; JavaScript only moves bytes between WASM, local files and IndexedDB. Nothing is uploaded to a server.

## Save and resume a run

- **K** saves the current run to **one local slot**, replacing that slot. Saving does not pause gameplay; pause first for an exact stationary checkpoint.
- **L** loads that slot, replacing the current run **only after full validation**. The result is always paused: **P / Esc** or **RETURN TO THE SANCTUM** resumes. A missing, corrupt, oversized or incompatible save leaves the current run intact.
- **Pause**, death and victory dialogs also provide **SAVE RUN**, **LOAD RUN**, **EXPORT .ASHRUN** and **IMPORT .ASHRUN** buttons. The field guide lists the shortcuts.
- **EXPORT .ASHRUN** downloads `run.ashrun` in the browser, or writes `build/saves/run.ashrun` on desktop. Native export deliberately replaces that fixed export file, just as K replaces the quick-save slot. Keep separately named copies for multiple checkpoints.
- Browser **IMPORT .ASHRUN** opens a local file picker; dropping a `.ashrun` onto the game works too. Desktop: drop the file, or run `./build/native/ashfall --load /path/to/run.ashrun`.

A save includes health/flasks, XP/embers, equipped catalog identities, player/enemy positions and knockback, navigation and waypoint cursors, stable enemy/target IDs and pursuit intent, kills, elapsed run time, dash direction/remaining time, invulnerability, enemy windup/hit/recovery state and player swing/combo/cooldown/attack serials. A swing that already hit an enemy cannot hit it a second time just because it was loaded. Mid-dash and mid-chase saves continue instead of resetting their cooldowns or routes. Dead and cleared runs can also be saved/loaded.

Particles, damage labels, notices, camera/UI preferences and the independent directional-sprite animator clocks are not restored. The short simulation hit-stop timer is restored. The animator is resynchronized to the loaded gameplay state. **Equipment artwork is separate:** loading a run never replaces applied/imported art or reads stats from an artwork project.

K is explicit saving, **not automatic gameplay autosaving**. A fresh launch starts a fresh descent until L is pressed. Restart, death, victory and portable-file import do not overwrite the quick-save slot. A pending browser Load is invalidated by a newer Save/Load/import or Restart, so an old asynchronous result cannot replace an explicitly restarted run.

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

### `.ashrun` v1

- Magic **`ASHRUN\0\0`**, at most **128 KiB**.
- Payload order is defined in `RunCodec`: rules/catalog revision and bundled-map fingerprint; world timing/counts/ID generators/target intent; complete player state/navigation/loadout/combat; then bounded enemy records/navigation/combat. Target-goal coordinates use unsigned `coordinate + 1`, with zero representing unset.
- v1 supports the **bundled sanctum/catalog only**. The encoder rejects a changed map; the decoder verifies the rules revision and deterministic grid fingerprint. A future level/catalog/rules change needs an explicit version bump or migration, not silent reinterpretation.
- Finite/ranged positions, directions, impulses, timers, counters, health, catalog enums and variants are validated. Positions must fit collision geometry; navigation is capped at **400 tile-center waypoints per actor**, with collision-safe waypoint segments and a valid cursor. Counts are capped at **64 total enemies**; unique nonzero stable IDs, next-ID/count consistency, health/death state, kills/live counts and selected living targets must agree. File size bounds total allocation/work, regardless of encoded counts.
- A fresh World is decoded/validated before replacing the live one. Pathfinder scratch memory and presentation resources are recreated/reused, never serialized.

### `.ashdraft` v1

- Magic **`ASHDRAFT`**, at most **50 MiB + 128 bytes**.
- Payload: mode boolean, one-byte name length plus 1–28 ASCII filename characters, four foreground RGBA bytes, four setting booleans, then either standalone dimensions/pixels or selected gear key plus length-prefixed `.ashgear` bytes.
- Standalone dimensions are **1–128** per side; precisely `width × height × 4` raw RGBA bytes follow. Gear drafts use the existing bounded/raw-or-RLE `.ashgear` codec, including its dimensions, registration, catalog revision, unique frame keys and checksum. Project and draft names must match.
- Names allow only letters, digits, `_` and `-`, never paths. Invalid metadata, lengths, enums, duplicate frames, unsupported versions/checksums or trailing data are rejected before applying any pixels.
- Recovery uses `Document::recover` / `Workspace::recover`, which deliberately have no exported save point. Normal file Open still starts from that file's clean save point.

## Verification

`make test` and `make sanitize` run five graphics-independent suites. Persistence adds over 12,000 assertions covering deterministic codec round trips, terminal states, mid-swing one-hit/cooldown preservation, private dash direction, pursuit continuation, repeated checkpoints, dirty/history recovery semantics, scheduling/debounce bounds, native I/O/failed replacement, truncation/checksum/version/size errors and valid-checksum malformed-input mutations.

With a diagnostic WASM build, `node tests/persistence_smoke.mjs` drives real UI inputs and actual IndexedDB: manual saves, persistent reload, portable file picker/export, corruption/oversize safety, stale Load/Restart races, mid-stroke autosave, deferred recovery protection, gear-wide recovery, export/Apply distinctions, artwork invariance, late-read/new-stroke races, corrupt draft consent, quota/retry, multi-tab ownership and denied storage. It does not mutate the C++ world/document via test hooks. `tests/release_smoke.mjs` additionally verifies game saves and editor recovery with all read-only test diagnostics removed.
