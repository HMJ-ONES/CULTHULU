# CULT-ULHU — agent instructions (read before any Unreal work)

## PERMANENT ARCHITECTURAL INVARIANT (2026-10-15)

There is exactly ONE Unreal project: `<repo>/unreal 5.8/`, project file
`<repo>/unreal 5.8/CultUlhu.uproject`, UE 5.8. A second `unreal/` project
directory once caused the editor to compile stale code while the new menu /
gameplay classes sat unused in the repo. That must never recur.

**RULE 1:** All Unreal project files belong in `unreal 5.8/`.
**RULE 2:** Never create another `.uproject`.
**RULE 3:** Never use the old `unreal/` directory.
**RULE 4:** Never commit new Unreal source files outside the canonical project.
**RULE 5:** Before every build, validate the project path
(`python3 "unreal 5.8/Tools/ValidateCultUlhu.py"` from the repo root).
**RULE 6:** After modifying gameplay C++, compile the actual canonical target
(`Build.bat CultUlhuEditor Win64 Development -Project=...unreal 5.8/CultUlhu.uproject`).
**RULE 7:** Python automation must resolve the canonical project dir explicitly
(`Tools/` scripts derive the repo root from `__file__`; never assume CWD).
**RULE 8:** Never overwrite locally created assets without preserving them.
**RULE 9:** Documentation and automation must use the same directory.
**RULE 10:** Treat the canonical directory as a permanent invariant.

## Repo layout

- `unreal 5.8/` — the ONLY Unreal project (Source, Config, Tools, Docs, Tests).
- `assets/` — raw cross-project assets (characters, creatures, maps, world
  generators). Binaries travel as `.b64` + `assets/decode_assets.py`.
- `src/`, `tests/` — engine-agnostic C++17 core + headless tests (CMake).
- `docs/` — project docs not specific to the UE project.

## Push protocol (local and remote diverged before; do not assume)

- Binary-safe push only: `gh_push.py` (never `push_files` for binaries).
- Before pushing: read the live remote HEAD, byte-compare touched files,
  layer the manifest onto the live HEAD, verify the ref afterward.
- `gh_push.py` always pushes — never run it to "verify".
- Never commit `Binaries/`, `Intermediate/`, `Saved/`, or decoded build output.

## Standing user directives

- UE 5.8 is canonical. Exact editor launch command is in `~/MEMORY.md`.
- Game stays free, donations optional; no microtransaction-shaped mechanics.
- CC0/original assets only; creature names per `assets/creatures/LEGAL_NAMES.md`.
- Never call Free Roam a "sandbox". Optimize for low-spec hosts.
- Schedules: none — work only on explicit user instruction (2026-10-15).
