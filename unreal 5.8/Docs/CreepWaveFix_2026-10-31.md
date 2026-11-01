# DOTGOO creep waves: standalone collision fix

The reported failure was reproduced using the desktop shortcut's actual launch path: UE 5.8, the canonical project, `/Game/Maps/L_MainMenu`, and `-game`. All three waves spawned on both teams and every lane, but every group recorded zero moving creeps. The editor-only automatic-wave test passed before the fix, so PIE alone did not establish that the desktop game worked.

## Cause and change

Arena decoration was spawned during `InitGame` with collision disabled but retained the static mesh actor's default `BlockAll` profile. Standalone component initialization restored that blocking profile. Decorative lane road/shoulder meshes and base discs then overlapped the creeps' capsules at their normal spawn height. The movement sweep correctly rejected an initially penetrating capsule, so every direction failed and the waves accumulated at base.

The arena mesh helper now explicitly sets **NoCollision** for decoration and disables collision on its owner actor. Solid geometry explicitly retains **BlockAll**. This preserves the intended state through standalone registration. The movement sweeps, real tower/nexus collision, lane routes, wave interval, composition, damage and speeds are unchanged. No map, model, animation or imported asset was replaced.

## Playing

Open the desktop **Cult-Ulhu** shortcut again to load the rebuilt module. The first wave deploys after the five-second warmup; subsequent waves deploy every 30 seconds, with siege creeps on every third wave. Waves march automatically on all three lanes, stopping to fight targets in range. Pausing stops match time and movement.

## Verification

The canonical editor target is built after project validation. The native automatic-wave regression observes the real first and third timed waves rather than manually spawning test creeps. It checks all six team/lane combinations, melee/ranged departure, siege departure and lane progress. The nexus collision regression separately checks walking/dashing against the solid objective and actual minion/hero detours around it.

The development-only **`-CultCreepWaveProbe`** follows the standalone menu into a normal DOTGOO match with bots active. It records real actor displacement and route progress across three waves, checks that siege creeps move, and verifies that decorative roads have no blocking collision. It writes `Saved/Automation/CreepWavesStandalone.json`, captures a frame under `Saved/Screenshots/`, and exits with a failure status if any team/lane fails. **`-CultCreepWaveDiagnose`** additionally produces short-run collision traces; that diagnostic run deliberately ends before the three-wave success criteria are met.

See [the verification record](CreepWaveResults_2026-10-31.json) for the reproduced failure and the rebuilt results. Logs and screenshots remain local under `Saved/`; build products are excluded from GitHub. This is a bounded wave/collision verification, not a certification of an entire multiplayer match or performance on other hardware.

The next requested Radmin work is recorded separately in [the multiplayer requirements](RadminMultiplayerRequirements.md).
