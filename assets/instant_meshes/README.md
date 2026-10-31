# Models for Instant Meshes

Twenty available source models have local binary PLY exports. Open an actual
`.ply` file using Instant Meshes' **Open mesh** button.

- `characters/`: Cthulhu, Dagon, Mi-Go, Yig, Nyarlathotep, Shoggoths, Yog-Sothoth and Nodens.
- `creatures/`: seven smaller creature models.
- `people/`: five civilian/cultist models.

These files contain geometry for remeshing. Original GLB/FBX/Blender files,
materials, textures, rigs and animation clips remain unchanged. Mi-Go and Yig
are exported in their supplied rigs' rest poses. Hidden bone-display helpers
are excluded. Nodens follows the game's existing horse exclusion.

The PLY geometry, face indices, triangle counts and bounds are checked against
the loaded source. `manifest.json` records the hashes and counts. Characters
without an acquired source mesh have no export; they are listed in that manifest.

For the dense static sculptures' reviewed retopology working files, use
[`../retopology/`](../retopology/). The initial Instant Meshes tests under
`remeshed/` were rejected and are not the delivered artist files.

Original character attribution is retained in
[`Content/CharacterModels/CREDITS.md`](../../unreal%205.8/Content/CharacterModels/CREDITS.md).
The smaller meshes retain their existing source attribution under `assets/`.

Reproduce exports with the existing Blender executable:

```text
blender --background --factory-startup --threads 2 --python "unreal 5.8/Tools/ExportInstantMeshes.py"
```

The converter runs only in background mode and never overwrites an existing
artist export. It reuses exports whose source and output hashes match its manifest.
Binary PLY files remain local to avoid unnecessary repository size.
