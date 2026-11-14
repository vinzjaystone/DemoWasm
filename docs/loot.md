# Backpack, equipment drops and loot

The gameplay system is graphics-independent **C++17** (`equipment.*`, `loot.*`, `world_loot.cpp`). The native and WASM applications use the same simulation and inventory operations. No backend, external art, filesystem emulation, JavaScript game logic or new runtime dependency is required.

## Playing

| Input / control | Result |
| --- | --- |
| **B** / **BACKPACK** header tab | Open/close the 24-cell backpack; movement, combat and animation pause while it is open |
| Click a backpack or worn item | Inspect catalog identity, rarity and actual armor / movement / ASPD; a backpack item is compared with its currently worn slot |
| **EQUIP / SWAP** | Equip the selected owned item; move the previous item into that same backpack cell |
| **UNEQUIP TO BACKPACK** | Empty the selected worn slot, if a free cell exists |
| **DROP** | Put a backpack item at your feet, retaining its exact ID and rolls; reclaim it normally |
| **SALVAGE +n**, then **CONFIRM +n** | Permanently consume a backpack item for embers; changing selection, sorting or closing cancels consent |
| **SORT** | Compact/sort by slot, rarity descending, base variant and stable ID; selection follows the item, not its old cell |
| **CRAFT FLASK** | Spend **30 embers** to refill one flask, up to three carried flasks |
| **1 / 2 / 3** / character equipment cards | Rotate only **owned spares** for Head / Armor / Boots; never create or unlock an unowned item |
| Click a ground marker | Collect immediately if reachable; otherwise use A* to walk close enough and collect |
| **E** / **LOOT [E]** | Collect reachable nearby drops; full gear/flasks do not block an ember cache |
| Hold **Alt** | Show all ground-drop names |
| **Esc** | Close the backpack before toggling the ordinary pause screen |

The player starts in the original hood, Ashguard plate and wraps, plus **three named common spares**: an iron sallet, crimson mantle and iron greaves. This is six owned items, **not** an unlocked copy of the nine-piece catalog. Other base pieces must drop. Each piece consumes one cell; duplicates are distinct instances rather than an automatically merged stack. Flasks (maximum three) and embers have their own counters and do not consume gear cells.

No pickup automatically equips or salvages anything. A full backpack leaves gear on the ground with a notice. Swapping still works at full capacity because the displaced item takes the incoming item's cell; unequipping requires an extra cell and fails intact if there isn't one. Worn items cannot be directly dropped or salvaged: unequip first. Dead players cannot transfer or consume loot.

Ground pickups require distance **≤1.1 tiles** and an unobstructed, collision-checked segment. Items cannot be collected through walls or blocked corners. A ground-movement command, selected monster, manual cleave or successful dash cancels click-to-loot intent. Repeated failures do not leave an endless pickup/repath loop. Living monsters have picking priority over nearby ground markers.

Drops **do not expire**, including when their corpse is erased. Winning offers **COLLECT THE SPOILS** before restarting, so the Warden's final drops are not trapped behind the victory dialog. Looting/movement/backpack remain available after acknowledging victory; the recorded clear time stops at the last kill. **R** / **DESCEND AGAIN** explicitly abandons the run and its unsaved items.

## Drop tables and progression

XP remains immediate on death. Gear, embers and flask refills are physical ground drops:

| Enemy | XP | Guaranteed drops | Additional drop |
| --- | ---: | --- | --- |
| Hollow | 20 | One gear instance, one **15-ember** cache | **25%** chance of one flask |
| Ash Warden | 50 | One Tempered-or-better item, **one guaranteed Relic**, one **45-ember** cache, one flask | — |

Hollow gear is **72% Common / 25% Tempered / 3% Relic**. Each item independently chooses one of the three gear slots and one of that slot's three base variants. The Warden's first item is Tempered, upgraded to Relic on the same 3% roll; its second is always Relic.

| Rarity | Extra armor | Extra movement (tiles/sec) | Extra ASPD |
| --- | ---: | ---: | ---: |
| Common | 0 | 0 | 0 |
| Tempered | +1–2 | +0.05–0.15 | +1–3 percentage points |
| Relic | +3–5 | +0.15–0.30 | +3–6 percentage points |

