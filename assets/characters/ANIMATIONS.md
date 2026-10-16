# NPC/Creature Animation Clips (wave 18)

Real authored animation clips for the 5 humanoid packages
(`civilian_guard`, `civilian_villager`, `civilian_laborer`, `cultist_hooded`,
`cultist_magus`), converted from the KayKit character-animation library.

## Source

- **Pack:** KayKit — Character Animations **1.1** (free tier, "Free 1.1" upload)
- **Author:** Kay Lousberg — https://kaylousberg.itch.io/kaykit-character-animations
- **Downloaded:** 2026-10-08. Re-fetch from the itch.io page (free download,
  no account needed): pick the **Free 1.1** file (14 MB), unzip, and use
  `Animations/gltf/Rig_Medium/*.glb`.
- **License: CC0 1.0 Universal** — verified two ways:
  1. The itch.io page states "Free for personal and commercial use, no
     attribution required. (CC0 Licensed)".
  2. The downloaded zip's `License.txt` reads
     "License: (Creative Commons Zero, CC0)
     http://creativecommons.org/publicdomain/zero/1.0/".
  A copy of that notice is kept in `assets/world/LICENSES.md` (world-art
  section; animation entry may be appended there).

## Compatibility (measured, not assumed)

All 5 packages share one identical 41-joint KayKit rig. The animation pack's
`Rig_Medium` rig has 23 joints:

- **23/23 (100%) of the pack rig's joint names exist in our skeletons**,
  with identical parent→child hierarchy among those joints.
- Rest-pose translations match to 0.0000 (identical proportions).
- The 18 joints in our skeletons that the pack doesn't animate are all
  Blender IK/control bones (`kneeIK.*`, `handIK.*`, `elbowIK.*`,
  `control-*-roll.*`, `heelIK.*`, `IK-foot.*`, `IK-toe.*`); no animation
  channel targets them.
- Every animation channel target in all 8 `Rig_Medium` GLBs resolves to a
  joint present in our skeletons (0 missing targets).

Retargeting is therefore by exact bone name — no remapping needed.

## What was integrated

16 clips per package as `.canim` (the format `CharacterPackageLoader::listCanim`
discovers in `<pkg>/animations/` and `ClipSerializer::load` parses). CLIP names
match `animationStateName()` so the state machine can bind by name. The same
16 files are used for all 5 packages because the skeletons are identical.

| file              | CLIP name          | KayKit source clip                  | loop | dur   |
|-------------------|--------------------|-------------------------------------|------|-------|
| `idle.canim`      | Idle               | Idle_A                              | yes  | 1.07s |
| `walk.canim`      | Walk               | Walking_A                           | yes  | 1.07s |
| `run.canim`       | Run                | Running_A                           | yes  | 0.80s |
| `attack.canim`    | Attack             | Melee_1H_Attack_Slice_Horizontal    | no   | 1.37s |
| `death.canim`     | Death              | Death_A                             | no   | 0.80s |
| `stunned.canim`   | Stunned            | Hit_A                               | no   | 0.67s |
| `cast.canim`      | Cast               | Ranged_Magic_Shoot                  | no   | 0.93s |
| `channel.canim`   | Channel            | Ranged_Magic_Spellcasting           | yes  | 0.67s |
| `cheer.canim`     | Cheer              | Cheering                            | yes  | 1.67s |
| `fearrun.canim`   | FearRun            | Running_B                           | yes  | 0.80s |
| `brawl.canim`     | Brawl              | Melee_Unarmed_Attack_Punch_A        | no   | 1.17s |
| `dodge.canim`     | Dodge              | Dodge_Forward                       | no   | 0.40s |
| `interact.canim`  | Interact           | Interact                            | no   | 1.30s |
| `work.canim`      | Work               | Chopping                            | yes  | 1.33s |
| `sacrifice_performer.canim` | SacrificePerformer | Ranged_Magic_Summon          | yes  | 4.30s |
| `sacrifice_victim.canim`    | SacrificeVictim    | Crouching                    | yes  | 1.07s |

Payload: ~693 KB per package, **~3.4 MB total** (80 files). All loop-flagged
clips were checked to wrap cleanly (first key == last key).

This satisfies every clip the package validator looks for
(`idle walk run attack death stunned` → reported as "anim: custom clip"
instead of "procedural fallback" warnings), plus the wave-18 states
`FearRun`, `Brawl`, `SacrificePerformer`, `SacrificeVictim`.

## Conversion method

`kaykit_glb_to_canim.py` (in this directory) converts one GLB clip → `.canim`:

1. Reads glTF animation channels; resamples translation+rotation onto the
   union keyframe timeline per bone (slerp for quaternions).
2. Positions → offsets from the node's rest translation (`.canim` stores
   rest-pose offsets per `BoneTrack.h`).
3. Quaternions → XYZ euler **degrees** (same convention `BoneTrack.h`
   documents); every keyframe's euler→matrix reconstruction is verified
   (worst error 4.5e-15).
4. Scale channels are ignored (none of the chosen clips animate scale).
5. Writes `CLIP "name" dur loop` / `TRACK bone nkeys` / `KEY t px py pz
   rx ry rz`, bone-sorted, ascending time.

Verification: all 80 files load with the repo's real `ClipSerializer::load`
(0 failures), sample 23 pose bones at t=0/mid/end, and pass the validator's
`clipStem`/`stemMatches` check for all needed clips.

## Regenerating / adding clips

```bash
# 1. Unzip the free pack somewhere, e.g. /tmp/kk/ KayKit_Character_Animations_1.1/
# 2. Convert (edit PLAN in a small driver, or call convert_clip + write_canim):
python3 assets/characters/kaykit_glb_to_canim.py   # self-test only
# 3. Copy the new .canim files into each package's animations/ dir.
```

## Deliberately not included

- `Rig_Large` clips (our models are all Rig_Medium).
- `Rig_Medium_Special` "Skeletons_*" clips (skeleton-specific; our 5 are flesh).
- `Rig_Medium_Tools` beyond `Chopping` (fishing/lockpicking/sawing have no
  gameplay hook yet; re-run the converter if one appears).
- `T-Pose` clips (bind-pose reference only).
- The pack's FBX copies and mannequin characters (duplicates of the GLBs).
- Mixamo was not used (license).
