# CULT-ULHU â€” Unreal 5.8 project

`CultUlhu.uproject` in this directory is the active project. The gameplay core
lives in `../src`; original character definitions and animation clips live in
`../assets/characters`. Retired project copies are not build targets.

## Build and open on Windows

Use the installed Unreal 5.8 engine and Visual Studio 2022 MSVC toolchain.
From the repository root in PowerShell:

```powershell
$projectFile = (Resolve-Path 'unreal 5.8/CultUlhu.uproject').Path
$engineDir = 'C:/Program Files/Epic Games/UE_5.8'
& "$engineDir/Engine/Binaries/ThirdParty/Python3/Win64/python.exe" 'unreal 5.8/Tools/ValidateCultUlhu.py'
& './unreal 5.8/Tools/BuildCanonicalCore.ps1' -Test
& "$engineDir/Engine/Build/BatchFiles/Build.bat" CultUlhuEditor Win64 Development "-Project=$projectFile" -WaitMutex -NoHotReloadFromIDE -NoUBA -MaxParallelActions=2
& "$engineDir/Engine/Binaries/Win64/UnrealEditor.exe" $projectFile
```

Stop if validation or a build fails. Open the canonical project explicitly;
its startup map is `/Game/Maps/L_MainMenu`. The mode menu leads to character
selection; initial beliefs apply only to Free Roam. See `Docs/EditorPlayInput.md`
for the editor input fix and `Docs/ArmyCommands.md` for ALT army commands.

The core library is generated into `lib/Win64/cultulhu.lib` by
BuildCanonicalCore.ps1. Do not substitute the old root CMake build: the canonical
wrapper excludes standalone drivers and uses the engine-compatible runtime.

## Standalone checks

A Development Game executable has compiled and a Windows cook/stage has succeeded.
The standalone HumanAnimationFiles and StagedHumanPoses automation checks execute
without an adjacent source-assets tree. This proves the tested humanoid assets,
activity poses and mode-start metadata; it does not establish release readiness.
See `Docs/Checkpoint_2026-10-21.md` and `../docs/PRODUCTION_AUDIT_2026-10-20.md`
for current results and remaining work.
`Docs/WorldPresentation.md` records the later Free Roam dusk/readability pass and
its editor render verification. The newer dusk spawn-area image and shared human
meshes now also pass the scoped independent executable checks below.
`Docs/PopulationScheduling.md` records the later NPC decision optimization, native
movement/combat checks and measured CPU limits; overall performance remains inadequate.
`Docs/HumanMeshRendering.md` describes the shared humanoid body meshes, original-art
verification and measured component/frame-time reduction. The fresh independent
Game run passes mesh integrity, a live comparison and original activity poses;
Docs/HumanMeshStandaloneResults.json records scope, timings and remaining warnings.
`Docs/SettlementPopulation.md` records the later correction for civilians buried by
city generation, occupied-footprint protection and its scoped Editor verification.

Build.cs stages the release character definitions, fonts and all 48 NPC animation
clips. Do not hand-copy or commit generated CharacterDefinitions/CharacterAnimations
inside Content. Authored maps, imported meshes, textures and materials are tracked.
PrepareRuntimeAnchors.py preserves the existing map transforms while saving runtime
spawn/camera identity. PrepareNaniteMaterials.py persists the shader usage needed
by the existing rocks, trees and crystals in packaged builds.

## Storage and development status

Keep one working editor build and only necessary temporary standalone output.
Binaries, Intermediate, Saved, DDC, static libraries, archives and duplicate project
copies are excluded from GitHub. Source, configuration, tools, tests, original
assets and authored Content are the durable inputs. See `../docs/STORAGE_AND_BACKUP.md`.

The game remains a prototype. Final hero models/animations, broader visual polish,
sustained frame rate, multiplayer and human fun/balance evaluation are outstanding.
Successful automation is evidence for its specific checks, not a claim of
Overwatch-level presentation or Paladins-level match quality.

`Docs/HUDReadability.md` records measured header contrast, responsive ability-card
rows, movement-cue separation, exact-size capture correction and input checks.
These newer HUD edits still need a fresh independent executable verification.

`Docs/MatchCameraFraming.md` records the mirrored spawn-row change and native
camera/input checks. It clears the reproduced Mi-Go ally obstruction at match
entry while preserving body height and real camera wall collision.

`Docs/MatchEvents.md` records separate elimination feedback, preserved objective
announcements, measured competitive message layout and bounded objective labels.

`Docs/SummonCombatCredit.md` records consistent summoner damage/kill/XP credit,
local hit feedback, same-faction validation and the original healing passive.

`Docs/MatchSurfaces.md` contains paired arena renders and scoped render-cost
measurements for world-aligned stone and distinct natural MOBA ground.

`Docs/JungleCliffs.md` records procedural cliff silhouettes, matching collision,
preserved access routes, paired renders and scoped render-cost measurements.

`Docs/BotThreatResponse.md` records delayed, directional bot defensive reactions,
cover and landing checks, and physical dodge versus stationary-projectile controls.

`Docs/HeroReadability.md` records visible competitive hero identity/health plates,
responsive placement, actual-view projection and paired render/occlusion evidence.

`Docs/ObjectiveSolidity.md` records physical MOBA objective bodies, minion routing
past allied structures and full-size hero/minion detour verification.

`Docs/WardArchitecture.md` records the compact stone lane/nexus meshes, preserved
combat/collision, grounded remains, paired renders and six native Editor checks.
The two new assets total 193 KB; no standalone package was generated for this pass.

`Docs/MatchBackdrop.md` records the source-generated exterior rock range in both
competitive modes, matched views and six scoped native checks. No assets are downloaded.

`Docs/ImpactPresentation.md` records open hit/shock effects, the corrected hit-size
clamp, bounded directional sparks and native feedback/physical-combat checks.

`Docs/BotMeleeReach.md` records shared physical swing dimensions, size-aware bot
reach/approach decisions and corrected-baseline/native match evidence.

`Docs/BotAbilityOpportunity.md` records actual-shape bot aura/cone opportunities,
resource preservation, alternate victims/cover and scoped native evidence.

`Docs/CharacterSelection.md` records the fitted sixteen-entity roster, distinct
original ability rows, fixed mode guidance, paired renders and native layout
checks across three modes and four viewport sizes.

`Docs/BotHeavyOpportunity.md` records actual-arc bot heavy decisions, a reproduced
moving-tip melee cover bypass, the physical contact correction and native checks.

`Docs/VesselSurface.md` records the small original placeholder-body material,
matched native views, geometry preservation and scoped render-cost measurements.
