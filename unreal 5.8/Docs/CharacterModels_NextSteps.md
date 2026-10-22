# Character art: simple next steps

Work in **CULTHULU/unreal 5.8/CultUlhu.uproject**. The original gameplay kits, height, movement and collision remain owned by the existing player pawn. Model import never replaces your local art.

## 1. Try the imported characters

1. Open the canonical project, press Play, choose a mode, then choose a character.
2. Select Cthulhu, Dagon, Mi-Go, Nyarlathotep, Shoggoths, Yig, Yog-Sothoth or Nodens to inspect the source model. Try Sghllor for the original glowing orb and animated mist.
3. Enter play and test walking, jumping, combat and **V** for first/third person.
4. Static source sculptures move with the character but have no authored limb animation. This is an art gap, not a claim that they have a complete animation set.

## 2. Your remeshing / retopology priorities

| Priority | Character | Actual source triangles | What to do |
| --- | --- | ---: | --- |
| 1 | Cthulhu | 1,532,947 | Retopologize into one coherent deforming body; bake sculpture detail into normal maps; unwrap UVs; rig wings, arms, head and tentacles. |
| 1 | Yog-Sothoth | 1,516,875 | Simplify individual forms and their silhouette; bake detail; decide which tendrils/forms animate and which remain rigid. Keep a cosmic shape rather than forcing a human rig. |
| 1 | Dagon | 1,095,375 | Retopologize body, face, fins and hands; bake detail; unwrap; rig for swimming/land movement and original kit attacks. |
| 2 | Nyarlathotep | 648,390 | Retopologize and bake; build a rig appropriate to this exact Lovecraftian form. |
| 3 | Shoggoths | 20,302 | Source is static. Build a nonhuman deformation rig or authored shape animation. Preserve the body silhouette and textures. |
| 3 | Nodens | 6,822 | Keep the selected two-wheel chariot. Separate/rig wheels if needed; do not add Zeus, a generic rider or a different vehicle. Design the deity rider later. |

Keep original high-resolution files. Work on a copy. Aim first for a good silhouette and clean deformation; final budgets require profiling in a full match, not a triangle count alone.

## 3. Animation / mapping work

- **Mi-Go:** use the original `Bugoid2.blend` under `Saved/CharacterSources/mi_go/original_blend/source/`. Its supplied `PoseLib` action is exported as **Prowl**, matching the source page. The supplied motion is a transition into a crouched prowl, not a complete seamless locomotion set. Global translation is baked out using the original feet so it stays with the character. Add idle, sprint, jump, hit reaction, death and attacks. Refine fungal growth and wings as desired.
- **Yig:** use the original FBX under `Saved/CharacterSources/yig/original_fbx/source/`, not the converted GLB, whose rig separates during animation. Preserve the source's body and tail rigs. The existing **Scene** timeline is 401â€“641 at 24 fps. Original sword object motion is baked to rigid bones so it survives skeletal import. Review and export clearly named individual actions before mapping walk/attack; the supplied idle is not an attack or walking cycle.
- **Cthulhu, Dagon, Nyarlathotep, Shoggoths, Yog-Sothoth, Nodens:** supplied GLBs contain no animation clips. Rig and animate these; the importer cannot recover clips that are absent.
- First-person arms and animation-driven attack timing remain separate art/integration work. Imported art currently follows the existing physical combat timing; it does not add homing hits or change damage.

For a revised model: face Unreal **+X**, use **+Z up**, keep a clean origin and consistent scale, export FBX or GLB with textures and only intended clips. Preserve each nonhuman skeleton. Export a reference pose too. Test feet, silhouette and animation at the character's existing height.

## 4. Fill acquisition gaps

| Character | Required next action |
| --- | --- |
| Bokrug | Obtain the selected **3DRT.com Lizard quadruped** from its creator with permission for game distribution. The selected Sketchfab page has no download. Do not substitute Yig's Naga. |
| Hastur | Obtain **M.O.T.H.'s King In Yellow** file and license. The selected page has no download. The older alternative is not approved. |
| Nug and Yeb | Obtain **Stilesstoddard's Floating Horror** file and license. Then make two distinct floating bodies for one selectable entity. The selected page has no download. |
| Rhan-Tegoth | Clear underlying rights: the uploader explicitly calls this Darksiders Genesis fan art. Its uploader's CC-BY label alone does not settle that issue. |
| Shub-Niggurath | Clear underlying rights: selected Dark Young is identified as an Overlord anime creature. No download/import while blocked. |
| Mother Hydra / Ghatanothoa | No final approved model for this task. Existing gameplay placeholders remain; no substitute was invented. |

Original source links and per-character evidence are in [ApprovedCharacterModels.json](../Config/ApprovedCharacterModels.json). The four previously deferred characters stay excluded from the release roster.

## 5. Import a revised or newly authorized file

1. Close Unreal after saving your work.
2. Put a copy under `unreal 5.8/Saved/CharacterSources/<exact_character_id>/` with a **new filename**, preserving the existing source. Do not put downloaded executable scripts there.
3. Update that character's `source_file` in `Config/ApprovedCharacterModels.json`. Record the real creator, source URL and license. Only mark rights cleared once you have the needed permission.
4. In PowerShell from the repository root run:

   ```powershell
   & '.\unreal 5.8\Tools\ImportSelectedCharacterAssets.ps1'
   ```

5. Read `unreal 5.8/Saved/Reports/CharacterModelImport.json`. A source link or downloaded file is not an imported model: look for `IMPORTED_STATIC`, `IMPORTED_RIGGED` or `IMPORTED_ANIMATED`.
6. Import creates a source-hash directory. It preserves older imports and refuses to overwrite unowned assets. Review the model in selection and play before retiring an older revision.
7. Configure `animation_roles` only for actual clips: `idle`, `walk`, `run`, `attack`, `preview`. Keep names explicit. One generic timeline should be reviewed/split first.
8. Send the revised art and observations for the next integration pass. A new rig may need animation mapping, not humanoid retargeting.

Raw downloads and working receipts stay local in Downloads/Saved. Do not commit Saved, Binaries, Intermediate, build packages or caches. Runtime assets, code, configuration, attribution and the progress report belong in the reviewed GitHub backup. No second Unreal project is needed.

For the two current original FBX rigs, `Tools/RestoreApprovedRigMaterials.py` restores the exact source GLB PBR maps on the FBX geometry. Run it through Unreal's `-ExecutePythonScript` after a new rig import if needed. It edits only importer-owned assets. The Blender preparation scripts preserve the originals and refuse to overwrite an existing prepared revision; choose a new revision folder when changing preparation settings.

The temporary signed Blender conversion tool was removed after verification, reclaiming about 1.34 GB. All source model downloads, original rig files and prepared sources remain available. If you need to rerun Blender preparation later, use your own official Blender installation; importing the prepared sources does not require Blender.
