# Review Notes — offline UE5 binding review (Oct 7, 2026)

An offline inspection pass over `unreal/Source/` without a UE5 install.
Everything in this file is **unverifiable from here** — it needs the
owner's UE5 editor + a first compile to confirm. Code that was *clearly*
wrong was fixed directly (see the wave-13 review notes in the shift
summary); this file holds only the uncertain items.

## Compile-order checks (first-build blockers if wrong)

1. **`std::unique_ptr` members in `UCLASS`es**
   (`Source/CultUlhuCore/Public/Subsystems/CultBeliefSubsystem.h`,
   `CultPowerSubsystem.h`, `CultManagerSubsystem.h`,
   `Public/Actors/CultUlhuEntityActor.h`).
   Should compile — `<memory>` comes in via `CoreMinimal.h` and UHT
   ignores non-`UPROPERTY` members — but this is the single most likely
   thing to produce a surprise UHT/compiler error. If UHT complains,
   the fallback is a raw pointer owned in `Initialize`/`Deinitialize`
   (same lifetime semantics the code already has).

2. **`ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer())`**
   (`Source/CultUlhu/Private/CultUlhuPlayerController.cpp`, `BeginPlay`).
   This is the documented UE5 Enhanced Input pattern, but the static
   `GetSubsystem` template's exact form is version-sensitive — confirm
   the first compile picks the overload on `ULocalPlayer`, not the
   instance method.

3. **`const struct FInputActionValue&` forward declaration**
   (`Source/CultUlhu/Public/CultUlhuPlayerController.h`).
   Legal C++ (elaborated type specifier; the full type is only needed in
   the `.cpp`, which includes `InputActionValue.h`, and the methods are
   not `UFUNCTION`s so UHT never sees the type). If UHT or the compiler
   disagrees, add `#include "InputActionValue.h"` to the header.

4. **`UPROPERTY` specifier whitespace** (e.g.
   `UPROPERTY(ReplicatedUsing = OnRep_Vitals)`,
   `UPROPERTY(meta = (BindWidget))`).
   Left as-is on purpose: UHT tokenizes specifier lists, so spaces
   around `=` compile fine in practice. If the first build throws UHT
   parse errors on exactly these lines, remove the spaces
   (`ReplicatedUsing=OnRep_Vitals`) and move on.

## Behavior to confirm in-editor (not compile blockers)

5. **Subsystem init order assumption.** `UCultManagerSubsystem::Initialize`
   `check()`s that `UCultBeliefSubsystem` already exists (game-instance
   subsystems initialize before world subsystems — documented engine
   behavior). If it ever trips, the real question is *why* init order
   changed, not the assert. (Checklist item already in
   `InEditorVerification.md`.)

6. **`check(Core)` in `UCultPowerSubsystem::BindCult`.** A `check` is
   compiled out in Shipping; if `GetGameInstance()` can ever be null
   when `BindCult` runs (called from `ACultUlhuGameMode::Tick`, so it
   shouldn't be), prefer an early `return` + log so the failure is loud
   instead of a downstream null-deref.

7. **Replication defaults.** The binding sets `bReplicates = true` in
   constructors and registers both replicated floats with
   `DOREPLIFETIME` — standard. Not flagged: `AActor`/`ACharacter`
   `SetReplicateMovement` is off by default; the GameMode design ticks
   the sim host-side and mirrors transforms manually, which is
   intentional (host-authoritative), but confirm movement smoothing
   looks right on clients in the Radmin test before assuming the
   manual `SetActorLocation` mirror is enough.

8. **Fixed RNG seed (1234)** in `CultBeliefSubsystem::Initialize`.
   Deliberate (matches the headless driver for replay parity). Randomize
   per session for live play — owner's call, flagged in the checklist.

## Fixed during this review (for the record)

- `UCultStatsPanelWidget::Refresh` fetched the **world** subsystem
  `UCultManagerSubsystem` via `UGameInstance::GetSubsystem<>()`, which
  always returns null for world subsystems — stats overlay would have
  stayed empty. Now uses `GetWorld()->GetSubsystem<>()`.
- `ECultAnimState` / `FromCore()` were missing the core's 12th
  `AnimationState::Levitated` (victim-side wave state); it silently
  mapped to `Idle`. Added the enum value + case, and a `default:`
  returning `Idle` so future enum growth degrades loudly-in-code
  rather than as a compiler warning.
- Guidance comment referenced `SetReplicatingMovement` (UE3 name);
  UE5's method is `SetReplicateMovement`.
