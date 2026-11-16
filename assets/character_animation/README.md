# Character motion authoring — work in progress

The goal covers every available working character except Nodens. The selected
studies now have native gameplay bindings in the canonical UE project. Original
game assets, source rigs and the artist's working Blender files are preserved.
No new ability animation is authored. These are initial gameplay motions with
remaining artistic polish, not final cinematic animation.

Current runtime selection (November 7): Dagon v006, Cthulhu v008, Nyarlathotep
v005, Yog-Sothoth v002, Shoggoths v005, Mi-Go v004, Yig v003 and original
Sghllor core/mist motion. See
`unreal 5.8/Docs/AnimationIntegration.md` for the update/artist workflow and
`unreal 5.8/Docs/ReviewedMotionIntegration.json` for measured integration evidence.
The desktop shortcut uses the same project. The older study notes below record
the authoring stages and their unresolved polish findings.

## Required set for each character

Idle, walk, run, three distinct combo melee strikes, jump, dash and intimidation
taunt. A landing clip supplements jump. Mi-Go must retain the supplied Prowl as
its dash. Each creature needs movement and attacks suited to its own anatomy.

| Character | Current work | Required follow-up |
| --- | --- | --- |
| Dagon | Anatomical 39-bone rig, ten actions, v005 independently reopened and rendered | Fix subframe floor contact; review fast claw travel and shoulder deformation; material/UV transfer; game integration |
| Nyarlathotep | Tripod rig and ten-action study, v005; evaluated sole correction and normalized skin weights | Inspect all three supporting feet and head-whip in motion; refine transitions/weights; material/UV transfer; integration |
| Cthulhu | Anatomical 51-bone rig; v008 has thirteen actions including faster forward stepping attacks and three wing-assisted backward strike variants; independent 60 fps review | Refine shoulder/finger deformation and attack transitions; verify capsule travel/contact timing, material/UV transfer and integration |
| Yog-Sothoth | Floating 52-bone rig and ten-action study, v002; eight blended lower columns, upper-mass follow-through and eye-facing calibration | Refine woven-tendril contacts and attack transitions; material/UV transfer, runtime timing and integration |
| Shoggoths | 44-bone rig and ten-action study, v005; curved forelimbs, four contact regions, original UVs/materials retained | Refine fast knuckle contact and interpolation; runtime timing, blending and integration |
| Mi-Go | v003 supplied 84-bone rig, nine new actions and original Prowl retained as dash; independent source-preservation and rendered-motion checks passed | Refine stance and scythe transitions, textured visual review, original-skeleton export and game integration |
| Yig | Existing body, tail and weapon rigs available | Coordinated four-arm attacks, weapon grips, slithering locomotion and remaining basic motions; review and integration |
| Sghllor | Existing original orb/mist presentation | Inspect native presentation; original form-appropriate basic motions and review without humanoid anatomy |

Nodens is excluded. Characters lacking usable approved models are excluded.

## Inspecting the studies

1. Open a versioned `*_BasicMotion.blend` in Blender, using **File > Open**.
2. Select the armature and open **Dope Sheet > Action Editor**. Choose a named
   action. Each role is also stored in the action's `CultUlhu.MotionRole` property.
3. Set the timeline to the action range and play. Walk/run are in place: actual
   game speed still needs to match the authored stance travel and clip timing.
4. Use **Save As** for your own edits. Never overwrite another artist revision.
5. The selected studies' `review/` folders contain compact MP4 clips and an independent
   verification report. This review checks actual reopened skinned geometry at
   half-frame intervals; closed loops alone do not prove good animation.

The four retopology-based studies currently use clay colors. Their original UVs
and texture detail must be transferred/baked before replacing textured game characters.
Shoggoths retains its original UVs and source materials.
Mi-Go retains the supplied mesh, UVs, material slots, rest skeleton, raw skin
weights and original Prowl keys/handles. Independent source reimport measured
less than 0.000001 of source height in evaluated Prowl deviation. Its original
24 fps timebase is preserved; new clips are sampled at 60 Hz on fractional
source frames. Source weights include more than four influences and imperfect
normalization; these are preserved source properties, not a new-rig quality pass.
Its authoring normalization is a parent Empty; remove that normalization during
animation-only export to the original Unreal skeleton. The supplied Prowl has
some inherited ground penetration, so retaining it does not certify foot contact.

Cthulhu v008 attack clips include authoring-root translation in normalized
metres. They are not safe drop-in gameplay root motion: runtime must sweep the
capsule, preserve camera-directed aiming and align slash contact/FX to the
authored strike phase. Their presence does not mean those gameplay changes or
all eight characters' animation sets are finished.
Rigging, surface weights, deformations, interpolation, looping, combo contact
timing and runtime animation blending all require verification. None of the
studies is certified as final artwork or already installed in Unreal.

The original models retain their attribution in [CREDITS.md](CREDITS.md).
Current modifications add anatomical skeletons, surface-connected skin weights,
original movement keyframes and temporary review shading.

## Working-copy backup

Only selected studies are archived under `archive/` as `.blend.b64`, following
the repository's binary-asset convention. Decode them with
`python assets/decode_assets.py` after cloning. Decoded copies remain separate
from the version folders and artist files. Review movies remain local to keep
the repository small. Earlier rejected studies are not release assets.

## Tools

