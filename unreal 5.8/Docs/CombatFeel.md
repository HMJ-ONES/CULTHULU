# Combat feel and readability pass

This pass improves the exchange of attacks, movement, and feedback while keeping the original twenty character definitions, Q/F/R abilities, right-click identities, damage multipliers, costs, and cooldowns. It builds on the physical hit boxes in `SkillCombatReview.md` and the army commands in `ArmyCommands.md`.

## Playing

- Click **LMB** for aimed melee. The interval is now 0.62 seconds, including the original 0.12-second windup and 0.20-second active sweep. Consecutive swings alternate sides. Click during the final 0.18 seconds of recovery to buffer one follow-up; holding LMB does not repeatedly attack. The original three-stage flurry is retained: normal hit, 1.15x hit with a 1s slow, then 1.50x hit with a 0.75s stun, with the source 1.2s chain window.
- **Shift + a direction** dashes; held Shift still sprints. Dashing cancels the current arm swing and queued melee, so it cannot carry a damaging arm across the full dash distance. The existing stamina cost, cooldown, and short evade window remain.
- Watch a creature's committed swing, dodge or step out, then attack during recovery. Visible attack warnings show the actual windup countdown and projected area. Damage still uses the original collision sweeps and cover checks, not the warning artwork.
- Normal contact shows floating damage and a hit marker; a real kill has a distinct elimination marker. Damage amounts are actual health removed, capped at remaining health rather than inflated overkill. A prevented hit during the evade window shows **DODGED**. Real incoming damage briefly highlights screen edges and indicates direction when its source is known.
- The last struck target's health remains readable briefly after the crosshair moves away. A small center-bottom recovery bar shows when melee is available again.

## Enemy behavior

Fast ghouls and imps angle into melee rather than all charging the exact center of the vessel. All creatures stop pushing into the player's capsule once they reach contact range. That distance uses the capsule cross-section at their arm height: small creatures approach the legs of giant entities before committing, rather than swinging outside reach of the narrower lower body. Creature ground placement sweeps the full capsule, and movement tries small steering angles around obstacles. Attacks retain a fixed direction during windup and strike, followed by recovery without movement or retargeting.

| Creature | Windup | Active | Recovery |
|---|---:|---:|---:|
| Imp / ghoul | 0.40s | 0.18s | 0.50s |
| Pale wight | 0.55s | 0.18s | 0.80s |
| Ossified brute | 0.72s | 0.18s | 1.05s |

The next swing also has a 0.18-second gap after recovery. Existing health remains 135 / 270 / 1350 / 2025, preserving the one / two / ten / fifteen baseline-hit tiers before source combos and belief bonuses.

Real damage accumulates poise pressure for at most two seconds between contacts. Minor creatures break at 135 damage, wights at 270, and brutes at 405. Breaking poise interrupts a windup and briefly staggers the creature (0.22s minor, 0.35s large). It does not retroactively erase a swing already active. A 1.5s / 2.4s stagger cooldown prevents permanent locking. These are native feel/balance values, not new roster powers or bonus damage.

## Presentation

Threat projections use the committed attack's direction, reach, radius, and windup. They are limited to nearby visible attacks, prioritize hostile zones near the player, then the ones activating soon, and do not reveal hidden enemies through cover. Hostile windups are coral; friendly area casts are cyan. At most five warnings and eight floating receipts render at once.

Short original synthesized impact, kill, dash, evade, and hurt cues play locally for the human player. Dense hits are rate-limited. Feedback adds no faith rewards, damage, aim assist, or time dilation. Stun, fear, the command cursor, defeat, and dashing clear pending melee inputs. Existing persistent corpses, ALT selection, independent armies, and belief adoption remain available.

The temporary vessel now uses a quiet dark material while authored entity rigs are being created. Existing imported creature models and human art are retained. A low-intensity fixed daylight sky environment keeps creatures visible in the giant vessel's shadow without capturing the surrounding blockout as black ambient light. Ability cards wrap long source names using measured font widths, and projected damage / threat graphics account for letterboxed camera views.

## Validation

`CultUlhu.CombatFeel` checks input buffering, one follow-up per buffer, alternating physical swings, actual-health/overkill receipts, real eliminations, stun cancellation, dash cancellation, evade feedback and expiry, elite poise resistance, interruption during windup, anti-stagger locking, and recovery commitment. `CultUlhu.Menu.CombatFeel` repeats those checks in the editor and captures `Saved/Screenshots/CombatFeel.png` with an authored creature and a deliberately extended test-only telegraph.

The game-world regression suite also covers combat collision, canonical kits, beliefs, spawning, living-world behavior, minor creature animation, persistent army orders, and 130 seconds of society simulation. Verification results are recorded below.

## Verified — 17 October 2026

- The native editor build succeeded. All nine game suites passed together in `Saved/Logs/CombatPolishVerified.log`: ArmyCommands, Beliefs, CanonicalGameplay, CombatFeel, GroundSpawn, LivingWorld, MinorCreatures, SkillCombat, Society.
- The combat-feel checks include real creature contact against the lower body of a tall player, not only synthetic damage calls. Creature movement covered 518.8cm with the authored leg animation active.
- The 130-second society observation recorded six torture completions, four sacrifices, twelve prayers, thirteen conversions, four completed constructions, and thirty-five destroyed city buildings. Three actual war kills had occurred before the window; final faction strength was x1.48 / x1.48 / x1.44.
- Test fixtures restore the player's temporary health, stamina, and attack state. The ground-spawn test continues to verify normal height, safe terrain placement, camera control, and movement.

- Final rendered checks passed 4/4 with no automation errors or warnings in `Saved/Logs/CombatPolishReady.log`: Menu.ArmyCommands, Menu.CombatFeel, Menu.InitialBeliefs, Menu.SkillPreview. The final native editor build succeeded after the presentation refinements.
- Reviewed `Saved/Screenshots/CombatFeel.png` and `Saved/Screenshots/SkillCombatHUD.png`: letterboxed projections align, source ability names fit on two lines, HUD hints stay inside the viewport, and the imported brute remains visible in shadow. The daylight ambient environment is set to intensity 0.10. Audio playback code ran without audio warnings; subjective listening was not performed.
- A repeated editor run exposed an army-test fixture occluded by randomly generated terrain/structures. The isolated command actors now spawn above the terrain; gameplay line-of-sight requirements remain unchanged. The corrected test passed in the final editor run.
- Unreal was left at the main menu with Play-In-Editor stopped. These are local project changes; no GitHub commit or push was made in this pass.
