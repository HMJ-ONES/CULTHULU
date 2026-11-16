# Code-authored creature motion

The two approved skeletal characters now have inspectable motion recipes in `Tools/CultRigMotion.py`, measured hierarchy/joint data in `Config/ApprovedRigMotion.json`, an Unreal asset authoring tool in `Tools/BuildApprovedGameplayAnimations.py`, and native pose blending in `Source/CultUlhu/Private/CultFluidAnimInstance.cpp`. Generated assets live beside each character under `Gameplay20261030_v1`; source meshes, skeletons and imported clips are preserved.

## Actual rigs and limb mechanics

Mi-Go has 85 tracked bones, including deformation bones and original control/end-marker bones. Its two main arms use shoulder/bicep/forearm/claw chains; the smaller arm pair has its own mini-shoulder/bicep/forearm and pincers. The two thigh/shin/foot/toe chains retain the source Prowl and its baked foot anchors for locomotion. Idle leaves the feet fixed; jump and landing flex the thigh and shin in opposite directions. Four wing-base chains spread during takeoff, brace/fold during dash and have small idle motion. The neck/torso breathe subtly; jaws open modestly during a strike. Unused IK poles, finger/toe end markers and control bones retain the supplied transforms instead of receiving arbitrary motion.

Yig has three independent skeletal pieces: 85 tracked body/arm bones, six weapon-rig bones, and 32 coil/tail bones. Four arm chains share shoulders/spine links; their motion is staggered and bounded rather than multiplying the same large angle through every joint. The coil uses small phase-shifted horizontal rotations distributed along the tail, retaining the original vertical contact profile. It does not pretend to walk on humanoid legs. The four rigid swords have zero-origin pivots and baked vertex positions; measured weighted geometry assigns them one-to-one to left hand, right hand, secondary left hand and secondary right hand. Each sword receives the corresponding hand's full reference-to-current transform, including rotation around the actual hand position. Rotating the weapon root alone would make swords orbit the wrong pivot.

Joint flex axes are computed from the posed hierarchy in source space. The intended anatomical lateral/up/forward axes are transformed into each joint's measured local frame. The generated strike has eased anticipation, a faster contact phase and recovery. Cast, jump, dash and landing use separate bounded recipes. The authored source clips remain available and credited.

## Playback and transitions

The native animation instance blends the previous rendered local pose into the next clip with a smooth quintic weight over 0.14 seconds. An interrupted transition begins from the current blended pose. Supplied loop seams receive a short continuity blend. Role changes that reuse the same clip preserve playback phase. Locomotion cadence follows actual movement speed and reverses when backing up. The independent rig pieces update after their owning model actor, and their playback clocks advance even when an individual piece is temporarily outside the view.

The model chooses idle, walk, run, jump, land, dash, melee/heavy attack or successful ability-cast states. Animation is presentation only: damage, cooldowns, stamina, committed aim, collision and dodge rules remain authoritative in gameplay.

## Reproduce and improve

1. Edit the small angle/timing recipes in `Tools/CultRigMotion.py`; keep the measured profile tied to the existing approved source imports.
2. Run `Tests/test_rig_motion.py` with Python. It checks quaternion normalization, bounded joint deltas, loop/recovery endpoints, planted contacts and rigid-weapon following without requiring Unreal.
3. Close the game/editor, validate the canonical project and compile `CultUlhuEditor` after C++ changes.
4. Run the canonical Unreal Editor with `-ExecutePythonScript=<project>/Tools/BuildApprovedGameplayAnimations.py`. The tool reads current source poses, preserves previous owned generated clips, and refuses to overwrite unowned animation assets. No Blender installation or model reimport is required.
5. Run native `CultUlhu.Art.FluidMotion`, `CultUlhu.Art.RigGeometryAudit`, `CultUlhu.Art.ApprovedCharacterModels` and `CultUlhu.Menu.CrosshairAimAndMotion`. Review front/side/rear motion in the actual game as well as the numeric results.

The first numeric pass checked 1,024 samples, with a maximum 34-degree authored joint delta and floating-point-only drift in the planted-contact and rigid-weapon checks. Native output and visual observations are recorded with the final verification report.

These are original functional gameplay motions, not a claim of cinematic animation quality. Terrain-adaptive foot/coil IK, custom contact poses for every target height, secondary flesh/cloth dynamics, a dedicated first-person arm view and artist-directed acting remain further work. Existing static sculptures need deformation-ready meshes and rigs before equivalent limb animation can be authored; keep their source files intact for the owner's remesh/rig work.
