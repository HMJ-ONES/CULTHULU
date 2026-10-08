# Character Pipeline — `assets/characters/<name>/` → UE5

How a plug-and-play character package becomes a playable UE5 character.
The package format is defined by the core (`character.def` + optional
`model.fbx`, `rig.map`/`bones.list`, `animations/`); this doc maps each
piece into UE assets. The Cthulhu Avatar package
(`assets/characters/cthulhu_avatar/`) is the worked example; wave 12
added five NPC packages (`cultist_hooded`, `cultist_magus`,
`civilian_villager`, `civilian_guard`, `civilian_laborer`) that follow
the exact same route, and `assets/characters/echo_of_the_void/` is the
new minimal example package. Creature *models* (7 species) live
separately under `assets/creatures/` and are keyed in
`ModelCatalog::creatureModels()` — they pair with
`ACultUlhuEntityActor`, not the character pipeline.

## Validate before you import

The core now grades packages before they ever reach UE
(`CharacterValidator::validateDeep()` → `CLEAN` / `OK WITH WARNINGS` /
`INVALID`). Run it headlessly first:

```
character validate <id>     # e.g. character validate cthulhu_avatar
```

Fix every `INVALID` before creating the Data Asset; warnings are
advisories (missing optional pieces degrade gracefully — see below).
The owner's Blender→FBX→package→validate loop is written up in
`docs/bring_your_own_model.md` — read that before importing a new rig.

The validator also runs the rig mapper and reports per-bone diagnostics.
The mapper knows Mixamo (`mixamorig:Hips`), Blender, and Rigify
(`upper_arm.L`, `DEF-` prefixes) bone-name aliases, so most sane rigs
map with no `rig.map` at all; the report tells you which bones matched
by alias and which didn't.

## Piece-by-piece mapping

| Package piece | UE5 destination | How |
|---|---|---|
| `character.def` (id, names, hp/speed/stamina, Q/F/R, combo id, RMB, passive) | `UCultCharacterData` Data Asset (`/Game/Characters/<Name>/DA_<Name>`) | Manual entry per the field table below, or the import script (next section). Validated field-for-field by `unreal/Tests/validate_binding.py`. |
| `model.fbx` (rigged) | `USkeletalMesh` (`/Game/Characters/<Name>/SK_<Name>`) | FBX import (see below). Skeleton = the model's skeleton. |
| `rig.map` / auto-mapped bones | UE retarget chain | The core `RigMapper` matches the FBX bone list to the 11-bone humanoid rig. In UE, the equivalent step is **IK Retargeter**: source = Mixamo/character skeleton, target = your UE mannequin-compatible skeleton. The `rig.map` is the human-readable record of that match — keep it next to the asset. |
| `animations/*.canim` | `UAnimSequence`s | Core `.canim` clips are the *fallback*. In UE, import real FBX anims (or Mixamo packs) and retarget them; bind them in the character's AnimBP. The core ships procedural fallback clips for all 9 gameplay states (Idle, Walk, Run, Attack, Death, Cast, Stunned, Channel, CastWave — see `src/animation/ProceduralClips.h`); Levitate/Launch/Levitated stay unbound, driven by Wave of Domination logic. |
| (missing model) | "Logic-only" character | Fully supported: `SkeletalMesh = None` on the data asset; the character plays with a placeholder mesh or none. The game must never crash on a missing mesh. |
| (missing clips) | Procedural fallback | Same rule: missing anims → core procedural clips, never a null montage crash. |

## `character.def` → `UCultCharacterData` field table

