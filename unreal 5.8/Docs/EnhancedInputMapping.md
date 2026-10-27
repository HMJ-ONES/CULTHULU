# Current controls and optional Enhanced Input

The native pawn works immediately with keyboard and mouse; no Input Action assets or Blueprint setup are required. The controller also supports optional Enhanced Input references. Both paths share press/release routing, so the same physical press cannot trigger an action twice.

| Action reference | Default control | Current behavior |
| --- | --- | --- |
| `IA_Move` | WASD / arrows | Camera-relative movement. Optional Axis2D uses X right, Y forward; physical movement keys take precedence. |
| `IA_Jump` | Space | Existing jump and release behavior. |
| `IA_Sprint` | Shift | Hold to sprint; a short tap requests the existing directional dash. A held sprint does not dash. |
| `IA_AbilityQ` | Q | Original selected character Q, with existing gates and costs. |
| `IA_AbilityF` | F | Original selected character F. |
| `IA_AbilityR` | R | Original selected character R. |
| `IA_Interact` | E | Existing interaction, including competitive invocation rules. |
| `IA_StatsOverlay` | Tab | Toggle current stats display. |
| `IA_Attack` | LMB | Existing melee, held-minion launch, or ALT selection according to current pawn state. |
| `IA_HeavyAttack` | RMB | Existing character/mode RMB action and release. |
| `IA_CommandMenu` | Hold Left ALT | Release cursor and select orders; release ALT to return to mouse look. LMB remains the pawn's existing context-sensitive command click. |

Mouse movement aims the camera. V switches first/third person. B opens beliefs/commands where the current mode permits them. Competitive Escape opens pause controls; Free Roam Escape returns home. Existing G recall and Enter rematch controls remain.

## Optional action assets

Assign the matching `UInputAction` properties and `MappingContext` on `ACultUlhuPlayerController` only when extending the input profile. Use Boolean values except Axis2D for move. Discrete actions bind Started, Completed and Canceled; movement binds Triggered, Completed and Canceled. Sprint state enters the same tap/hold decision used by keyboard polling. Keep the command action as held ALT, not the obsolete ALT+LMB chord.

The controller forwards to `ACultUlhuPlayerPawn`, which owns all actual combat, interaction, movement and control-state decisions. It does not bypass the kit runtime, add a second stamina drain, or substitute direct core calls. Missing optional references leave the zero-asset keyboard/mouse path active. Remapping menus and a complete gamepad profile are separate work.

Native verification: `CultUlhu.Input.EnhancedAndFallback` injects real Enhanced Input values, combines them with a legacy key, and checks one toggle per press, actual movement, completed release and held-sprint dash safety. `CultUlhu.Menu.EditorInput` retains the real S+LMB and ALT+S editor-shortcut regression.
