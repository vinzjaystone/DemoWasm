# Portable run fixtures

These are small `.ashrun` **test inputs**, not shipped game assets or required startup data. Browser tests import them using the same visible file picker as a player. All application diagnostics remain read-only.

- **`legacy-run-v1.ashrun`** (901 bytes): captured with the original rules-v1 encoder at `a1d9741` before inventory implementation. Default sanctum, one dead Hollow (not yet erased), 47.25 seconds, 67 health, one flask, 140 XP, 30 embers, crown/mantle/fleetstriders. Migration must preserve those values, create only the three worn Common instances, grant no starter bag and never replay old drops. Keep this immutable: regenerating it with the new encoder would stop testing compatibility.
- **`full-backpack-v2.ashrun`** (5,817 bytes): synthetic CPU fixture. Default sanctum/eight living encounters plus 25 defeated Wardens, full backpack, 1,125 collected embers and unclaimed gear/flasks. Exercises capacity-neutral swaps, non-destructive failed pickups/unequips, rarity stats and empty-slot persistence without making a browser grind dozens of encounters.
- **`victory-loot-v2.ashrun`** (625 bytes): synthetic single-Warden clear with four unclaimed drops. Exercises the victory dialog's **COLLECT THE SPOILS** action and guaranteed Relic, including with production diagnostics disabled.

The two v2 fixtures use deliberately low-health CPU encounter setup to create bounded test states; they are not presented as recordings of ordinary play. `tests/core_tests.cpp` independently finishes the actual authored arena without teleports/forced damage, and the full browser loot suite kills a real Hollow using normal click-to-target combat. No such setup API is present in the game.

The CPU suite checks that each committed v2 fixture decodes and re-encodes byte-for-byte, and compares its ownership, economy and drop records with the generated state. Cross-platform generation allows a tiny position tolerance for libm's final `sin`/`cos` rounding bit; persisted positions themselves still round-trip exactly.

Regenerate the v2 fixtures after an intentional format/rules change:

```sh
cmake -S . -B build/tests -DASHFALL_BUILD_GAME=OFF
cmake --build build/tests --parallel
build/tests/ashfall_loot_tests --write-fixture tests/fixtures/full-backpack-v2.ashrun
build/tests/ashfall_loot_tests --write-victory-fixture tests/fixtures/victory-loot-v2.ashrun
ctest --test-dir build/tests --output-on-failure
```

SHA-256 at introduction:

```text
13ca84ac751e5adc796247670e8567ba9333464fcc889ba25a2478ea1b4833ea  legacy-run-v1.ashrun
e4f3b55ae52d435fa7cd9c86a0526fb583b11427dc0db36d62617a43e18f5c17  full-backpack-v2.ashrun
52ec3a58dc552d2ac069e4eaa31f0bedfdd0467c3a44954f5b405e7b5a01dbb6  victory-loot-v2.ashrun
```