Rolls add to the unchanged base catalog stats. Movement/ASPD rolls are stored as integers (hundredths / percentage points), not accumulated floats. Armor mitigation and movement use equipped instance totals; attack speed remains clamped to **0.5–2.0×**, with interval `2.5 / multiplier`. Changing gear never resets an already-started attack cooldown or serial. Empty gear slots contribute no stats and omit that sprite layer, including in the procedural fallback.

Salvage pays **5 + 10 × rarity index + base catalog armor** embers (rarity indices 0 / 1 / 2). It does not reroll or award XP. Crafting is an explicit currency sink, not a free refill; no funds are spent if flasks are full or currency is insufficient. Currency/XP counters are bounded at 10,000,000. Currency pickups/salvage leave the item intact if adding its reward would exceed that limit.

The sanctum uses a fixed nonzero loot seed and a dedicated xorshift32 stream. Rewards depend on the sequence of kills/rolls, not wall time or cosmetic particles. The stream and already-generated rolls are saved, so saving/loading cannot reroll a drop. Repeating the same run/kill order is intentionally reproducible. This is not an anti-cheat or cryptographic random system.

## Ownership, artwork and limits

- Every gear instance has a nonzero stable ID, base slot/variant, rarity and three bounded roll integers. Exactly one of **worn / backpack / ground** owns it at a time. Transfers preserve the whole instance. Ember/flask ground markers also have unique IDs from the same monotonic allocator.
- The inventory holds values/optionals, never pointers into enemy or item vectors. UI selection uses IDs, so swaps, sort, vector erasure and save/load cannot turn an old cell index into a different item transaction.
- Enemy deaths are terminal and their drops are created once. The simulation accepts at most **64 total enemies** per run; **320 ground-record capacity** leaves space for all four-drop Wardens plus six starter items. No legitimate kill has to overwrite an existing drop to make room. Drop count/allocator bounds are checked when loading.
- Rarity changes gameplay instance bonuses and UI borders, **not** the base tint, artwork key or portable `.ashgear` catalog. Two instances of the same base piece use the same edited frames. Art Apply/import/restore never changes item identity, rarity, rolls, ownership or cooldowns.
- Backpack capacity, rewards and economy are deliberately small single-arena rules. There are no merchants, gold/selling UI, weapons/affix-name database, item durability, quest items, network authority or infinite respawns in this PR. Those are not implied by the loot system.

## Saving and compatibility

Use **K/L** and `.ashrun` files as before. There is **no automatic gameplay save**: backpack operations do not silently overwrite the slot. Save before abandoning/reloading a run.

Run **rules revision 2** saves all worn instances, exact backpack cells, empty slots, ground records/positions, the next loot ID, RNG state and click-to-loot intent, in addition to the complete combat/navigation state from revision 1. Files still use the existing 20-byte version-1 envelope and **128 KiB** limit. Original **rules-v1** saves load with their exact three catalog pieces as Common instances, no new spares, no retroactive drops and their existing XP/embers unchanged. The next explicit save writes revision 2. Older game builds reject revision 2 rather than silently losing gear. Artwork/draft formats and IndexedDB database schemas are unchanged. See [persistence](persistence.md) for the complete storage contract.

## Verification

`ashfall_loot_tests` adds over **290,000 assertions**: bounded transfers/full-bag swaps, empty slots, exact rarity limits and tables, one-location ownership, stale IDs, cooldown invariance, economy limits, wall/route cancellation, all 64 Wardens' drops, deterministic checkpoint continuation, authentic v1 migration, hostile v2 fields/duplicates and 2,000 mixed transactions with repeated round trips. Existing core tests still finish the authored eight-enemy arena through normal movement/combat commands, then collect its caches after victory.

`tests/loot_smoke.mjs` drives real mouse/keyboard input and the actual WASM applications: a real Hollow encounter, ground/currency pickup, no click-through, live/empty gear layers, confirmed salvage, full-bag failure/swap behavior, rolled stats, sorting, crafting, exact IndexedDB/export/reload, v1 migration and victory spoils. Small synthetic portable-run fixtures exercise capacity and terminal states through the normal file picker, never through a game-mutation test hook. `LOOT_INVENTORY_ONLY=1` skips the timed encounter for the sprites-disabled regression. The production release smoke also checks unequipping, migration and victory pickups with **all read-only test APIs absent**.
