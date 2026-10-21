# Playable project checkpoint — 2026-10-21

The active project is `unreal 5.8/CultUlhu.uproject`. This checkpoint preserves
the current local gameplay work and authored maps/assets over the newer upstream
directory migration. It is a development checkpoint, not a release certification.

The reported S/LMB editor-camera incident is covered by the temporary PIE Alt+S
command filter; F8 remains available for deliberate ejection. EditorInputSafety
and the camera-constrained Army/Match HUD input regressions passed. See
EditorPlayInput.md for reproduction and the exact tested view shapes.

Humanoid animation staging now shares a manifest of three skins and sixteen
clips. Runtime loading uses loose staged Content files, never the adjacent raw
asset repository. Civilian skeletal assets are explicitly cooked. All 48 staged
clips match their originals. Editor HumanAnimationFiles and StagedHumanPoses
passed on this checkpoint, including the prayer, construction, conversion,
sacrifice and attack poses on the actual imported rig parts.

The Development Game target compiled after updating its build settings and
guarding editor-only debug actor labels. The existing menu camera and three mode
starts now have persistent runtime tags; PrepareRuntimeAnchors.py saved the
existing maps without replacing transforms or geometry. The final Editor and Game targets compiled, Windows cooking/staging succeeded,
and both HumanAnimationFiles and StagedHumanPoses passed in the independent
staged executable with Editor=0 and no adjacent repository asset tree. The
cooked skylight cubemap and initialized-world screenshot are also checked.

The first standalone run exposed sixteen Nanite material-usage fallbacks and
a missing skylight cubemap. Persisted usage flags on the existing world materials
and explicit sky-asset cooking resolved both in the final runtime: zero material
fallbacks, zero missing cubemap warnings and zero automation errors. See
Saved/Reports/StandaloneDeliveryFinal.json and the local rendered baseline
Saved/Screenshots/StagedFreeRoam_20261021.png. No binary package is retained.

Competitive mode execution in a package, multiplayer, final hero art, sustained
frame rate and human combat balance remain unproven. The broader findings are in
`docs/PRODUCTION_AUDIT_2026-10-20.md`. Generated logs, screenshots, executables,
libraries and caches are excluded from GitHub. A verified obsolete 2.47 GiB
editor PCH/object pair was removed. After verification, another 3.74 GiB of
temporary standalone staging, cooked metadata, Game-target intermediates and
Game binaries/debug symbols was removed. The working editor build remains.
Future standalone verification must rebuild the Game target.

The rendered world still has bright blockout lighting, a placeholder hero vessel,
flat material treatment and a prototype HUD. The delivery fixes do not establish
professional visual quality; the full playable/fun/beautiful goal remains active.

A subsequent Free Roam presentation pass adds fixed exposure, moonlight, a
working dusk gradient and bounded cave fill. Its editor render/collision/camera
checks passed; see WorldPresentation.md. The earlier standalone results above
predate that profile and do not certify its packaged rendering.
