# Integration Guide — from zero to a running UE5 build

This is the owner's runbook. It assumes a Windows machine with UE5 and
Visual Studio; every step that must be confirmed in the editor is also in
`InEditorVerification.md` (check items off there).

## 0. Prerequisites

- UE5 5.4+ installed (5.5 recommended; set `EngineAssociation` in
  `CultUlhu.uproject` to your version).
- Visual Studio 2022 with the "Game development with C++" workload and
  the MSVC v143 toolchain matching your engine build.
- CMake 3.16+ (to build the core static lib).
- Radmin VPN installed (for multiplayer tests later).
- This repo cloned locally; you will open `<repo>/unreal/CultUlhu.uproject`.

## 1. Build the core static lib (MSVC)

The UE module links `cultulhu.lib` — it must be built with the **same
MSVC toolchain as your engine** (ABI rule; a MinGW `.a` will not link).

```bat
cd <repo>
cmake -S . -B build-msvc -G "Visual Studio 17 2022" -A x64
cmake --build build-msvc --config Release
mkdir unreal\lib\Win64
copy build-msvc\Release\cultulhu.lib unreal\lib\Win64\cultulhu.lib
```

Notes:

- The core CMake already excludes the headless driver from the lib
  (`src/driver/` is filtered out) — the lib is pure sim, no `main()`.
- Rebuild the lib whenever `../src` changes; the UE build will not do it
  for you. (A future step: a pre-build script that does this automatically.)
- Debug editor builds want a Debug lib too (`--config Debug` →
  `unreal/lib/Win64/cultulhu_d.lib` + a `Target.Configuration` switch in
  `CultUlhuCore.Build.cs`).
  // VERIFY IN EDITOR: add the Debug-lib switch before you need it.

## 2. Generate project files & first compile

1. Right-click `unreal/CultUlhu.uproject` → **Generate Visual Studio project files**.
2. Open the `.sln`, build the `CultUlhuEditor` target (Development Editor).
3. Open the project in the UE editor.
// VERIFY IN EDITOR: both modules (`CultUlhuCore`, `CultUlhu`) load —
check the Output Log for module-load errors. If `cultulhu.lib` isn't
found, the linker error names it; fix the path in `CultUlhuCore.Build.cs`.

## 3. Wire the subsystems

Subsystems need no manual wiring — `UGameInstanceSubsystem`s instantiate
with the game instance, `UWorldSubsystem`s with the world. But confirm:

1. Open the Output Log; add temporary `UE_LOG` lines in each subsystem's
   `Initialize()` and confirm order: `CultBeliefSubsystem` →
   `CultPowerSubsystem` → (world) `CultManagerSubsystem`.
2. `ACultUlhuGameMode::Tick` binds `ExertionSystem` to the world cult via
   `BindCult()` on the first tick — confirm `bCultBound_` flips (breakpoint
   or log).
3. Set `ACultUlhuGameMode` as the GameMode for your test map
   (World Settings → GameMode Override, or project Maps & Modes).

## 4. Player character & input

1. Follow `EnhancedInputMapping.md`: create the 11 `UInputAction` assets +
   `IMC_CultUlhu`, assign them on an `ACultUlhuPlayerController` Blueprint.
2. Follow `CharacterPipeline.md` for the Cthulhu Avatar: import FBX (or
   skip the mesh — logic-only works), create `DA_cthulhu_avatar`, AnimBP,
   character Blueprint.
3. Set the character Blueprint as `DefaultPawnClass`, the controller
   Blueprint as `PlayerControllerClass` (GameMode defaults).
4. PIE: walk (WASD), jump (Space), sprint (Shift — watch stamina),
   Q/F/R, E near an altar prop, Tab overlay, LMB combo, RMB wave
   (no victims yet — see step 6), Alt+LMB menu.
// VERIFY IN EDITOR: every binding in EnhancedInputMapping.md, one by one.

## 5. UMG

1. Create `WBP_CommandMenu`, `WBP_StatsPanel`, `WBP_CommandMenuButton`
   per `UmgWidgetSpecs.md` (exact `BindWidget` names!).
2. Add the widgets to the viewport from the PlayerController or HUD
   (create on `BeginPlay`, keep references).
3. PIE: Tab toggles the stats panel (gauges move as exertion ticks);
   Alt+LMB opens the radial menu at the cursor.

## 6. First gameplay slice (single-player)

Suggested order — each is independently testable:

1. Spawn civilians (`ACultUlhuEntityActor` with core `Civilian`s) in the map.
2. RMB (no target): the wave travels; victims levitate; hold RMB and move
   the mouse to swing them; release to drop (they live, staggered).
3. RMB with a targeted entity: 2.5 s stun, no wave.
4. LMB while holding victims: launch projectiles (2× damage).
5. Slam a victim into a building prop: heavy damage + victim death.
6. Watch the Tab overlay: Onslaught exertion climbs with the kills.
   // VERIFY IN EDITOR: the full Wave of Domination loop, end to end.

## 7. Listen-server multiplayer over Radmin

Per `NetcodeDecision.md`:

1. Package or PIE-host: the host clicks **Host** (calls
   `ACultUlhuGameMode::HostListenServer`), notes their Radmin IP
   (26.x.x.x — the core `RadminNet` adapter detection can print it).
2. Clients join the same Radmin network, then in-game **Join** with the
   host IP → `open 26.x.x.x` (default port 7777).
