# Bring Your Own Model

How to take a rigged Blender model and get it playable in CULT-ULHU.
You know modeling and rigging — this covers the rest: the export
settings that matter, the package folder layout, and how to check your
work with the built-in validator. No animation experience required:
missing animations fall back to procedural motion automatically.

## 1. Export from Blender (FBX)

1. Select **both** the mesh and the armature.
2. `File → Export → FBX`. In the export panel:
   - **Apply Transform** — on (bakes scale/rotation so the model lands
     upright at the right size).
   - **Armature**: export **Only Deform Bones** (uncheck "Add Leaf
     Bones" — they confuse auto-mapping).
   - **Forward: -Z**, **Up: Y** (Blender's FBX default — matches the
     engine's expectation).
   - Scale: **1.0**. If your model was built in meters, it arrives in
     meters; `move_speed` in `character.def` is meters/second.
3. Save the FBX as `model.fbx`.

GLB works too (`model.glb`), but FBX carries the armature more
reliably for the future animation import. Either is fine for now.

## 2. The package folder

Create `assets/characters/<your_name>/` with:

```
assets/characters/my_horror/
    character.def   ← required: stats + abilities (see below)
    model.fbx       ← your export (or model.glb)
    rig.map         ← optional: explicit bone mapping (see §3)
    bones.list      ← optional: one bone name per line, lets the
                      auto-mapper run before the model is even wired
                      into the engine (handy for checking names early)
    animations/     ← optional: .canim clips (skip this for now —
                      procedural fallbacks cover everything)
```

The smallest playable character is a `character.def` and nothing else
(see `assets/characters/echo_of_the_void/` — logic-only, fully
playable). Add pieces when you have them; the game never crashes on a
missing piece.

### `character.def` — the fields that matter

Copy `assets/characters/echo_of_the_void/character.def` and edit it.
The essentials:

- `id` — must match the folder name.
- `max_hp`, `move_speed`, `max_stamina` — the vitals.
- `[q]`, `[f]`, `[r]` — your three abilities. `effect` picks the
  behavior: `aoe_damage`, `fear_aura`, `summon`, `buff`, `projectile`,
  `heal`. `cooldown` is seconds, costs are stamina.
- `melee_combo` — name of the melee chain (any string; leave the
  default unless you add a custom combo).
- `[rightclick]` — `kind` is one of `MeleeHeavy`, `MindControl`,
  `AcidSpit`, `EldritchGrasp`; `cc` is `Stun`/`Slow`/`Root`/`Fear`.

## 3. Bone mapping (`rig.map`)

The engine drives an 11-bone humanoid rig:

```
hips, spine, head,
upperArmL, upperArmR, lowerArmL, lowerArmR,
upperLegL, upperLegR, lowerLegL, lowerLegR
```

You usually **don't need** a `rig.map`: the auto-mapper already knows
Mixamo (`mixamorig:LeftForeArm`), Rigify (`forearm.L`, `DEF-thigh.L`),
and plain Blender names (`Forearm.L`, `Clavicle_L`). It matches
case-insensitively and reports exactly what it did (see §4).

Write a `rig.map` only when the auto-mapper gets something wrong.
Format — one `engine_bone = your_bone` per line, `#` comments allowed:

```
# rig.map — explicit overrides (auto-mapper handles the rest)
upperArmL = Clavicle_L
lowerArmL = Forearm_L
```

Lines naming bones the engine doesn't know are flagged, not fatal.

## 4. Validate: `character validate <id>`

In the driver (`./build/cultulhu_play`):

```
character validate my_horror
```

You get a **grade** plus the full story:

- `CLEAN` — every piece present and sane.
- `OK WITH WARNINGS` — playable; each warning names its **fallback**
  (e.g. "no 'cast' clip (procedural fallback)").
- `INVALID` — something blocks loading (bad `character.def` value,
  unreadable model file); fix the listed errors.

The `info:` lines show the per-bone mapping detail (`upperArmL <-
'Clavicle_L' (alias, 90%)`), so you can see exactly which bones the
auto-mapper claimed and which fell back. If a bone shows `(unmapped)`,
procedural animations still work — add a `rig.map` line only if the
motion looks wrong later in UE5.

Quick iteration loop while modeling:

```
addchar assets/characters/my_horror   # hot-load without restarting
character validate my_horror          # check the grade
```

## 5. What happens when pieces are missing (the fallback chain)

| Missing piece | What the game does |
|---|---|
| `model.fbx` / `model.glb` | Logic-only: abilities, AI, and stats all work; no mesh |
| `rig.map` | Auto-mapper runs on the FBX bone list (or `bones.list`) |
| `animations/` | Procedural clips for idle, walk, run, attack, death, cast, stunned, channel, and RMB gestures |
| unknown `effect` / `rmb_ability` | Warning; falls back to default numbers |

Nothing in this table crashes. The validator's job is to tell you
which row you're on, so there are no surprises.

## 6. Testing in the driver

```
chars                          # list registered characters
character validate my_horror   # deep report
spawn cultist 3                # the world still works around you
```

When the model looks right in-engine later, the UE5 side is covered in
`unreal/Docs/CharacterPipeline.md` (FBX import checklist, retargeting).
