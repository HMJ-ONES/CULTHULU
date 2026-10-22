# Placeholder vessel surface

Hero bodies used the stock sphere with an almost-black tint and no surface
detail. Inspection confirms that BasicShapeMaterial really has a Color
parameter; this was not a missing-parameter bug. A dedicated original material
now gives the existing placeholder bodies restrained vein/grain variation,
moderately rough highlights, a small dark-area fill and a subdued faction rim.
The shape remains a placeholder oval. No authored hero model is replaced.

The material reconstructs continuous sphere coordinates from the stock UVs,
so its procedural pattern closes at the longitude seam and converges at the
poles. The pattern follows the body's UVs rather than sliding through world
space. It is opaque, lit and depth-tested; it adds no through-wall silhouette,
texture, geometry, light, post-process pass or collision. Each hero has its own
material instance. The old material remains a fallback if the generated asset
is unavailable. The existing Materials directory is already in AlwaysCook;
a fresh standalone package has not been generated or verified for this pass.

BuildVesselSurface.py explicitly resolves the canonical project, refuses to
overwrite an asset without its ownership tag and preserves a prior owned asset
by content hash before rebuilding. Its HLSL source is committed alongside the
small generated uasset. Imported art, eyes, maps and original kit definitions
are untouched.

## Native evidence

Canonical validation passes and CultUlhuEditor Win64 Development compiles on
UE 5.8.3 in 15.12 seconds. The final native batch has five successes and no
automation errors: both vessel-surface fixtures, both hero-plate fixtures and
combat feel. The surface fixtures verify compiled non-fallback shader maps,
ten independent production material instances per mode, correct faction
accents and unchanged actor/body transforms, capsule size, health and visual
collision. Eight native 1280 x 720 images compare old/new materials on the same
frozen bodies, with the same cameras and scene in each pair. Four group images
are backed up here; all eight image hashes are in VesselSurfaceResults.json.

The first version passed all five native checks but was rejected visually:
its saturated bodies, broad veins and bright rim looked rubbery. The accepted
revision reduces base colour, vein contrast, fill and rim strength. The report
retains that rejected run's outcomes, image hashes and cost samples.

| Mode | Old surface | Revised surface |
| --- | --- | --- |
| Domination | [Before](Images/VesselSurface_Domination_BeforeGroup.png) | [After](Images/VesselSurface_Domination_AfterGroup.png) |
| MOBA | [Before](Images/VesselSurface_Moba_BeforeGroup.png) | [After](Images/VesselSurface_Moba_AfterGroup.png) |

Render-cost evidence uses four 120-frame windows in A-B-B-A order in each
frozen editor scene, discarding the first ten samples of each window. These
ordinary editor viewport samples are separate from the high-resolution image
captures. Draw-call and primitive counts are unchanged in the recorded final
windows. The first accepted run has higher median GPU times with the new
material: roughly +0.196 ms in Domination and +0.625 ms in MOBA. Some new-material
windows also have substantially higher CPU times. The unchanged follow-up
comparison measures +0.156 ms in Domination and +0.097 ms in MOBA. Both runs
and exact per-window values are retained in the results; neither
run is an FPS or low-spec certification. No performance improvement is claimed.

All 425 previously backed-up Content files remain byte-identical to f17051d,
and all sixteen original released definitions match after line-ending
normalization. Only one small material asset is added. Normal project MCP
configuration is unchanged; isolated native tests disable its transport by
command line.

## Remaining production gap

This gives the placeholders some surface form. It does not supply distinct
hero silhouettes, anatomical models, rigs, first-person arms, movement and
attack animation, final hero art or a polished selection preview. The player
can still see a large oval back in third person. Small distant bodies and
arbitrary lighting/elevation have not been certified for readability. The
remaining art/animation gap is large, and the overall game remains far short
of the requested production refinement and verified human match quality.
