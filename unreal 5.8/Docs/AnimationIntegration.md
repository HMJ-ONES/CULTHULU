# Character motion and artist handoff

`Config/ReviewedCharacterMotion.json` selects the current motion revisions.
There is one game project: `unreal 5.8/CultUlhu.uproject`. The desktop
`Cult-Ulhu` shortcut opens this project with `-game`; it has no separate content
copy. Restart the running game after a content/DLL update.

## What is being integrated

| Character | Revision | Motion source |
|---|---|---|
| Dagon | v006 | Anatomical rig; planted/gathering feet during melee |
| Cthulhu | v008 | Anatomical rig; three forward and three backward attacks, wing assistance |
| Nyarlathotep | v005 | Tripod anatomy and three different strikes |
| Yog-Sothoth | v002 | Floating mass and eight measured lower appendage guides |
| Shoggoths | v005 | Original textured surface, four support limbs |
| Mi-Go | v004 | Supplied skeleton; original Prowl retained for dash |
| Yig | v003 | Three original skeletons; four swords follow their hands; matching 60 Hz compression |
| Sghllor | v001 | Native core/mist transforms on the original model |

The basic roles are idle, walk, run, three melee strikes, jump, dash, landing
and taunt. **T** requests a taunt; movement, jumping, combat and dashing interrupt
it. No new spell/cast animation is authored. Nodens is excluded from this work.

Cthulhu's melee step uses swept CharacterMovement, including server movement
replication. It does not grant dodge immunity. Existing attack collision and
camera targeting remain authoritative. Animation contact poses are timed to
the existing sweep; visual claws do not apply extra damage.

## Safe update sequence

1. Preserve artist edits and choose a new revision in the selection JSON.
2. Author and independently review the Blender study. Keep earlier source
   files. The Blender program folder is a disposable tool, never an art store.
3. Export with `ExportAnatomicalMotionStudy.py` or
   `ExportSuppliedMotionStudy.py`. Use the canonical Tools scripts, not copies.
4. Run `Tools/ValidateCultUlhu.py`, then compile **CultUlhuEditor Win64
   Development** against the canonical `.uproject`. Finish compiling before
   launching Unreal imports or changing another C++ header.
5. Stage new assets with `ImportAnatomicalMotionStudy.py` (environment variable
   `CULT_MOTION_CHARACTER`), `ImportMotionStudyClips.py`, or
   `BuildReviewedYigAnimations.py`. Staging does not change active bindings.
6. Check the source-hash and pending-binding reports in
   `Saved/MotionIntegration`. Run `ActivateReviewedMotion.py` only after every
   selected revision is staged and the matching C++ build succeeds.
7. Run the rendered native tests `CultUlhu.Art.ReviewedCharacterMotion` and
   `CultUlhu.Art.ApprovedCharacterModels`. Inspect their screenshots and test
   an actual match; a passing transform test alone cannot establish fluidity.
8. Restart the desktop shortcut. Back up the exact runtime `.uasset` files,
   source studies, configuration, code and attribution through the repository's
   binary-safe push process. Never commit build caches or `Saved`.

Activation preserves the old registry/configuration in
`Saved/MotionIntegration/BeforeActivation`. Original model assets and supplied
clips are checked byte-for-byte and remain available. New skeletal material
copies use the original textures; source materials are preserved.

## Simple next steps in Blender

1. Open the versioned `.blend` under `assets/character_animation/<character>/`.
   Save your edits under a new name/version. Yig's motion recipes are in
   `Tools/CultYigReviewedMotion.py`; its supplied FBX is listed in
   `Config/ApprovedCharacterModels.json`.
2. Review the three attacks from front and side. Adjust silhouette, foot
   contact and recovery before adding finer finger or tentacle movement.
3. For Dagon, Cthulhu, Nyarlathotep and Yog-Sothoth, refine the provisional
   retopology/UV projection where needed. Their original sculptures remain
   untouched. Preserve vertex groups if editing the mesh in place; a new mesh
   requires deliberate weight transfer and a new revision.
4. For Shoggoths, preserve the original per-corner UVs and material slots.
   Refine the rolling knuckle contacts: the authoring review still records
   small penetration and sliding. Do not erase that limitation from reports.
5. Mi-Go and Yig already have supplied rigs. Keep the source rigs and clips;
   improve new motion on separate actions. Do not remesh these by default.
6. Send back the edited file/version. Re-export and test it through the same
   sequence so both the editor and desktop game receive the same result.

These are initial gameplay motions, not a claim of final cinematic animation,
perfect foot locking, or a completed human artistic review.

## Verification — November 7, 2026

The canonical Editor target compiled successfully. Rendered native checks
passed for ReviewedCharacterMotion (83 role cases), ApprovedCharacterModels
(height, capsule, camera visibility and corpse geometry), FluidMotion
(Mi-Go/Yig transition continuity), CrosshairAimAndMotion and SkillCombat.
The independent Yig recipe check also passed hand/sword alignment, loop closure,
tail floor height and distinct attack trajectories.

The standalone two-process DOTGOO probe passed replicated movement and combat,
moving guest creep waves, local menus, host bot takeover and the server/guest
tower-warning countdown. This used direct local networking; it does not resolve
or certify the separately deferred production lobby reservation error.

Native capture inspection confirmed visible textured models for all eight.
Small captures and measured checks establish integration, not final artistic
quality. Original meshes and supplied animation files remained byte-for-byte
intact during staging and activation; source materials were retained. Runtime additions total
122 owned assets, approximately 90 MB, excluding build caches.
