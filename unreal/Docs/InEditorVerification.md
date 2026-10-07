# In-Editor Verification Checklist

Every `// VERIFY IN EDITOR:` marker in `Source/` appears here, grouped by
area. Check each off in the owner's UE5. (An offline script,
`unreal/Tests/validate_binding.py`, confirms this list stays in sync with
the markers — it can't verify the engine behavior, only the bookkeeping.)

## Build & modules

- [ ] `Source/CultUlhuCore/CultUlhuCore.Build.cs`: the `cultulhu.lib` path
      resolves; lib was built with the same MSVC toolchain as the engine.
- [ ] `Source/CultUlhuCore/CultUlhuCore.Build.cs`: other platforms added if
      needed (Linux `.a` path is stubbed; Mac etc. as required).
- [ ] `Source/CultUlhuCore/CultUlhuCore.Build.cs`: exceptions/RTTI toggles
      reviewed (core is plain C++17; defaults should hold).
- [ ] `Source/CultUlhuCore/Private/CultUlhuCoreModule.cpp`: module loads at
      startup — no errors in the Output Log.
- [ ] `Source/CultUlhuCore/Public/Subsystems/CultBeliefSubsystem.h`:
      `std::unique_ptr` members inside the `UCLASS` compile cleanly.
- [ ] `Source/CultUlhuCore/Public/Actors/CultUlhuEntityActor.h`:
      `std::unique_ptr` member in the `AActor` compiles (no UHT complaints).
- [ ] `Source/CultUlhu/CultUlhu.Build.cs`: `OnlineSubsystem` modules added
      if/when the session-based lobby replaces direct-IP join.

## Subsystems & sim

- [ ] `Docs/SystemMapping.md`: log-line check of subsystem init order —
      `CultBeliefSubsystem` → `CultPowerSubsystem` → `CultManagerSubsystem`.
- [ ] `Source/CultUlhuCore/Private/Subsystems/CultBeliefSubsystem.cpp`:
      RNG seeding policy chosen (fixed 1234 = replays; randomize for live).
- [ ] `Source/CultUlhuCore/Private/Subsystems/CultPowerSubsystem.cpp`:
      `GetGameInstance()` valid in `BindCult()` (called from GameMode).
- [ ] `Source/CultUlhuCore/Private/Subsystems/CultManagerSubsystem.cpp`:
      world-subsystem init runs after game-instance init (`check(Core)`).
- [ ] `Source/CultUlhu/Private/CultUlhuGameMode.cpp`: `bUseSeamlessTravel`
      set as desired.
- [ ] `Source/CultUlhu/Public/CultUlhuGameMode.h`: set as the GameMode in
      project / map settings.
- [ ] `Source/CultUlhu/Public/CultUlhuGameMode.h`: `DefaultPawnClass` = your
      `ACultUlhuCharacter` Blueprint; `PlayerControllerClass` =
      `ACultUlhuPlayerController`.
- [ ] `Source/CultUlhu/Private/CultUlhuGameMode.cpp`: `bCultBound_` flips on
      the first tick (ExertionSystem bound to the world cult).

## Characters & animation

- [ ] `Source/CultUlhuCore/Public/Actors/CultUlhuCharacter.h`:
      `CharacterData` assigned on every character Blueprint.
- [ ] `Source/CultUlhuCore/Private/Actors/CultUlhuCharacter.cpp`:
      missing-`CharacterData` is loud (warning), never silent.
- [ ] `Source/CultUlhuCore/Private/Actors/CultUlhuCharacter.cpp`:
      `HealthComponent` (if added) tuned from `CoreDef_.maxHp`.
- [ ] `Source/CultUlhuCore/Private/Actors/CultUlhuCharacter.cpp`: sprint
      multiplier / jump Z velocity per character.
- [ ] `Source/CultUlhuCore/Private/Actors/CultUlhuCharacter.cpp`: stamina
      drain/regen (`StaminaGauge`) ticks on owner + host; `ReplicatedStamina`
      at low Hz.
- [ ] `Source/CultUlhuCore/Private/Actors/CultUlhuCharacter.cpp`: health /
      stamina bar widgets driven from `OnRep_Vitals`.
- [ ] `Source/CultUlhuCore/Private/Actors/CultUlhuEntityActor.cpp`:
      replication settings fit the net update budget.
- [ ] `Source/CultUlhuCore/Private/Actors/CultUlhuEntityActor.cpp`: death
      VFX / health bars hooked in `OnRep_CoreHp`.
- [ ] `Source/CultUlhuCore/Public/Data/CultCharacterData.h`: mesh skeleton
      matches the package's rig map.
