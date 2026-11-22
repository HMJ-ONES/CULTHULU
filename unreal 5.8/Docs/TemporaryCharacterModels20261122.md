# Temporary character models — 2026-11-22

All 16 released characters have in-game visuals. Eight missing selections now use user-authorized free substitutes. Preferred final selections remain in `Config/ApprovedCharacterModels.json` → `preferred_character_selections`; Ghatanothoa remains Biblical Beast — Behemoth there. The rejected Overlord is not used.

## Per-character status

IMPORTED and PLAYABLE describe the tested local build. ANIMATED describes the implementation, not finished animation polish. No missing-model blockers remain. Full limb animation remains unfinished for the four static substitutes.

| Character ID | Model | Status | Animation / remaining work |
|---|---|---|---|
| mi_go | Insectoid Monster Rig | IMPORTED / PLAYABLE / ANIMATED | Existing skeletal gameplay bindings preserved; poses and camera visibility checked. |
| cthulhu_avatar | Cthulhu - Lovecraft lore | IMPORTED / PLAYABLE / ANIMATED | Existing skeletal gameplay bindings preserved; poses and camera visibility checked. |
| dagon | Dagon - Lovecraft lore | IMPORTED / PLAYABLE / ANIMATED | Existing skeletal gameplay bindings preserved; poses and camera visibility checked. |
| shoggoths | LOVECRAFTIAN HORROR | IMPORTED / PLAYABLE / ANIMATED | Existing skeletal gameplay bindings preserved; poses and camera visibility checked. |
| yog_sothoth | Yog-Sothoth - Lovecraft lore | IMPORTED / PLAYABLE / ANIMATED | Existing skeletal gameplay bindings preserved; poses and camera visibility checked. |
| yig | Naga Dragon Serpent. stylized fantasy monster | IMPORTED / PLAYABLE / ANIMATED | Existing skeletal gameplay bindings preserved; poses and camera visibility checked. |
| sghllor | Sghllor original light and mist | IMPORTED / PLAYABLE / ANIMATED | Original animated light and mist; no creature skeleton. |
| bokrug | Four-Legged-Frost-Predator. | IMPORTED / PLAYABLE / ANIMATED | Existing skeletal gameplay bindings preserved; poses and camera visibility checked. |
| nyarlathotep | Lovecraftian (temporary) | IMPORTED / PLAYABLE / ANIMATED | Existing skeletal gameplay bindings preserved; poses and camera visibility checked. |
| nodens | Chariot (temporary) | IMPORTED / PLAYABLE / ANIMATED (procedural body only) | Procedural chariot motion only; no creature skeleton. Horse omitted. |
| ghatanothoa | Troll (temporary) | IMPORTED / PLAYABLE / ANIMATED | Supplied idle/walk/run/attack/cast/hit/death. Corrected morph units and calibrated posed bounds. |
| hastur | Low Poly Wizard (Rigged) (temporary) | IMPORTED / PLAYABLE / ANIMATED (procedural body only) | Procedural body motion only. Original GLB has rigid node animation; Unreal geometry has no skin. |
| mother_hydra | Yamata no Orochi (temporary) | IMPORTED / PLAYABLE / ANIMATED (procedural body only) | Procedural body motion only. Static sculpture needs an armature and skin weights. |
| rhan_tegoth | Crabulb - Rigged (temporary) | IMPORTED / PLAYABLE / ANIMATED | Supplied idle/walk plus procedural claw combat. Run reuses walk; no dedicated death clip. |
| shub_niggurath | Tree Monster (temporary) | IMPORTED / PLAYABLE / ANIMATED (procedural body only) | Procedural body motion only. Static tree needs an armature and skin weights. |
| nug_and_yeb | Simple Moon Jellyfish (Baked Animation) (temporary) | IMPORTED / PLAYABLE / ANIMATED | One licensed jellyfish duplicated into two bodies. Supplied pulse loop plus independent diagonal drifting; no separate attack clip. |

## Open in Blender

1. Edit a copy. Keep current imported Unreal assets intact.
2. Blender: File → Import → glTF 2.0, then choose a GLB below. For the troll use the prepared file, which fixes the supplied morph units.
3. Save your working `.blend` in a new folder under the repository’s `assets/characters/` directory.
4. Rig and weight Hastur, Mother Hydra, Shub-Niggurath and Nodens before creating limb animations.
5. Export improved geometry and animations to a new versioned file, preserving textures, UVs and skeleton compatibility.
6. Provide the exported path for reimport and validation. Do not overwrite `.uasset` files.

Paths below are relative to `unreal 5.8/`:

| Character ID | Blender source |
|---|---|
| ghatanothoa | `Saved/CharacterSources/ghatanothoa/Temporary20261122/prepared/source.glb` |
| hastur | `Saved/CharacterSources/hastur/Temporary20261122/source.glb` |
| mother_hydra | `Saved/CharacterSources/mother_hydra/Temporary20261122/source.glb` |
| rhan_tegoth | `Saved/CharacterSources/rhan_tegoth/Temporary20261122/source.glb` |
| shub_niggurath | `Saved/CharacterSources/shub_niggurath/Temporary20261122/source.glb` |
| nug_and_yeb | `Saved/CharacterSources/nug_and_yeb/Temporary20261122/source.glb` |
| nyarlathotep | `Saved/CharacterSources/nyarlathotep/source.glb` |
| nodens | `Saved/CharacterSources/nodens/source.glb` |

The six new untouched GLBs are also archived as `.glb.b64` under `assets/characters/temporary_20261122/`. Source URLs, licensing records and SHA-256 hashes are in `sources.json`. `assets/decode_assets.py` restores them after cloning. Preserve credits when distributing modified art. These source files are outside the deleted Blender folder.

## Validation performed

- Canonical validator and actual CultUlhuEditor Win64 Development build passed.
- `CultUlhu.Art.ApprovedCharacterModels` passed: all 16 previews; pawn installation in all three modes; canonical height, blocking capsules, first/third-person body visibility and corpse geometry. Troll and crab posed vertices fit gameplay height.
- Visually inspected all six new imports, including troll attack/run and crab walk. Verified separate floating twin bodies and independent drifting.
- Local two-process host/client probes passed in Free Roam, Domination and DOTGOO, with Nug & Yeb hosting and Mi-Go joining. Remote imported visual, movement, combat, local leaving and bot replacement passed. DOTGOO creep movement and ward-warning replication passed. This does not claim an internet/Radmin or Cloudflare test.
- Refreshed eight temporary selection portraits. Cook paths include the new art, materials and portraits.
- Inspected the desktop Cult-Ulhu shortcut: it targets this exact canonical project with `-game` and uses these updates.

Evidence: `Saved/Logs/TemporaryModelsFinal.log`, `Saved/Screenshots/ApprovedCharacters/`, and `Saved/Automation/Room{0,1,2}_*.json`. Build output and test saves are excluded from GitHub.

Licenses and artist credits: [CREDITS.md](../Content/CharacterModels/CREDITS.md).
