> Historical prototype notes. The generic abilities and E recruitment below were superseded by [CanonicalGameplayIntegration.md](CanonicalGameplayIntegration.md).

# Playable Wilds first pass — October 16, 2026

## Controls

| Input | Action |
| --- | --- |
| WASD / arrows | Move relative to camera yaw |
| Hold either Shift | Sprint at 1.85 times walking speed |
| Space | Jump, targeting 40% of canonical creature height |
| Mouse | Look with captured game input |
| V | Toggle first/third person |
| Left mouse | Ground smash |
| Right mouse | Forward heavy smash (2.5 second cooldown) |
| Q | Aim a ranged blast with the camera (5 second cooldown) |
| F | Restore 300 health (12 second cooldown) |
| R | Wide cataclysm (20 second cooldown) |
| E | Recruit the nearest civilian within 50 m; otherwise toggle followers between attack/follow |
| Escape | Return to main menu |

Click the PIE viewport once if it does not have keyboard/mouse focus.
Shift+F1 is Unreal's default shortcut for releasing the mouse in PIE.
An in-game HUD shows controls, health, cooldowns, and follower orders.

## Implemented

- Preserve existing roster scale and ground-aware capsule spawning.
- Character-sized jumps, faster acceleration/braking, and scaled step height.
- Mouse axis bindings replace the extremely low sensitivity raw-delta polling.
  Gameplay controllers explicitly capture input and clear ignored look/move state.
- Terrain bounding-box inset and support checks restore the last grounded
  position when leaving the supported world. Jumping remains allowed.
- Player health, beast damage, healing, and return to the initial spawn on defeat.
- A visible placeholder vessel hides in first person.
- Prototype attacks damage destructible buildings, beasts, and civilians.
- Existing recognized city-structure meshes/instances are upgraded at runtime
  to per-building destructibles. Twelve seeded settlement structures are added
  near the initial spawn, with civilians and early enemies nearby.
- Additional enemy groups occupy the existing swamp and cave-approach areas.
  Beast movement samples ground and sweeps rather than teleporting horizontally.
- A prototype temple sits roughly 60 m behind the initial spawn, with eight
  recruited followers. Brown cylinders are civilians; purple cylinders are
  cultists. Beast stand-ins use spheres with eyes when no static mesh exists.
- Recruited followers enter the existing core CultManager roster. Follow/attack
  behavior is a basic Unreal prototype: attack orders select nearby beasts.
- Missing prototype materials can be created with EnsurePlayMaterials.py; it
  never overwrites existing materials. New actors are runtime-only; authored
  maps and existing art are not regenerated or overwritten.

## Deliberate limits of this first pass

This is not the final roster-specific ability system. Q/F/R currently share a
generic prototype kit; animation, VFX, tuning, and individual entity kits remain.
The temple is a protected visual rally point, not a completed base-building
or temple-ownership system. Followers have simple steering and cannot yet wage
strategic wars or navigate arbitrary cave networks. Creature placeholders are
not the imported skeletal creature assemblies. Civilian/follower models are
temporary shapes intended for replacement with authored art.

The larger procedural terrain, dramatic mountains/cliffs/pits/vales, actual cave
interiors, complete cities, separated rival temples, persistent world seeds,
navigation, and faction warfare remain further work. Current encounter positions
use the existing terrain; the added settlement uses a fixed deterministic seed.
Only recognized city structures are upgraded; decorative geometry is not broadly
made destructible, and the player's prototype temple is protected.

## Validation

The expanded CultUlhu.GroundSpawn regression checks original and smaller roster
scales, camera look rotation and mode switching, sprint speed, healing, Smash
damage, recruiting via E, walking, jumping/landing, and recovery after an excursion
outside the map. An initial run succeeded: 44.49 m walk distance over two seconds
and 12 m jump rise for the 30 m roster entity. Runtime reported 12 structures and
15 enemies. Final build/render verification results are recorded separately in
the task response and local Saved/Logs output.

Known engine issue: EditorToolset and ToolsetRegistry Python startup scripts
report missing editor-only AgentSkill/PythonTestRunner types in a `-game` run.
They do not prevent gameplay regression success; normal editor verification
uses the live MCP server. Engine files were not modified.

## Resumed editor verification

The native MCP Play request initially started a spectator. Inspection of Epic's
PlayLevel.cpp showed that Ctrl being held also requests spectator mode. Focusing
the editor and pressing/releasing Ctrl cleared its stale modifier state. A new
MCP Play request then spawned the actual CultUlhuPlayerPawn and initialized the
12 structures, 15 enemies, civilians, and eight followers. No gameplay override
was needed to force possession.

Real keyboard input verified V switching to first person; mouse motion inside
the viewport visibly rotated the camera. The rendered check exposed small HUD
text overlapping Unreal's development warnings, so the guide was moved to the
bottom with a larger font and responsive scaling. Placeholder eyes now hide
with the vessel body in first person. Existing unbuilt lighting/reflection
capture warnings remain; this pass does not rebuild or replace authored lighting.

The resumed gameplay regression succeeded again (Saved/Logs/PlayabilityVerified.log):
43.19 m movement over two seconds, 12 m jump rise, and all other assertions passed.
HUD sizing now follows viewport resolution, including high-resolution displays.
Only the HUD layout changed after this regression run; the editor target was
rebuilt for the final rendered verification.

Final visible-editor check: the scaled bottom HUD is readable, V hides the vessel
and eyes in first person, and the player spawns and possesses normally. Real
mouse motion visibly changed camera orientation. Escape uses Unreal's editor
shortcut to stop PIE before the pawn's menu binding receives it; menu travel
through that key was not verified in the editor. The visible editor was left
on Dark Wilds with PIE stopped, ready for a fresh Play test.
