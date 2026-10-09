# System Mapping — core → UE5

How each engine-agnostic core system lands in Unreal. The rule: **the core
owns simulation state and rules; UE owns presentation, input hardware,
and transport.** UE classes are thin wrappers — if you find game logic
creeping into a wrapper, move it into `../src` instead.

## Subsystems

| Core (`../src`) | UE wrapper | UE base class | Lifetime | Notes |
|---|---|---|---|---|
| `EventBus`, `GameClock`, `RNG`, `BeliefSystem` | `UCultBeliefSubsystem` | `UGameInstanceSubsystem` | Game instance | Session-global sim state. Created first. |
| `PowerSystem`, `ExertionSystem` | `UCultPowerSubsystem` | `UGameInstanceSubsystem` | Game instance | `ExertionSystem` binds lazily via `BindCult()` — it needs the world-scoped `CultManager`. `GameMode::Tick` binds it once. |
| `CultManager` | `UCultManagerSubsystem` | `UWorldSubsystem` | World | Fresh cult per match/level. Pulls bus/clock/rng from `UCultBeliefSubsystem`. |
| (sim tick order) | `ACultUlhuGameMode::Tick` | `AGameModeBase` | World | Beliefs → Cult → Power, every frame. Host-only: clients do not tick the sim. |

Subsystem init order guarantee: game-instance subsystems initialize before
world subsystems, so `UCultManagerSubsystem::Initialize` can safely
`GetGameInstance()->GetSubsystem<UCultBeliefSubsystem>()`.
// VERIFY IN EDITOR: confirm with a log line in each Initialize.

## Entities & characters

| Core | UE wrapper | Notes |
|---|---|---|
| `Entity` (+ `Building`, `Altar`, `Creature`, …) | `ACultUlhuEntityActor : AActor` | `PossessCoreEntity()` takes ownership. Host mirrors core→actor transform; `ReplicatedHp` for clients. |
| `CharacterDef` + `CharacterRegistry` | `ACultUlhuCharacter : ACharacter` + `UCultCharacterData : UPrimaryDataAsset` | Data asset per character (see `CharacterPipeline.md`). `ApplyCoreDef()` maps vitals/locomotion to UE components. |
| NPC packages (wave 12: `cultist_hooded`, `cultist_magus`, `civilian_villager`, `civilian_guard`, `civilian_laborer`) | Same `UCultCharacterData` pipeline | Humanoid packages under `assets/characters/`; validate with `character validate <id>` before importing. |
| Creature models (wave 12: 7 species via `ModelCatalog::creatureModels()`) + `Monstrosity`/`Dhole` entities | `ACultUlhuEntityActor` (or a creature subclass) | Models live in `assets/creatures/`; spawn headlessly with `spawn monstrosity [species]`, list with `bestiary`. |
| `RmbAbility` / `WaveOfDomination` | Game-module ability component (to be written) | Core state machine stays in C++; UE feeds it `RmbContext` (press/hold/release/mousemove/LMB) and reads `suggestedCasterState()` for animation. |

## Animation

| Core | UE |
|---|---|
| `AnimationStateMachine` + `AnimationClip` | `UCultUlhuAnimInstance : UAnimInstance` |
| `AnimationState` enum | `ECultAnimState` (mirror; keep in sync) |
| `mixamoClipName()` | Retargeted anim sequences in the AnimBP; state machine transitions driven by `AnimState` |
| One-shot casts/attacks | `AnimMontage`s via `PlayAttackMontage()` / `PlayCastWaveMontage()` / `PlayLaunchMontage()` |
| Victim `Levitated` anim | `SetVictimLevitated()` → AnimBP bool / pose snapshot; `ECultAnimState` now has all 12 core states (added `Levitated` in the wave-13 review) |

Mixamo packs are retargeted with the IK Retargeter onto the character
skeleton (see `CharacterPipeline.md`). The core's procedural clips are the
fallback when a package ships no `.canim` files.

## Input

| Core | UE |
|---|---|
| `InputManager` (abstract buttons/axes, `StaminaGauge`, `AbilitySlot`, combo timestamps) | Enhanced Input: `UInputAction`s + one `UInputMappingContext` (see `EnhancedInputMapping.md`) |
| `ComboTracker` | Fed by LMB `Started` events; game module resolves stage → anim + damage |

The core never touches OS input; the PlayerController translates Enhanced
Input events into `InputManager` state. This keeps the headless REPL driver
working unchanged.

## UI

| Core (data only) | UE |
|---|---|
| `CommandMenu::generateMenu/dispatch` | `UCultCommandMenuWidget : UUserWidget` (radial, runtime-built buttons) |
| `StatsPanel::gather` | `UCultStatsPanelWidget : UUserWidget` (Tab overlay) |

Widget Blueprints parent these classes; exact `BindWidget` names are in the
headers and `UmgWidgetSpecs.md`.

## Netcode

See `NetcodeDecision.md`. Short version: **UE replication + listen servers
replace the custom socket layer for gameplay**; `src/net` stays as the
headless-tested reference implementation and the design basis for the
Radmin lobby flow.

## Zone atmosphere → UE5 lighting (wave 32)

`ZoneDef::atmosphere` (authored per zone in the `.map` files via
`atmosphere <zone> fog=#rrggbb,density ambient=#rrggbb,intensity
sky=#rrggbb stars=0..1`) is the core's lighting brief for the binding.
The headless core cannot render, but it authors every number the
"beautiful but very dark" look depends on:

| Core field | UE5 target | Notes |
|---|---|---|
| `fogR/G/B`, `fogDensity` | `AExponentialHeightFog` (per-zone override volume) | Density 0..1 maps to the fog density curve; violet-black default `#0a0618` |
| `ambR/G/B`, `ambIntensity` | Sky-light / ambient cubemap tint + intensity | Ember shrine runs warm (`#3a1a0e`); caves near-black |
| `skyR/G/B` | `ASkyAtmosphere` zenith tint / night-sky material | Night is the default; day is the exception |
| `stars` | Starfield opacity in the sky material | `cavern_mouth` = 0.0 (rock overhead), `cult_base` = 1.0 |

Zone transitions: lerp the active atmosphere record over ~3s when the
camera crosses a zone boundary (the core exposes the camera's current
zone via `FreeRoamMode`; smooth the pop).

## What is intentionally NOT mapped yet

- `Dungeon`/`DungeonInstance` → level streaming / world partition mapping.
  Core side is done (`src/world/`; wave 13 added the 72×72 Vale of Pnath
  deep dungeon with hazards + relic vault — see `IntegrationGuide.md`
  §9); the UE side (sublevels streamed by entrance proximity, hazard
  trigger volumes) is still to be built.
- `ModelCatalog` / `MapLoader` → no C++ wrapper exists yet; the UE
  import path consumes them as data (see `IntegrationGuide.md` §9 and
  `ArtImportAndPerf.md`). If you want them callable from Blueprints,
  wrap the two static lookup tables in a `UBlueprintFunctionLibrary`.
- `BuilderAI` construction sites → actor spawning + progress UI.
- Save system → UE `USaveGame` bridge.
