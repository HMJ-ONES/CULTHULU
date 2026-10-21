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
existing maps without replacing transforms or geometry. The latest small test
assertion changes have been compiled in the Editor target; standalone needs an
incremental rebuild followed by cook/stage and actual packaged execution.

Packaging, multiplayer, final hero art, sustained frame rate and human combat
balance remain unproven. The broader findings are in
`docs/PRODUCTION_AUDIT_2026-10-20.md`. Generated logs, screenshots, executables,
libraries and caches are excluded from GitHub. A verified obsolete 2.47 GiB
editor PCH/object pair was removed; the current editor and game caches remain
needed for ongoing build verification.
