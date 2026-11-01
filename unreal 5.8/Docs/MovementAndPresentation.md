# Movement and presentation — 2026-10-19

Native hero walking speed is now 125% of the preceding build. The source move-speed differences between entities and the character-height scale remain intact. Holding Shift still sprints at 1.85 times the new walk speed; a short Shift tap still dashes on release. Dash force derives from walk speed, so dash travel also increases by 25%. Both allied and opposing hero bots use the same change. Jump height, stamina costs, cooldowns, and canonical character-definition files are unchanged by this pass.

## Visual system

`UI/CultUIStyle` provides the shared charcoal, bone-white, sea-green, and antique-gold palette. Minimal two-pixel-radius buttons use thin outlines, clear pressed and disabled states, a gold hover border, and a thicker persistent selection border. The menu's original vector occult seal is drawn in Slate rather than adding a raster texture dependency.

Typography uses Roboto Regular and Medium with Roboto Bold Condensed display headings. The three font files and the redistribution license are in `Content/UI/Fonts`, and are explicitly registered as NonUFS runtime dependencies. The editor target receipt was inspected and contains all four files. This verifies staging registration, not a full Shipping cook/package.

The mode menu now has a compact, left-aligned hierarchy and restrained occult ornament. Character selection separates the released sixteen-entity roster from a scrollable moveset panel. Kit names and cooldowns come from the original definitions; competitive health and Cthulhu's explicit MOBA RMB variant are described accurately. Initial beliefs retain the required three-choice gate in Free Roam only.

The combat HUD uses the same font family and palette, larger key labels, quieter ability frames, and a solid notification backing. Scoreboards, pause/results actions, ALT orders, Free Roam statistics, and belief selection share the typography. ALT orders have hover feedback, more vertical spacing, and an opaque backdrop to separate them from combat widgets underneath.

The in-game B panel now measures text with the actual rendering font and adapts between one- and two-column belief lists. Rules and real Outcomes use separate scrollable tabs. All original belief rules, adoption actions, and cult actions remain accessible. The world continues running while the panel is open. Larger render targets scale the panel and its mouse hit areas together.

## Preservation and checks

`Saved/Backups/BeforeMovementUIPolish.zip` preserves the preceding game-module source and configuration. No original character-definition files or character heights were changed. The four future-release entities remain excluded from selection.

The Development Editor build passed. `Saved/Logs/MovementUIPresentation.log` passed Select, InitialBeliefs, ArmyCommands, Beliefs and SkillPreview. Rendered inspection exposed the old B panel's row-height problem in an 888x500 editor window; its subsequent responsive layout and the solid command backdrop are checked separately in the final run.

## Final verification

- `MovementUIFinal.log` passed ArmyCommands, Beliefs, DominationMatch, MobaMatch and MobaMechanics after the responsive overlay changes. The MOBA observation recorded 9/9 assigned-lane entries and 665.6 real hero damage in seventy seconds. ArmyCommands recorded two existing movement-timestep warnings during a fixture/load hitch; no tests reported errors.
- `MovementUIPolishVerified.log` passed Select, DominationMatch and MobaMechanics after the final left-alignment, target-label placement, short RMB hint, and measured notification wrapping changes, with no automation errors or warnings. Screenshots were inspected, including the small 888x500 editor window / 870x330 game viewport.
- `MovementUITraversal.log` passed CombatFeel and GroundSpawn with no automation errors or warnings: 5477.6 cm of observed walking, 1187.9 cm jump rise, genuine sprint-speed increase, grounded landing, boundary recovery, camera switching, real melee/dash collision and evade-window checks.
- Ten distinct suites passed across these runs: Select, InitialBeliefs, SkillPreview, ArmyCommands, Beliefs, DominationMatch, MobaMatch, MobaMechanics, CombatFeel and GroundSpawn. These are local Editor/runtime checks, not a full Shipping build or a claim of a complete natural-length match.

Rendered previews are in `Saved/Screenshots`: `PolishedModeSelect.png`, `CodexCharacterSelect.png`, `InitialBeliefs.png`, `CultBeliefsReview.png`, `ArmyCommands.png`, `DominationHUD.png` and `MobaMinionHeld.png`. The final menu uses aligned content; target labels sit clear of the objective instructions; RMB hints and notifications fit their panels. The editor is reopened at `/Game/Maps/L_MainMenu` for playtesting.
