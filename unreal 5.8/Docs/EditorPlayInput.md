# Editor play-input protection — 2026-10-19

The reported yellow-bordered, Unlit editor-camera view matches Simulate In Editor rather than the possessed gameplay viewport. Unreal binds Alt+S to Simulate. In an existing PIE session that editor command requests a switch to SIE. Cult-Ulhu also uses Alt for its command cursor, so pressing S with that modifier can reach the global editor command handler while the game is in GameAndUI input mode. Plain S plus LMB remains ordinary backward movement and melee.

The primary game module now installs a temporary editor command-lookup filter for Simulate while the session is in PIE. It removes the filter when Play ends or the user intentionally ejects, and restores protection after deliberate repossession. F8 and toolbar actions remain available for intentional editor work. The filter does not modify saved shortcuts or user preferences, and is compiled only with editor support.

Development Editor build succeeded. The rendered `CultUlhu.Menu.EditorInputSafety` regression passed in `Saved/Logs/EditorInputSafety.log`. It sends real controller S and LMB key events, observes a real melee action and 753.6 cm of backward travel, dispatches Alt+S through Unreal's actual global play-world command list, combines that with the game command cursor and LMB, verifies possession and mouse-look restoration, intentionally ejects/repossesses with F8, and verifies shortcut availability and the original binding after Play ends. ArmyCommands also passed. One unrelated NPC movement-timestep warning occurred during the load/fixture sequence; neither test reported errors.

`Saved/Backups/BeforeEditorInputGuard.zip` preserves the prior module entry point. If already in the editor-camera view, F8 returns to the character while the play session is still active. The updated editor is reopened at the main-menu map.


## Camera bars and command alignment — 2026-10-21

Native Canvas controls now convert mouse positions through the active LocalPlayer view rectangle, matching the camera-constrained HUD. ALT drag selection, command and belief buttons, and match pause/result controls use this space consistently. A new click in a camera bar is ignored; a drag already in progress clamps to the view edge. World cursor aiming continues to use Unreal's full-viewport deprojection. The input helper queries live projection data because Unreal clears HUD Canvas after drawing.

`HUDCoordinatesFinal.log` passes actual ALT/LMB/B/ESC input for two-follower selection, Prepare for battle, belief Outcomes/selection/Close and paused Domination Resume with side bars, top/bottom bars and no bars. `HUDCoordinatesVerification.log` additionally passes EditorInputSafety and PopulationPresentation. Reports are in Saved/Reports/ArmyHUDCoordinates.csv and MatchHUDCoordinates.csv; rendered drag fixtures are Saved/Screenshots/ArmyDrag_*.png. This verifies the tested view shapes, not every resizing/platform configuration.
