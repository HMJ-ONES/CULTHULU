# Storage and backup

The playable Unreal project is `unreal 5.8/CultUlhu.uproject`. GitHub preserves
its source, configuration, tools, tests, documentation and authored Content,
alongside the canonical `src`, `tests` and original `assets` trees. Authored
maps and imported meshes are necessary to reproduce the current game.

Never commit Binaries, Intermediate, Saved, DerivedDataCache, cooked packages,
local static libraries, copied project archives or duplicate project folders.
CharacterAnimations and CharacterDefinitions inside Content are build outputs:
their originals live in assets/characters and Build.cs stages them explicitly.
Rebuild the core library with `unreal 5.8/Tools/BuildCanonicalCore.ps1`.

Keep one working editor build. Create standalone test packages only when needed,
avoid a second archive copy, and remove task-owned test outputs after validation.
Preserve authored assets and user backups until their recovery path is verified.
Generated outputs remain local because they are needed to run/build Unreal;
GitHub is the durable backup for the small, important inputs.

For a backup, review an explicit JSON file manifest and run tools/gh_push.py.
It layers those files onto the live game-core HEAD on a new codex/ branch,
compares binary bytes, performs a normal push and verifies the remote ref.
It does not change the working checkout or remove upstream files.
To append a verified follow-up to that same branch, pass `--update-existing`
and set `--base` to the identical branch name. The script fetches its live HEAD
and uses a normal fast-forward push; it never force-pushes over concurrent work.