| `character.def` key | Data Asset field |
|---|---|
| `id` | `CharacterId` (must match; the registry key) |
| `display_name` | `DisplayName` |
| `flavor` | `Flavor` |
| `max_hp` | `MaxHp` |
| `move_speed` | `MoveSpeed` (m/s; C++ converts to cm/s) |
| `max_stamina` | `MaxStamina` |
| `[q]` / `[f]` / `[r]` sections | `QAbility` / `FAbility` / `RAbility` (`id→SpellId`, `name→Name`, `flavor→Flavor`, `cooldown→CooldownSec`, `stamina_cost→StaminaCost`, `mana_cost→ManaCost`, `effect→EffectKind`, `power→EffectPower`, `range→Range`) |
| `melee_combo` | `MeleeComboId` |
| `[rightclick]` (`kind`, `name`, `damage_mult`, `range`, `cc`, `cc_seconds`) | `RightClick` (`Kind`, `Name`, `DamageMult`, `Range`, `CcType`, `CcSeconds`) |
| `rmb_ability` | `RmbAbilityId` (e.g. `wave_of_domination` → the core state machine; empty = plain numbers) |
| `[passive]` (`id`, `desc`) | `PassiveId`, `PassiveDesc` |

// VERIFY IN EDITOR: after importing the Cthulhu Avatar, compare every
// field against `assets/characters/cthulhu_avatar/character.def` by hand
// once. The offline test checks the *table*, not your asset.

## FBX import checklist (per character)

1. Model in Blender: apply transforms, ensure the armature is the FBX root
   (the owner rigs in Blender — no re-rigging needed if bone names are sane).
2. Import `model.fbx` → creates `SK_<Name>` + skeleton. **Do not** let the
   importer create a new skeleton per animation later — reuse this one.
3. If bone names are Mixamo-style (`mixamorig:Hips`) or Rigify-style
   (`upper_arm.L`), the core `RigMapper` already knows the aliases; in UE
   you just need a consistent *target* skeleton for retargeting.
4. AnimBP: parent `UCultUlhuAnimInstance`. Build the state machine with one
   state per `ECultAnimState` (Idle/Walk/Run/Attack/Cast/Stunned/Death/
   Channel/CastWave/Levitate/Launch/**Levitated** — 12 states total;
   `Levitated` is the victim-side wave state, added in the wave-13
   review). Transitions read `AnimState`.
   // VERIFY IN EDITOR: every enum value has a state; missing states fail
   // silently at runtime (character freezes), so check twice.
5. Assign `SK_<Name>` + AnimBP on the `UCultCharacterData` asset, then the
   asset on the character Blueprint (`ACultUlhuCharacter` subclass).

## Mixamo animation retargeting (IK Retargeter)

1. Download the Mixamo pack (e.g. walk/run/idle/attack) **with the same
   character** (or "no character", then retarget).
2. Import → creates `SK_Mixamo` + source skeleton + anim sequences.
3. Create an **IK Retargeter**: source = Mixamo skeleton, target = your
   character skeleton. Use the default humanoid chain mapping; fix the
   root/pelvis by hand if the walk slides.
   // VERIFY IN EDITOR: retargeted walk/run don't foot-slide; adjust the
   // IK goals or fall back to the core procedural clips for locomotion.
4. `Export Animations` → sequences on your skeleton → plug into the AnimBP.
5. The core `AnimationStateMachine::mixamoClipName()` gives the canonical
   Mixamo clip name per state — use it to keep names consistent.

## RMB kits (Wave of Domination and future characters)

- The core state machine (`RmbAbility` subclasses, factory
  `createRmbAbility()`) runs in C++ on the host.
- UE feeds it per-frame: press/hold/release, mouse deltas, LMB-during-hold.
- Animation: `suggestedCasterState()` → `ECultAnimState` (`CastWave`,
  `Levitate` loop, `Launch`); victim actors get `SetVictimLevitated(true)`.
- New characters define their own RMB kits by adding a core `RmbAbility`
  subclass + factory id — the UE side needs no changes (it speaks the
  `RmbAbility` interface only).

## Adding a character in UE (5-minute version)

1. Drop `assets/characters/<name>/` in the repo (core auto-discovers it).
2. Import `model.fbx` (if any) → `SK_<name>`.
3. Create `DA_<name>` (`UCultCharacterData`), fill fields from
   `character.def` per the table above.
4. Create AnimBP parented on `UCultUlhuAnimInstance`, wire states.
5. Create Blueprint child of `ACultUlhuCharacter`, set `CharacterData`.
6. Done — no C++ changes, no core changes.
