# Ground collision and spawn fix — October 16, 2026

The pawn retains its existing roster height scaling. After scaling, before
movement ticks, it sweeps its full capsule down onto a walkable surface and
checks blocking clearance. It starts 2.4 cm above the capsule's contact position
instead of dropping from the PlayerStart altitude. Nearby bounded candidates
handle obstructed starts. If no safe ground exists, it logs an error, displays
a diagnostic, and disables movement instead of falling through the world.

Existing Dark Wilds terrain is recognized by mesh name `dark_wilds_ground`.
Its collision is set to complex-as-simple and BlockAll, with physics state
recreated before the spawn sweep. Other props and water are not changed.
The canonical Tools/BuildDarkWilds.py was restored from migration commit
842820e and now saves these collision settings when rebuilding terrain.
No existing map or Content asset was overwritten or regenerated.

Camera attachment offsets now use local, unscaled dimensions, removing a
second application of character scale. First-person pivot is inside the head
region rather than above the capsule. Movement speed and Smash scaling remain
unchanged. A pre-existing CultManagerSubsystem include-order error exposed by
the rebuild was corrected without changing subsystem behavior.

## Validation completed

- CultUlhuEditor Win64 Development: successful UE 5.8 build.
- Builder Python: syntax parsed successfully with Unreal's bundled Python.
- Actual L_DarkWilds loaded in an unattended game run with NullRHI.
- CultUlhu.GroundSpawn automation: SUCCESS.
- 30 m roster entity: retained scale 16.667; scaled half-height 1466.7 cm.
- 20 m roster entity: retained scale 11.111; found nearby clear terrain.
- Player walked 3398.9 cm in two seconds of game time and remained grounded.
- Smash reduced a nearby test destructible's health.

Runtime log: Saved/Logs/GroundSpawnRegression.log (local generated output).
Run the regression using UnrealEditor-Cmd.exe, canonical project path, and:

```
/Game/Maps/L_DarkWilds -game -nullrhi -unattended -nosound
"-ExecCmds=Automation RunTests CultUlhu.GroundSpawn"
"-TestExit=Automation Test Queue Empty"
```

These checks do not establish rendered camera quality, keyboard/mouse input,
menu travel, every mode-specific start, or all slopes/roster entries. Those
remain visual/manual checks. The test reported pre-existing missing optional
rubble/scorch assets; damage checks nevertheless passed. No builder regeneration
was run because it would replace level content.

Pre-edit source/config/project checkpoint was saved in the session's temporary
directory as CultUlhu-before-ground-fix-20261016-114120.zip. The active project
remains untracked, as discovered during onboarding; no GitHub sync was attempted.
