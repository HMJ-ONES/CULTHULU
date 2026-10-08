# CULT-ULHU World Assets — License Record (Wave 11, Part A)

All 3D models and palette textures under `assets/world/` are from **Kenney**
(https://kenney.nl), released under **Creative Commons Zero v1.0 Universal
(CC0 1.0)** — public domain. No attribution is legally required; credit is
given below as a courtesy.

## Per-source CC0 statements

### 1. Graveyard Kit (v5.0)
- Pack page: https://kenney.nl/assets/graveyard-kit — License row on the page
  reads "Creative Commons CC0" (verified 2026-10-07).
- The pack's own `License.txt` (shipped by Kenney) states:
  "License: (Creative Commons Zero, CC0) —
  http://creativecommons.org/publicdomain/zero/1.0/ ... You can use this
  content for personal, educational, and commercial purposes. Support by
  crediting 'Kenney' or 'www.kenney.nl' (this is not a requirement)."
- Models used: rocks, crypt, crypt-large, stone-wall, stone-wall-damaged,
  stone-wall-column, gravestone-cross, gravestone-broken, gravestone-round,
  gravestone-wide, cross, pine-crooked, pine-fall, debris,
  gravestone-debris, fence-damaged, iron-fence, altar-stone, altar-wood,
  fire-basket, pillar-obelisk, pillar-large, candle-multiple.

### 2. Castle Kit (v2.0)
- Pack page: https://kenney.nl/assets/castle-kit — License row on the page
  reads "Creative Commons CC0" (verified 2026-10-07).
- Same CC0 1.0 terms as above per the pack's `License.txt`.
- Models used: ground, ground-hills, wall, wall-corner, wall-doorway, gate,
  tower-square-base, tower-square-mid, tower-square-top, stairs-stone.

### 3. Mini Dungeon (v2.0)
- Pack page: https://kenney.nl/assets/mini-dungeon (license row did not render
  in the page text fetch on 2026-10-07; verified instead via the pack's own
  `License.txt`, shipped by Kenney, which states "License: (Creative Commons
  Zero, CC0) — http://creativecommons.org/publicdomain/zero/1.0/").
- Same CC0 1.0 terms as above.
- Models used: dirt, floor, barrel, banner, wood-structure.

## What CC0 means here
You may use, modify, and distribute these assets (including in commercial
products) with no permission, payment, or attribution required. The only
hard rule is the one we already follow: nothing here is redistributed as a
standalone asset pack.

## Courtesy credit (not required)
3D assets by Kenney — https://kenney.nl — CC0 1.0.
3D assets by KayKit (Kay Lousberg) — https://kaykitgameassets.itch.io — CC0 1.0.

### 4. Nature Kit (v1.0, wave 17)
- Pack page: https://kenney.nl/assets/nature-kit — License row on the page
  reads "Creative Commons CC0" (verified 2026-10-08).
- The pack's own `License.txt` states: "License: (Creative Commons Zero,
  CC0) — http://creativecommons.org/publicdomain/zero/1.0/ ..."
- Models used: rock_largeA, rock_largeC, rock_tallA, rock_tallC,
  cliff_cave_rock, tree_oak_dark, tree_cone_dark, tree_tall_dark
  (all untextured; no palette needed).

### 5. KayKit Dungeon Remastered (v1.1, wave 17)
- Pack page: https://kaylousberg.itch.io/kaykit-dungeon-remastered —
  "Free for personal and commercial use, no attribution required.
  (CC0 Licensed)" (verified 2026-10-08). GitHub mirror README confirms
  "Licensed under CC0 1.0 Universal".
- Models used: pillar, torch, torch_mounted, chest, banner_shield_blue,
  floor_tile_big_spikes. Embedded 1024x1024 atlas extracted and
  downsampled to 512x512 (`textures/kaykit-dungeon-atlas.png`).
- Models used (wave 19): stairs_wide, rubble_large,
  floor_tile_small_broken_A, wall_arched, wall_broken — same pack, same
  CC0 1.0 terms; `.gltf.glb` files used directly, image URI rewritten to
  `../textures/kaykit-dungeon-atlas.png`.

### 6. KayKit Halloween Bits (v1.0, wave 17)
- Pack page: https://kaylousberg.itch.io/halloween-bits —
  "Free for personal and commercial use, no attribution required.
  (CC0 Licensed)" (verified 2026-10-08).
- Models used: grave_A, grave_B, tree_dead_small, lantern_standing,
  candle_triple (.gltf+.bin packed to .glb; external 1024x1024 atlas
  downsampled to 512x512 as `textures/kaykit-halloween-atlas.png`).
- Models used (wave 19): arch, arch_gate, bone_A, ribcage, skull,
  skull_candle, coffin, tree_dead_large, fence_broken — same pack, same
  CC0 1.0 terms; `.gltf`+`.bin` packed to `.glb`, image URI rewritten to
  `../textures/kaykit-halloween-atlas.png`. (coffin_decorated was
  evaluated and rejected: 2064 tris, over the 2000-tri hard cap.)

### 7. KayKit Character Animations 1.1 (wave 18)
- Pack page: https://kaylousberg.itch.io/kaykit-character-animations —
  "Free for personal and commercial use, no attribution required.
  (CC0 Licensed)" (verified 2026-10-08). The downloaded zip's own
  `License.txt` states: "License: (Creative Commons Zero, CC0)
  http://creativecommons.org/publicdomain/zero/1.0/".
- License: **CC0 1.0 Universal**
  (https://creativecommons.org/publicdomain/zero/1.0/), by Kay Lousberg.
- Used as: 16 clips per humanoid package converted to `.canim`
  (`assets/characters/<civilian_guard|civilian_villager|civilian_laborer|
  cultist_hooded|cultist_magus>/animations/`): idle, walk, run, attack,
  death, stunned, cast, channel, cheer, fearrun, brawl, dodge, interact,
  work, sacrifice_performer, sacrifice_victim. Full provenance in
  `assets/characters/ANIMATIONS.md`.

## Sources deliberately NOT used
- **Quaternius** (quaternius.com): evaluated, rejected — its current license is
  the proprietary "Quaternius Asset License (QAL) v1.0" (last updated
  2026-08-28), not CC0. Its older CC0-era assets were not used to avoid any
  license ambiguity.

### 8. In-house procedural props (wave 19, original work)
- `props/mist-bank.glb`, `terrain/scorched-patch.glb`,
  `props/ember-cluster.glb`, `props/beams-collapsed.glb`,
  `props/dead-bush.glb` and their textures (`textures/mist-soft.png`,
  `textures/scorch-dark.png`, `textures/ember-glow.png`,
  `textures/wood-dark.png`, `textures/bark-dead.png`) were generated in
  code for this project — no third-party source, no license to verify.
- Original work by the CULT-ULHU project; treat as project-owned assets.
