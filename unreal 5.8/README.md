# CULT-ULHU — Unreal 5.8 project

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
