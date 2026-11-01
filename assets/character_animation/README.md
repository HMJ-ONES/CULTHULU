# Character motion authoring — work in progress

The goal covers every available working character except Nodens. These files
are original motion studies, not a finished or integrated animation release.
Original game assets, source rigs and the artist's working Blender files are
preserved. No ability animation is being authored.

## Required set for each character

Idle, walk, run, three distinct combo melee strikes, jump, dash and intimidation
taunt. A landing clip supplements jump. Mi-Go must retain the supplied Prowl as
its dash. Each creature needs movement and attacks suited to its own anatomy.

| Character | Current work | Required follow-up |
| --- | --- | --- |
| Dagon | Anatomical 39-bone rig, ten actions, v005 independently reopened and rendered | Fix subframe floor contact; review fast claw travel and shoulder deformation; material/UV transfer; game integration |
| Nyarlathotep | Tripod rig and ten-action study, v005; evaluated sole correction and normalized skin weights | Inspect all three supporting feet and head-whip in motion; refine transitions/weights; material/UV transfer; integration |
| Cthulhu | Anatomical 51-bone rig and ten-action study, v007; folded wing fans and facial tendrils, 60 fps keys, independent sole correction | Refine shoulder/finger deformation and attack transitions; complete visual review, material/UV transfer and integration |
| Yog-Sothoth | Floating 52-bone rig and ten-action study, v002; eight blended lower columns, upper-mass follow-through and eye-facing calibration | Refine woven-tendril contacts and attack transitions; material/UV transfer, runtime timing and integration |
| Shoggoths | Lightweight source available | Anatomical rig, knuckle-supported movement and distinct attacks, review and integration |
| Mi-Go | Existing supplied skeleton and animation available | New locomotion/basic combo/taunt; preserve original Prowl for dash; review and integration |
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
5. The selected studies' `review/` folders contain ten compact MP4 clips and an independent
   verification report. This review checks actual reopened skinned geometry at
   half-frame intervals; closed loops alone do not prove good animation.

The static studies currently use clay colors. Their original UVs and texture
detail must be transferred/baked before replacing textured game characters.
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
- `unreal 5.8/Tools/ReviewCharacterMotion.py -- <repo-relative .blend>`
- `unreal 5.8/Tools/CaptureMotionAngles.py -- <repo-relative .blend> <comma-separated roles> <phase>`
- `unreal 5.8/Tools/DiagnoseMotionTwists.py -- <repo-relative .blend>`
- `unreal 5.8/Tools/InspectSkinDeformation.py -- <repo-relative .blend> <role> <phase>`
- `unreal 5.8/Tools/MeasureMotionFootContacts.py -- <repo-relative .blend>`

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