- [ ] `Source/CultUlhuCore/Private/Data/CultCharacterData.cpp`:
      `CharacterDef` field names still match the core.
- [ ] `Source/CultUlhuCore/Public/Animation/CultUlhuAnimInstance.h`:
      `ECultAnimState` in sync with the core `AnimationState` enum.
- [ ] `Source/CultUlhuCore/Public/Animation/CultUlhuAnimInstance.h`:
      AnimBP has a state for every `ECultAnimState` value.
- [ ] `Source/CultUlhuCore/Public/Animation/CultUlhuAnimInstance.h`:
      montages exist per character; slots named as in the header.
- [ ] `Source/CultUlhuCore/Public/Animation/CultUlhuAnimInstance.h` +
      `Private/Animation/CultUlhuAnimInstance.cpp`: victim levitation visual
      (AnimBP bool / pose snapshot) implemented.
- [ ] `Source/CultUlhuCore/Private/Animation/CultUlhuAnimInstance.cpp`:
      `FromCore()` extended when the core enum grows.
- [ ] `Docs/CharacterPipeline.md`: Cthulhu Avatar fields hand-checked
      against `assets/characters/cthulhu_avatar/character.def` once.
- [ ] `Docs/CharacterPipeline.md`: retargeted Mixamo walk/run has no
      foot-slide (or core procedural clips used for locomotion).

## Input

- [ ] `Source/CultUlhu/Public/CultUlhuPlayerController.h`: all 11
      `UInputAction*` assigned in Blueprint defaults.
- [ ] `Source/CultUlhu/Private/CultUlhuPlayerController.cpp`: every
      `BindAction` matches the action assets (`Docs/EnhancedInputMapping.md`).
- [ ] `Source/CultUlhu/Private/CultUlhuPlayerController.cpp`: handler bodies
      implemented; full binding playtest passes.
- [ ] `Source/CultUlhu/Private/CultUlhuPlayerController.cpp`: mapping
      context priority correct vs UI contexts.
- [ ] `Docs/EnhancedInputMapping.md`: Alt+LMB chord fires once and does not
      double-fire `IA_Attack` (or the fallback check is in place).
- [ ] `Docs/EnhancedInputMapping.md`: authoring checklist complete
      (11 actions, context, assignments).

## UMG

- [ ] `Source/CultUlhuCore/Public/UI/CultCommandMenuWidget.h`: Widget
      Blueprint parents the class; layout per `Docs/UmgWidgetSpecs.md`.
- [ ] `Source/CultUlhuCore/Public/UI/CultCommandMenuWidget.h`:
      `BindWidget` names match (`MenuRoot`).
- [ ] `Source/CultUlhuCore/Public/UI/CultCommandMenuWidget.h`:
      `WBP_CommandMenuButton` exists with `SetLabel`.
- [ ] `Source/CultUlhuCore/Private/UI/CultCommandMenuWidget.cpp`:
      `MenuContext` filled from the Alt+LMB pick before `OpenMenu`.
- [ ] `Source/CultUlhuCore/Private/UI/CultCommandMenuWidget.cpp`: radial
      layout implemented per spec.
- [ ] `Source/CultUlhuCore/Private/UI/CultCommandMenuWidget.cpp`:
      `MenuModel_.dispatch()` called with the stored context, then close.
- [ ] `Source/CultUlhuCore/Public/UI/CultStatsPanelWidget.h`: `BindWidget`
      names match (`PowerText`, `KillsText`, `InsurrectionBar`,
      `BeliefGaugeList`, `KdaList`).
- [ ] `Source/CultUlhuCore/Private/UI/CultStatsPanelWidget.cpp`:
      `UCultManagerSubsystem` fetched correctly (world vs game instance).
- [ ] `Source/CultUlhuCore/Private/UI/CultStatsPanelWidget.cpp`: KDA
      tracker actually wired (empty table in multiplayer = bug).
- [ ] `Source/CultUlhuCore/Private/UI/CultStatsPanelWidget.cpp`: 12 belief
      gauge rows implemented.
- [ ] `Source/CultUlhuCore/Private/UI/CultStatsPanelWidget.cpp`: KDA rows
      implemented; hidden when empty.

## Gameplay slice

- [ ] `Docs/IntegrationGuide.md` §6: full Wave of Domination loop
      (wave → levitate → swing → drop/slam/launch → Onslaught feed).
- [ ] `Source/CultUlhu/Private/CultUlhuGameMode.cpp`: two-machine Radmin
      test (host `?listen`, client `open 26.x.x.x`); vitals/KDA/guages
      replicate; Server RPCs execute on host.
- [ ] `Docs/IntegrationGuide.md` §8: packaging checklist complete.
