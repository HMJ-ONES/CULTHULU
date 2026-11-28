# Free Roam presentation — 2026-10-21

Free Roam now applies a runtime moonlight profile after the legacy level cleanup.
It uses fixed exposure, restrained bloom/vignette, a cool directional key and
ambient fill. The bright daytime atmosphere is hidden; a small original gradient
material supplies the dusk background. Imported art and authored map files are
not rewritten. Domination/MOBA keep their existing arena presentation path.

Two nonshadowing point lights per existing cave improve floor and creature
readability. Their radius is 100 m, with a 250 m draw limit and 50 m fade range.
Their GPU cost has not been profiled. The profile is applied once, with no new
per-frame actor logic. The background dome has collision, shadows and Nanite
disabled; it is centered using the mesh's actual bounds.

CultWildsPresentation.cpp owns the runtime settings. BuildWildsSky.py creates the
dedicated material and validates its connected graph. The material is under
10 KB and uses no texture downloads. An explicit `-CultRebuildWildsSky` script
invocation preserves the previous generated asset in Saved/Backups before
rebuilding its graph. Do not use it to replace imported/user-authored materials.
The existing Materials cook directory includes this new asset, but the new
profile still needs a subsequent standalone recook/runtime verification.

## Verification and limitations

The canonical validator and final CultUlhuEditor build passed.
WildsPresentationFinal.log passes CultUlhu.Menu.WildsPresentation and
CultUlhu.Menu.PopulationPresentation with zero automation errors and no
material-usage fallback warnings. Several NPC movement-timestep warnings occurred
during world entry; this change does not resolve them or prove sustained FPS.

The comparison captures the courtyard and cave before/after in the same paused
generated world. It checks the real camera position, the player's unchanged
physical transform, actual supporting collision and an upward visibility trace
through the background. The fixture restores its pause/camera/CVar state.

Local evidence:

- Saved/Screenshots/WildsCourtyard_Before.png and WildsCourtyard_After.png
- Saved/Screenshots/WildsCave_Before.png and WildsCave_After.png
- Saved/Reports/WildsPresentationSummary.json (results and screenshot hashes)

The final images were inspected. The dusk gradient renders, and cave creatures
and ground are distinguishable. Earlier iterations were rejected for unreadable
shadows and a disconnected sky gradient; an isolated bright-material render
confirmed the dome geometry was visible before its graph was repaired. Those
diagnostic images are not final presentation evidence.

The screenshots still show blockout terrain/temple treatment and a placeholder
hero vessel. This is a lighting/readability improvement, not proof of professional
visual refinement, human combat quality, every cave angle or every display's
dark-scene readability. Final hero art, environment detail, consistent material
treatment, HUD refinement and sustained performance remain substantial work.
