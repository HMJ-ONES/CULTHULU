# CULT-ULHU — UE5 Binding (`unreal/`)

The Unreal Engine 5 presentation + platform layer for CULT-ULHU.
The game logic stays in the engine-agnostic C++17 core (`../src/`);
everything here is a thin UE5 wrapper: subsystems, actors, input,
animation hooks, UMG specs, and docs.

## ⚠️ Honest constraint — read first

**UE5 is not installed in the environment where this was written, so none
of this has been compile-verified.** It is written carefully to UE5
conventions (UE 5.4/5.5 era), but the owner must compile it in their UE5
editor. Every spot that needs in-editor confirmation is marked:

```cpp
// VERIFY IN EDITOR: <what to check>
```

`Docs/InEditorVerification.md` collects the full checklist.

## Layout

```
unreal/
  CultUlhu.uproject               # project file (set EngineAssociation to your install)
  Config/DefaultEngine.ini        # minimal; real settings are editor-authored (see docs)
  Source/
    CultUlhuCore/                 # UE module wrapping the C++17 core (no game code)
      CultUlhuCore.Build.cs
      Public/Private/...
    CultUlhu/                     # game module: GameMode, PlayerController
  Docs/
    SystemMapping.md              # core system -> UE subsystem table
    NetcodeDecision.md            # replication vs our socket layer (decision + port plan)
    IntegrationGuide.md           # project setup -> packaging, step by step
    CharacterPipeline.md          # assets/characters/<name>/ -> UE data assets
    EnhancedInputMapping.md       # owner's full binding map as Enhanced Input actions
    UmgWidgetSpecs.md             # CommandMenu + StatsPanel widget specs
    InEditorVerification.md       # the full verify-in-editor checklist
  Tests/
    validate_binding.py           # offline checks (no UE5 needed): uproject JSON,
                                  # binding-table completeness, VERIFY markers accounted
```

## Core-linking strategy (the one decision that matters)

`CultUlhuCore.Build.cs` adds `../../src` to the include path and links a
**prebuilt `cultulhu.lib`** built from the core with the *same MSVC
toolchain as your engine*:

```
cd <repo> && cmake -S . -B build-msvc -G "Visual Studio 17 2022" -A x64
cmake --build build-msvc --config Release
copy build-msvc\Release\cultulhu.lib unreal\lib\Win64\cultulhu.lib
```

ABI rule: the lib and the UE module must be built with the same
compiler/CRT settings. Never mix a MinGW-built `.a` with MSVC-built UE5.
(See `Docs/IntegrationGuide.md`.)

## Quick start for the owner

1. Install UE5 (5.4+; 5.5 recommended) + Visual Studio 2022.
2. Read `Docs/IntegrationGuide.md` end to end.
3. Open `CultUlhu.uproject`, compile, work through
   `Docs/InEditorVerification.md`.

## What lives where (30-second map)

| Concern | Core (`../src`) | UE binding (`unreal/`) |
|---|---|---|
| Beliefs, power, exertion, cult sim | `src/beliefs`, `src/power`, `src/exertion`, `src/cult` | `CultUlhuCore/Public/Subsystems/*` |
| Entities, characters | `src/entities`, `src/characters` | `CultUlhuCore/Public/Actors/*` |
| Animation states | `src/animation` | `CultUlhuAnimInstance` + retarget docs |
| Input bindings | `src/input` (abstract) | Enhanced Input (`Docs/EnhancedInputMapping.md`) |
| Command menu, stats panel | `src/ui` (data only) | `CultCommandMenuWidget`, `CultStatsPanelWidget` |
| Multiplayer | `src/net` (socket reference) | UE replication + listen servers (`Docs/NetcodeDecision.md`) |
| Character packages | `assets/characters/*` | `Docs/CharacterPipeline.md` |