3. Lobby: ready-up + 5v5 team assignment (UMG, wave 9+), then `ServerTravel`.
4. Confirm: clients see replicated vitals, KDA, belief gauges; Server RPCs
   (directives, commands) execute on the host.
   // VERIFY IN EDITOR: two-machine Radmin test before calling netcode done.

## 8. Packaging checklist

- [ ] `cultulhu.lib` (Release) built with the shipping toolchain and in place
- [ ] All `// VERIFY IN EDITOR:` items in `InEditorVerification.md` checked off
- [ ] Maps cooked: no Blueprint compile errors, no missing `BindWidget`s
- [ ] Input: packaged build re-test of all 11 actions (packaging can expose asset-reference issues PIE hides)
- [ ] Radmin two-machine test from packaged builds
- [ ] Core lib rebuilt from a clean tree (no stale objects)

## 9. Waves 11–13: art, NPCs/creatures, dungeons, pipeline hardening

Core content added after the wave-8 binding was written. None of it
changes the binding architecture — it plugs into the same seams.

**Wave 11 — CC0 art pass.** `assets/world/` holds the CC0 (Kenney) art;
binaries travel as `*.glb.b64` in git, so run
`assets/decode_assets.py` after cloning. Two core-side tables drive the
UE import:

- `src/assets/ModelCatalog.h` — `worldModels()` (logical name →
  repo-relative `.glb` under `assets/world/`) and `creatureModels()`
  (species/role key → `assets/creatures/` or
  `assets/characters/<pkg>/model.glb`). Import the meshes, then use
  these tables — not hardcoded paths — when wiring placements.
- `src/world/MapLoader.h` — `MapData MapLoader::load(path)` parses
  `assets/maps/ruined_city.map` (90 placements, 4 zones: x y z, rotY,
  scale, zone per prop). The driver `mapinfo` command dumps the same
  data headlessly; an Editor Utility script can read the `.map` file
  and spawn `UInstancedStaticMeshComponent` instances per §"Instancing"
  in `ArtImportAndPerf.md`.

**Wave 12 — NPCs + creatures.** Five humanoid character packages under
`assets/characters/` (`cultist_hooded`, `cultist_magus`,
`civilian_villager`, `civilian_guard`, `civilian_laborer`) follow the
same `character.def` → `UCultCharacterData` pipeline as the Cthulhu
Avatar (see `CharacterPipeline.md`). Seven creature models
(`pale_wight`, `ossified_brute`, `charnel_imp`, `skittering_ghoul`,
`wraith`, `risen_dead`, `dagon_spawn`) live under `assets/creatures/`
and are keyed in `ModelCatalog::creatureModels()`; the driver
`bestiary` command lists them with tri counts and file-presence checks,
and `spawn monstrosity [species]` exercises them headlessly. UE-side:
NPCs get `ACultUlhuCharacter` Blueprints; creatures/monstrosities
(`cultulhu::Monstrosity`, incl. the `Dhole`) get `ACultUlhuEntityActor`
(or a creature subclass) with `PossessCoreEntity()`.
Legal note: names follow `assets/creatures/LEGAL_NAMES.md`
(public-domain Lovecraft only — see the repo memory/LEGAL guideline);
don't invent new Mythos names in UE-side assets.

**Wave 13 — Vale of Pnath deep dungeon.** `src/world/ValeOfPnath.h`
(`class ValeOfPnath : public DungeonInstance`, 72×72, up to 20 rooms)
adds depth/dread-scaled hazards (abyss pits, maddening whispers, dhole
tunnels with telegraph→ambush) plus a relic vault + guardian at the
bottom; `dungeon pnath [seed]` in the driver generates one headlessly.
UE mapping: dungeon instances → sublevels streamed by entrance proximity
(see `ArtImportAndPerf.md` "LOD & culling"); hazards map to trigger
volumes + the existing damage/fear systems. The `Dhole` uses an
intentional placeholder mesh slot — no CC0 dhole model exists.

**Shift 2 — character pipeline hardening.** The core now validates
packages deeply: `CharacterValidator::validateDeep()` returns a graded
report (`CLEAN` / `OK WITH WARNINGS` / `INVALID`), exposed headlessly
via `character validate <id>`. Run it on every package before importing
to UE. Related: the rig mapper knows Mixamo / Blender / Rigify bone-name
aliases (per-bone diagnostics in the report); the core ships procedural
clip fallbacks for all 9 gameplay anim states
(`src/animation/ProceduralClips.h`: Idle, Walk, Run, Attack, Death,
Cast, Stunned, Channel, CastWave — Levitate/Launch/Levitated stay
unbound, driven by Wave of Domination logic); and
`docs/bring_your_own_model.md` is the owner's Blender→FBX→package→validate
loop write-up. New minimal example package: `assets/characters/echo_of_the_void/`.

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| Link errors mentioning `cultulhu` symbols | Stale/missing `.lib`, or MSVC version mismatch — rebuild the lib (step 1) |
| Subsystem `check(Core)` crashes | Init order; confirm game-instance subsystems exist before the world one |
| Character doesn't move | `CharacterData` null on the Blueprint, or `MaxWalkSpeed` not applied — see `ApplyCoreDef` |
| Alt+LMB also attacks | Chord misconfigured — see `EnhancedInputMapping.md` fallback |
| AnimGraph stuck in Idle | `ECultAnimState` state missing in the AnimBP, or `SetCoreState` never called |
| Stats panel empty in multiplayer | KDA tracker not wired into `Refresh()` (see header comment) |