All authoring scripts run only in a separate background Blender process and
refuse to overwrite an existing revision. They resolve the repository from
their own path and check immutable source hashes.

- `unreal 5.8/Tools/AuthorDagonMotion.py -- vNNN`
- `unreal 5.8/Tools/AuthorNyarlathotepMotion.py -- vNNN`
- `unreal 5.8/Tools/AuthorCthulhuMotion.py -- vNNN`
- `unreal 5.8/Tools/AuthorYogMotion.py -- vNNN`
- `unreal 5.8/Tools/AuthorShoggMotion.py -- vNNN`
- `unreal 5.8/Tools/AuthorMiGoMotion.py -- vNNN`
- `unreal 5.8/Tools/InspectSuppliedMotionRig.py -- mi_go`
- `unreal 5.8/Tools/VerifySuppliedRigPreservation.py -- <repo-relative .blend>`
- `unreal 5.8/Tools/ReviewCharacterMotion.py -- <repo-relative .blend>`
- `unreal 5.8/Tools/CaptureMotionAngles.py -- <repo-relative .blend> <comma-separated roles> <phase>`
- `unreal 5.8/Tools/DiagnoseMotionTwists.py -- <repo-relative .blend>`
- `unreal 5.8/Tools/InspectSkinDeformation.py -- <repo-relative .blend> <role> <phase>`
- `unreal 5.8/Tools/MeasureMotionFootContacts.py -- <repo-relative .blend>`
- `unreal 5.8/Tools/MeasureMotionFootContacts.py -- <repo-relative .blend> --quadruped`
- `unreal 5.8/Tools/RenderMotionMaterialPreview.py -- <repo-relative .blend> <role> <phase>`

Pass script arguments after Blender's `--`, and the script path after
`--python`. Measured source frames are in
`unreal 5.8/Config/CharacterAnatomicalFrames.json`. No UE gameplay code changed
for these studies, and no heavy software download was required.

Cthulhu's first passes pulled thigh and lower wing geometry with the arms.
Those rejected candidates remain local iteration evidence. The v007 working
copy adds front/side torso anchors, separate anatomical region masks, calibrated
asymmetric hand landmarks, and wing-tip seeds. `CultMotionStudy.py` supplies
versioned authoring and evaluated-surface checks; character-specific recipes
still define every joint and motion. This change does not regenerate Dagon or
Nyarlathotep's previously archived working files.

Independent Cthulhu v007 sole measurements sample the evaluated feet through
the middle of each planted phase. Both soles stay approximately 1.8–2.2 mm
above the reference floor at the normalized two-metre authoring scale. Maximum
measured stance travel after compensating for the authored forward speed is
about 3 mm walking and 6.3 mm running. These figures describe the authoring
study only; in-game playback must be calibrated to its stance-speed metadata.
See its `review/foot_contacts.json`. The sole check found and corrected a
roughly two-centimetre hover in the earlier left-foot contact.

Yog-Sothoth retains floating locomotion. Its lower sculpture contains woven,
connected appendages, so `MeasureYogAnatomy.py` measures eight continuous spatial
columns as deformation guides. These are not eight invented humanoid limbs.
The rig blends their motion through the connected surface and adds eight local
upper-mass anchors. Native multi-angle review identified the main eye face on
source +X; v002 rotates that face to +Y authoring forward. This calibration is
stored in `CharacterAnatomicalFrames.json` and has not changed the game binding.

The Yog-Sothoth combo uses a side lash, a forward thrust and a paired inward
curl. The taunt fans the hanging tendrils outward. Jump/dash/landing are body
motions; no projectile, spell, cast or ability clip is added. Its `angles/`
folder contains native front, side and back captures. The v002 independent
review additionally compares actual deformed surfaces at matched normalized
phases to establish that the three attacks differ physically. This check does
not establish final artistic quality, hitbox timing or game readiness.

Shoggoths uses the original lightweight GLB and keeps its source UVs, four
materials and packed textures. Its mouth face is rotated from source -Y to
authoring +Y. Native topology measurements separate both forelimbs and both
hind limbs below the trunk. This prevents the curled paw from pulling the
nearby hind leg. Six shorter bones spread bending along each massive forelimb;
released paws follow the forearm instead of twisting against a locked wrist.

An explicit seam-vertex remap joins 3,281 coincident vertices without moving
any source position or deleting any of the 20,302 original polygons. The
per-face UV/material fingerprint is checked before and after. A conventional
merge operation removed overlapping source faces and was rejected. The
accepted remap preserves those faces and all original per-corner UVs.

Its combo is a hooked forelimb rake, a raised forelimb hammer and a body shove
braced on both forepaws. The taunt raises the forepaws and rears the three-mouthed
upper body. `material_review/` contains a native textured preview, and `review/`
contains ten compact motion clips. The source-art fingerprint and rig checks
are in `report.json` and `review/verification.json`.

Contact measurements are provisional: the rounded knuckle can roll across its
support patch, so patch-centre travel also includes soft deformation. The v005
fast gait still has roughly 1.4 mm of subframe penetration and about 3.9 cm of
travel in the left knuckle patch at normalized two-metre scale. These are open
polish items, not passing ground-contact certification. Runtime playback has
not been calibrated or integrated. Older rejected Shoggoths blend caches may
be pruned after hash checks; their reports/captures remain as iteration evidence.
