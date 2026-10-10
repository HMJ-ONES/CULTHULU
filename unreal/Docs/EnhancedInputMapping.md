# Enhanced Input Mapping — the owner's full binding scheme

One `UInputMappingContext` (`IMC_CultUlhu`) holding every action below.
Create the `UInputAction` assets in `/Game/Input/Actions/`, the context in
`/Game/Input/`. Value types and triggers are the load-bearing details —
// VERIFY IN EDITOR: each row once, in PIE, with the "show debug" input
// debugger (Enhanced Input has one; use it).

## Actions

| Input Action | Key(s) | Value type | Trigger | Core touch-point |
|---|---|---|---|---|
| `IA_Move` | WASD | Axis2D (`FVector2D`) | — (continuous) | `InputManager` move vector → `AddMovementInput` (camera-relative) |
| `IA_Jump` | Space | Digital (bool) | `Started` | jump buffer + coyote stubs (`src/input`) → `ACharacter::Jump()` |
| `IA_Sprint` | Left Shift | Digital (bool) | `Started` / `Completed` | `StaminaGauge`: drain 18/s while held, regen 12/s, exhausted at 0 → recover at 30; sprint multiplies `MaxWalkSpeed` |
| `IA_AbilityQ` | Q | Digital | `Started` | `AbilitySlot` (Q): cooldown + stamina gates → `SpellDef` effect |
| `IA_AbilityF` | F | Digital | `Started` | `AbilitySlot` (F) |
| `IA_AbilityR` | R | Digital | `Started` | `AbilitySlot` (R) |
| `IA_Interact` | E | Digital | `Started` | `findInteractable()` radius 3.0 m (altar/captive/relic/door) → `setInteractTarget()` |
| `IA_StatsOverlay` | Tab | Digital | `Started` | toggles `UCultStatsPanelWidget` visibility |
| `IA_Attack` | Left Mouse | Digital | `Started` | `ComboTracker` click timestamps → stage → anim state + damage × mult + CC |
| `IA_HeavyAttack` | Right Mouse | Digital | `Started` / `Completed` | per-character RMB kit. Cthulhu Avatar = `wave_of_domination`: press = cast/hold, mouse-move = swing, LMB during hold = launch, release = drop |
| `IA_CommandMenu` | **Alt + Left Mouse (chord)** | Digital | `Started` | raycast from cursor → `MenuContext` → `UCultCommandMenuWidget::OpenMenu()` |

## Notes

- **Chorded action:** `IA_CommandMenu` uses Enhanced Input's chorded-action
  support (Alt as the chord key + LMB). If chorded actions misbehave in
  your engine version, fall back to a manual check: on LMB `Started`, test
  `IsInputKeyDown(EKeys::LeftAlt)` in the PlayerController.
  // VERIFY IN EDITOR: the Alt+LMB chord fires exactly once per press and
  // does NOT also trigger IA_Attack.
- **LMB double duty:** during Wave of Domination's hold phase, LMB means
  *launch victims*, not *melee*. The PlayerController must route LMB by
  RMB-kit state (`WaveOfDomination::phase()`), not by a static binding.
- **Stamina numbers** (tunable; also in the core README): 100 max, drain
  18/s, regen 12/s, exhausted→recover at 30.
- **Camera-relative movement:** `IA_Move` is fed through the control
  rotation (yaw only), standard third/first-person template pattern.
- **Tab vs UI focus:** the stats overlay is a toggle, not hold. Make sure
  the widget doesn't steal keyboard focus (set `Is Focusable = false`).
- **Gamepad (later):** leave the mapping context open for a gamepad
  profile; the core consumes abstract states so only the context changes.

## Mapping context contents (authoring checklist)

- [ ] All 11 actions above exist as assets with the value types listed
- [ ] `IMC_CultUlhu` maps each action to its key(s) with priority 0
- [ ] `ACultUlhuPlayerController.MappingContext` references `IMC_CultUlhu`
- [ ] Each `UInputAction*` property on the controller is assigned in defaults
- [ ] Alt+LMB chord does not double-fire `IA_Attack` (or the fallback check exists)
