# CULT-ULHU creature assets — MANIFEST (wave 12)

Every file below is CC0. Binaries are base64-packed (`*.glb.b64`) because the
GitHub push path corrupts raw binaries — run `python3 assets/decode_assets.py`
once after cloning to restore the `.glb` files. Animation clips were stripped
from rigged KayKit models to hold the light budget (skeletons kept; our
procedural animation fallback drives them).

## Models

| file | source | license | tris | rigged | bestiary role |
|---|---|---|---|---|---|
| pale_wight.glb | KayKit Skeletons "Skeleton_Mage" | CC0 1.0 | 4,588 | yes (clips stripped) | Pale Wight — robed skeletal oracle, cult horror |
| ossified_brute.glb | KayKit Skeletons "Skeleton_Warrior" | CC0 1.0 | 5,934 | yes (clips stripped) | Ossified Brute — heavy bone horror |
| charnel_imp.glb | KayKit Skeletons "Skeleton_Minion" | CC0 1.0 | 5,288 | yes (clips stripped) | Charnel Imp — swarm chaff |
| skittering_ghoul.glb | KayKit Skeletons "Skeleton_Rogue" | CC0 1.0 | 5,278 | yes (clips stripped) | Skittering Ghoul — fast skirmisher |
| wraith.glb | Kenney Graveyard Kit "character-ghost" | CC0 1.0 | 413 | no (static) | Wraith — lesser servitor |
| risen_dead.glb | Kenney Graveyard Kit "character-zombie" | CC0 1.0 | 1,078 | no (static) | Risen Dead — reanimated corpse |
| dagon_spawn.glb | Kenney Mini Dungeon "character-orc" | CC0 1.0 | 374 | skin data present | Dagon Spawn — deep-one hybrid brute |

## License statements (quoted from source sites)

KayKit free packs (verified 2026-10-07 on kaylousberg.itch.io/kaykit-skeletons):
"Free for personal and commercial use, no attribution required. (CC0 Licensed)"

Kenney packs (verified 2026-10-07 on kenney.nl/assets/graveyard-kit and
kenney.nl/assets/mini-dungeon):
"License | Creative Commons CC0"

## Processing notes

KayKit models:
- Source: https://github.com/KayKit-Game-Assets/KayKit-Character-Pack-Skeletons-1.0
  and https://github.com/KayKit-Game-Assets/KayKit-Character-Pack-Adventures-1.0
- `animations` arrays stripped from GLB JSON (76–95 clips each — the engine's
  procedural animation fallback covers them).
- Dead BIN data pruned (unreferenced accessors/bufferViews from the stripped
  clips): ~4.8MB → ~270–373KB per model.

Kenney models (wraith, risen_dead):
- Source: https://kenney.nl/assets/graveyard-kit (`Models/GLB format/`)
- Used as-is (already tiny: 114KB / 245KB). Static meshes, no skeleton;
  procedural animation fallback applies.

Naming is honest: the KayKit models are undead horrors re-themed for the
bestiary, the Kenney pair are graveyard spooks, and the orc is a brute
re-themed as a deep-one hybrid — none are true Mythos entities.
Genuinely Lovecraftian CC0 models (tentacled things, true deep ones,
shoggoths, winged terrors) were not found in any CC0 source surveyed
(Kenney, KayKit, OpenGameArt, itch.io) — see the wave-12 report for the
full survey and gaps. All in-game names follow LEGAL_NAMES.md
(Lovecraft-original public-domain names or plain generic English only).
