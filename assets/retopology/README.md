# Static character retopology — artist handoff

Four dense static sculptures have separate editable working files. Mi-Go and Yig were not remeshed. Shoggoths (20,302 triangles) and Nodens (6,260 triangles) retain their existing lighter geometry.

| Character | Original triangles | New faces | Quads | Blender file |
| --- | ---: | ---: | ---: | --- |
| dagon | 1,095,375 | 26,557 | 26,527 | [dagon_retopology.blend](dagon_retopology.blend) |
| cthulhu_avatar | 1,532,947 | 34,117 | 34,045 | [cthulhu_avatar_retopology.blend](cthulhu_avatar_retopology.blend) |
| nyarlathotep | 648,390 | 25,837 | 25,805 | [nyarlathotep_retopology.blend](nyarlathotep_retopology.blend) |
| yog_sothoth | 1,516,875 | 38,616 | 37,692 | [yog_sothoth_retopology.blend](yog_sothoth_retopology.blend) |

## Open and work

1. In Blender, use **File > Open** and select the character's `_retopology.blend` file. These files already contain the selected, smooth-shaded lower-density mesh.
2. Use **Save As** to create your own working file before sculpting or rigging. Keep the supplied copies and original high-poly sources as references.
3. Inspect the face, fingers, joints, thin fins/wings and appendages. Automatic retopology supplies a base; arrange deformation loops manually where your animation requires them.
4. Create UVs and bake the original high-poly sculpt detail into normal maps. These geometry-only copies do not carry the original UVs, texture assignments, rigs or animations.
5. Rig the finished mesh, paint weights, and test the limb movements. Then provide that working `.blend`/FBX for game integration.

The `_retopology.obj` files are alternatives for **File > Import > Wavefront OBJ**. They use Blender-compatible Y-forward/Z-up axes. The `_review.png` comparisons show the green remesh next to the gray original.

## What was checked

Every delivered OBJ is reimported into Blender. Bidirectional distance checks compare the new vertices and 15,000 area-weighted original-surface samples. All four results have zero boundary edges, non-manifold interior edges, loose vertices and zero-area faces. Original source hashes remain unchanged. The source sculptures and currently working game assets have not been replaced.

Dagon and Cthulhu use voxel surface repair and QuadriFlow. Nyarlathotep and Yog-Sothoth retain the better-fitting Instant Meshes solution with explicit topology repairs. Small repaired areas use triangles: the results are mostly quads, not exclusively quads. These are artist working bases, not finished UV-mapped or animation-ready game assets.

The first Instant Meshes results for Dagon and Cthulhu had visible gaps and were rejected. Do not use the experimental files under `assets/instant_meshes/remeshed/` as final artwork. This folder is the reviewed handoff.

Full mesh counts, surface distances, topology checks and file hashes are in [report.json](report.json). Attribution and modification notes are in [CREDITS.md](CREDITS.md).

## GitHub and disk usage

OBJ geometry, reports, attribution and reproduction tools are backed up in GitHub. Blender files use `.blend.b64` sidecars under `archive/`, following the repository's binary-asset convention. The larger original PLY files and comparison images stay local.

After a fresh clone, run `python assets/decode_assets.py` to decode the archived Blender files. Copy the decoded files from `archive/` into your own artist working folder. Existing artist working files outside `archive/` are never replaced by that archive decoding.

## Reproduction

Run these only through a separate background Blender process; the scripts refuse to reset a live artist session.

1. `unreal 5.8/Tools/ExportInstantMeshes.py` exports and validates the available source models.
2. `unreal 5.8/Tools/RemeshStaticCharacters.py` uses the installed Instant Meshes CLI for its initial candidate meshes. Run it with Blender's bundled Python/NumPy and `--instant-meshes <path-to-exe>`.
3. `unreal 5.8/Tools/RetopologizeStaticModels.py` reconstructs and validates the four final models in background Blender. It retains completed working files and caches expensive solver output under Saved.
4. `unreal 5.8/Tools/FinalizeRetopology.py` makes the reviewed smooth-shaded working copies and captures. It never overwrites an existing artist copy.
