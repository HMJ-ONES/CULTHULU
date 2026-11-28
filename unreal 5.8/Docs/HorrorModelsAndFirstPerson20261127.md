# Horror models and first-person presentation

Work started November 27, 2026. Canonical project: `unreal 5.8/CultUlhu.uproject`.

## Selected models

| Character | Acquired source | Integration and remaining art |
|---|---|---|
| Hastur | [Grim Reaper Low Poly — Emmanuel__Bouvier](https://sketchfab.com/3d-models/grim-reaper-low-poly-c4106291b7454917a909afa37affed08) | Cemetery removed; character retained, reduced render LOD, aged yellow materials and original folded, torn mantle with exposed tendrils. Playable using the existing kit and procedural body motion. The character source is static; its download's animation belongs to the scenery. It still needs an artist-quality rig for articulated cloth/limb performance. |
| Nug | [Flesh Blob — ChopperManiac](https://sketchfab.com/3d-models/flesh-blob-b3f5bb51f8144a30b6fa6b22f5d44bf7) | Actual licensed GLB, 1K package, approximately 22K source triangles. Source wall/ground excluded from the runtime binding. Supplied morph pulse retained. |
| Yeb | [Living Flesh Blob — Yury Misiyuk](https://sketchfab.com/3d-models/living-flesh-blob-free-organic-horror-asset-6357bbc094d54cf5ba150ef2951b3750) | Actual licensed GLB, 1K PBR maps, approximately 10K source triangles. Separate mesh, skeleton and pulse animation. Floats diagonally above Nug with a different drift phase and speed. |

All three acquired packages identify CC-BY-4.0 in their embedded attribution. The official download dialogs explicitly allow commercial use with credit. Credits remain in `Content/CharacterModels/CREDITS.md`. Existing art and previous selections are preserved.

Research exclusions: CinekR9's King in Yellow and cyclocosmia3's low-poly wraith are noncommercial; the Pinshape King in Yellow bust is also no-derivatives; NSDesign/NSMiniatures and Epic Miniatures offerings require payment or have personal-use terms. The Meshy Hastur page did not expose an actual downloadable package in this session. NORBERTO-3D's reaper credits a downloaded base without identifying its source. The Minecraft necromancer and Cartoon Network reaper are identifiable third-party characters. Those candidates were not shipped. Several million-triangle generated necromancers were unsuitable for this lightweight pass.

Optional future candidate: [BlenderEffect's Yellow-robed King Hastur on BOOTH](https://booth.pm/ja/items/6733817) is indexed as a paid model. It was not purchased, and its game-redistribution terms were not verified. The current update adds no paid dependency.

## First person and projectiles

- Actual licensed arm, claw or weapon geometry is extracted for Cthulhu, Dagon, Yig, Mi-Go, Shoggoths, Nyarlathotep and Rhan-Tegoth. Original materials and current skeletal poses are retained. The compact bake has an asset-path compatibility check and a 24K-vertex hard budget per viewer.
- Camera-relative ready poses, restrained sway, breathing, attack follow-through and slash trails are local presentation. The world skeleton and existing server melee sweep continue to decide damage. These meshes have no collision and do not replicate.
- Bokrug, Mother Hydra, Shub-Niggurath and Nug & Yeb receive original contracting, ridged spit organs. The twins have two. They are procedural view anatomy, not extracted source-model mouths.
- Native first-person scaling and a local near-plane override keep anatomy near the camera. First-person motion blur is disabled for readability. Other players retain the complete world model; third person hides the view anatomy.
- Other casters retain their existing presentation. This does not claim a bespoke hand rig for characters without suitable anatomy.
- Original acid-glob and chitin-dart meshes, wet materials, drip streaks and impact splashes replace plain organic projectile spheres. Bokrug, Mother Hydra, Shub-Niggurath and Mi-Go use them. Damage, travel, hitboxes and cooldowns are unchanged by this visual pass. Drips share the existing trail buffer; no extra replicated actors, particle collision or dynamic lights are added.
- [inspiredsloth's CC0 Projectile Effects Vol. 1](https://inspiredsloth.itch.io/projectile-effects) was researched, but its download did not complete. None of its files or Godot-specific shaders were used. The shipped projectile artwork is original.

## Blender sources and recovery

1. Open a copy of `Saved/CharacterSources/HorrorRefresh20261127/hastur/prepared.glb`, `nug/prepared.glb` or `yeb/prepared.glb` in Blender. These folders are inside the canonical project and outside the deleted Blender installation folder.
2. Hastur's original added mantle source is `Saved/HorrorRefresh20261127/OriginalHastur/SM_KingMantle.obj`; `Tools/BuildHasturMantle.py` regenerates it. Preserve the imported source meshes and work in a new versioned file.
3. Compact licensed source archives and attribution records live under repository `assets/characters/horror_refresh_20261127/`. Run `Tools/RestoreHorrorSources.py` to restore staged GLBs after a clone. It verifies hashes and refuses to overwrite edited files.
4. After changing a skeletal character mesh, regenerate the local view geometry with Unreal automation test `CultUlhu.Art.BakeFirstPerson`, then run `CultUlhu.Camera.FirstPersonAnatomy`. Incompatible mesh paths disable the old bake rather than drawing a previous character's arms.
5. Keep both artists' credits when editing or redistributing the twins. Raw source URLs and hashes are in the archive's `sources.json`.

## Verification

The canonical CultUlhuEditor Development target compiled successfully. Focused native automation passed: approved character previews/spawns/collision across all three modes, the sixteen-character DOTGOO playability matrix, projectile geometry budgets, eleven first-person actual-primary-attack cases, and camera safety in all three modes. The final Dagon skin-weight override correction was compiled, all seven view bakes regenerated, and Dagon's actual-attack test rerun successfully.

Two separate local game processes passed the DOTGOO host/guest probe with Nug & Yeb selected on the host: movement, combat, local Escape menus, host replacement by a bot, guest creep movement, and the replicated tower warning countdown. This is a local replication test, not verification of Internet/Radmin/Cloudflare matchmaking. The host fixture did not measure creep movement; the guest did. Evidence is in Saved/Automation/Room2_{host,guest}.json and Saved/Room2{Host,Guest}.log.

An earlier screenshot review caught and corrected source scenery in Nug's binding, idle arms below the camera, exposed organ roots and an unsuitable floating mask placement. These were not accepted on the strength of visibility flags alone. Final first-person screenshots are under Saved/Screenshots/FirstPersonRefresh; character previews are under Saved/Screenshots/ApprovedCharacters.

This is focused validation, not a claim that every repository test passes or that every character has bespoke first-person anatomy. Hastur still needs an artist-quality articulated rig; seven characters use extracted source hands/weapons, four use original procedural casting anatomy, and the remaining casters retain their existing presentation. Low-spec performance and remote network latency still require testing on representative machines.

The desktop Cult-Ulhu shortcut launches this same canonical project and compiled module. Close any old game process and relaunch it to load these changes. No separate game copy or second Unreal project was created.
