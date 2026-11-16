# Playable native match modes

Both menu modes now launch dedicated local 5v5 matches: the player and four allied bots against five enemy heroes. This is the local single-player match implementation; networked human matchmaking is not implemented by this change. Free Roam retains its procedural society, cities, beliefs, armies, and creatures.

## Starting and controls

Choose Domination or Defense of the Great Old Ones, pick any of the sixteen released entities, and enter battle directly. Belief selection and belief effects are exclusive to Free Roam. A short protected preparation countdown precedes the match. The nine bots use released canonical kits, with mirrored compositions across teams. Character height remains canonical. Hero movement preserves relative kit speeds and now uses the 1.25 native speed multiplier described in `MovementAndPresentation.md`. Match third-person framing is closer and over the shoulder, leaving the crosshair clear; first person remains available with V. Bot obstacle probes use the full capsule to detect low jungle cover. Both mirrored teams include Mother Hydra as a support. Competitive health is normalized to the source Match rule of 100 HP, rather than the Free Roam health pool.

- LMB / RMB and Q/F/R retain the selected kit and physical attack collision. Tap Shift briefly (under 0.18 seconds), then release to dash in the current W/S/A/D direction; with no direction, dash forward. Holding Shift beyond 0.18 seconds sprints without an initial dash. Changing direction while holding Shift never repeats a dash. Jump and V camera controls remain available.
- TAB shows the ten-player K/D/A and level scoreboard. The tactical map remains visible during play.
- G channels an eight-second recall to the healing temple. Moving, attacking, or taking health damage interrupts it. The temple automatically heals after two seconds without damage.
- Hold ALT and click solid ground to rally nearby allied bots. The first two allies support that position for twelve seconds.
- ESC pauses the local simulation and opens Resume / Restart / Mode Select. ESC resumes; ENTER restarts. The results screen offers Rematch and Mode Select, with ENTER and ESC shortcuts.
- B and all belief/adoption bonuses are disabled in matches. Free Roam retains the three-belief selection and B adoption panel.
- All heroes recover 0.5% maximum HP per second after five seconds without attacking, dealing damage, or taking a hit. Healing and movement do not reset this delay; dash still interrupts recall. Temple recovery remains faster.

## Domination

The scoring model is the original `src/modes/CapturePointMode`: one central seal, 400 target, +1 per uncontested second with a living hero standing on it, +5 per opposing hero elimination. Any enemy hero inside the seal contests it and stops hold scoring. Servants and environmental kills cannot manufacture five-point hero rewards. The native seal radius is widened for the full-height entity bodies.

The match lasts ten minutes. A tied expiry enters source sudden-death overtime: the first new score wins, with a two-minute draw cap. Hero respawns take five seconds. R begins on its source cooldown, with no opening ultimates. Domination summon kits follow the source match variant: a frontal mauling with a delayed physical cone instead of lane summons. The native surge gives each source servant one 15-damage strike worth of power, rather than misreading a servant count as hit-point damage. Other canonical abilities, costs, cooldowns, and identities are retained.

The arena has a visible central seal, two protected preparation spawns, healing temples, and broad flanking routes around broken obelisks. Nine bots contest the point, fight from inside the seal rather than chasing opponents away from it, react to committed windups with real dashes, and retreat to heal.

## Defense of the Great Old Ones

The native adapter reuses the source `MobaDefense` objective damage, lane protection, wave stat generation, scaling, and timeout rules. Three lanes connect two temple bases. Each team has six 3000-HP lane towers, a 4000-HP nexus ward, and a 1500-HP Great Old One. Each outer ward protects its own inner tower. All six lane towers must fall before the nexus ward takes damage; destroy that ward to expose the god. Destroy the exposed enemy god to win.

Waves contain three melee cultists and one ranged cultist per lane; every third wave also contains siege units. Waves deploy every thirty seconds, with a strict forty-eight living lane-unit cap per team. Later waves scale using the source +2% interval factor. Opening a lane empowers that lane's attackers by the source x1.75. Melee and ranged waves use retained hooded-cultist and magus models and original animation clips; allegiance rings distinguish teams. Siege constructs, wards, and gods remain native prototype geometry. Retained ruin art frames the arenas and temple grounds.

Units march through real lane waypoints and attack with visible collision sweeps or straight projectiles. Tower shots travel straight at 15000 cm/s after a 0.25-second windup. Towers retain their selected hero target while that hero remains alive and in range, even if another hero moves closer. An enemy lane minion entering range takes priority over heroes; once the covering wave leaves or dies the tower can reacquire a hero. Held or flying minions cannot tank tower shots. Each bot has a persistent assigned lane (two top, two mid, one bottom per team, counting the human). After spawning or retreating it visits its lane entrance, escorts living waves from behind, fights opponents in that lane, avoids unsupported tower dives, and pushes the nexus ward followed by the exposed enemy god. Eligible relic detours stay near the assigned route. Allied bodies do not block team-mode attacks or bot sight lines; world cover still blocks attacks. Structures cannot be pulled out of their lanes.

The source progression is retained: level = 1 + XP / 50, capped at 30. At level 1 press Q or F to choose the first ability; the other becomes available at level 3, and R at level 6. A minion last hit awards 60 XP, a tower takedown awards 800 to its killer, and hero kills award 50 + 25 times the victim's level. Native convenience additions share nearby minion XP (30) and tower-team XP (200) so supporting a push is productive without a last-hit-only economy. There is no money or shop.

Each level grants an invocation charge. At level 10+, E near the healing temple starts a twenty-second artifact invocation. The existing tier thresholds are 10 / 20 / 30. Jungle relics require the corresponding level and uncontested presence: tier 1 is immediate, tier 2 channels for four seconds, and the mythic relic appears at ten minutes and channels for six seconds. Leaving, damage, or an opposing hero contests/interrupts a claim. Artifact damage amplification is capped at x3, with source level scaling at +2% per level.

Respawns start at five seconds and increase by one second per elapsed minute, capped at twenty seconds. A brief respawn shield ends when the hero attempts an attack. Matches terminate after twenty minutes: surviving god-health fraction decides, then remaining towers, then a draw. No endless stalemate is possible.

## Presentation and preservation

HUD elements expose score, clock, contest status, tower counts, wave timing, level, XP, recall, invocation, unlock requirements, and target protection. Damage, eliminations, corpses, dodge feedback, and attack warnings use the existing combat feedback. Results stop hero/unit simulation and freeze the match; a rematch reconstructs the arena and resets scores and progression, and restores mouse-look input.

All original character definitions remain untouched. Hypnos, Bast, the Howling Eye, and Elder Mind remain excluded by the sixteen-character release allowlist. The build stages only released hero definitions and the NPC animation clips needed by wave units. Clips are NonUFS files for the source animation serializer; the definitions remain UFS. Explicit cook settings include both maps, mode materials, retained world art, and the two wave-character art folders. `Saved/Backups/BeforeMatchModes.zip` preserves the native source before this work.

## Verification

`CultUlhu.Menu.DominationMatch` exercises entry, two full teams, canonical height and grounded spawning, preparation protection, point presence, contesting, actual hero kill scoring, duplicate kill rejection, timed respawns, victory, frozen results, pause/resume, and rematch.

`CultUlhu.Menu.MobaMatch` additionally exercises ability drafting and level gates, real tower/core actors, backdoor protection, outer-to-inner ward order, friendly-fire rejection, a live seventy-second wave and bot-combat observation, core destruction, and rematch. Test fixtures intentionally deal administrative tower damage to verify objective transitions; normal gameplay must land physical attacks. Rendered snapshots include both modes' HUDs, live lanes, and results.

Verified on 2026-10-17 (local time):

- Final Development Editor build succeeded. The final rendered Domination and MOBA tests both passed with zero errors and zero warnings (`Saved/Logs/MatchModesReady.log`), after the shoulder-camera and full-capsule steering adjustments.
- The final MOBA observation recorded 166.6 real hero damage, 59 animated authored lane rigs, and 35 minions advancing along lanes after seventy seconds. No natural hero eliminations occurred during that short observation; elimination scoring and respawn were exercised separately in Domination. Full natural-length match outcomes were not observed.
- The preceding rendered five-test run passed army commands, initial beliefs, skill preview, Domination, and MOBA (`MatchModesFinalEditor.log`). It also exercised actual pause/resume, timed belief adoption, recall interruption, completed artifact invocation, delayed ultimate contact, frozen results, and rematch input restoration.
- All ten native gameplay regression suites were verified across the broad regression and targeted reruns. The broad run (`MatchModesGameplayVerified.log`) passed nine, including the repaired society warfare/construction behavior; its jump fixture was then corrected to allow a movement frame before releasing Jump. The targeted traversal rerun passed, and the final GroundSpawn + SkillCombat rerun passed both (`MatchModesCombatFinal.log`), with a measured 1200 cm jump rise. These were separate runs, not one simultaneous ten-test pass.
- Runtime staging receipt inspection confirmed sixteen released character-definition dependencies and eight NPC animation clips, with none of the four deferred characters staged. Cook configuration was added, but a full shipping cook/package and networked matchmaking were not tested.
- Screenshots in `Saved/Screenshots/`: `DominationHUD.png`, `DominationVictory.png`, `MobaHUD.png`, `MobaLive.png`, and `MobaVictory.png`. Final editor state: `/Game/Maps/L_MainMenu`, PIE stopped.


## Healing and mode isolation update — 2026-10-18

Mother Hydra replaces the former non-support fourth pick on both mirrored bot teams. Her canonical F, Brine Blessing, heals an aimed allied hero or herself when aimed at empty ground. It restores up to 60 HP in matches (the existing source competitive healing scale), costs 35 stamina, and has a sixteen-second cooldown. Selecting F first makes it available at MOBA level one; otherwise it unlocks at level three. Support bots choose the most injured reachable allied hero, including themselves, without spending the heal against enemies. Range, line of sight, alive state, team, and native maximum health are checked. Refused or full-health targets do not spend stamina or cooldown.

Competitive healing excludes all minions, kit-summoned creatures, towers, and gods. In Free Roam, allied heroes, friendly cultists, and creatures belonging to that cult qualify. Wild or imprisoned beasts, enemy cult members, civilians, and buildings do not qualify. Shub-Niggurath's original Thousand Young passive returns 10% of actual damage inflicted by her own summons as healing to her; this never heals the minion itself. Her match summons retain the canonical 25% longer duration. Original character definitions are unchanged.

Belief services remain as neutral dependencies for the shared original combat code, with empty active beliefs and a forced x1 exertion combat multiplier in competitive worlds. Initial choices are refused, the character button enters battle directly, B cannot open the panel, adoption requests are rejected, and faith clocks do not advance. Free Roam's selection and effects remain enabled.

The mode AI checks exposed a separate map-loading defect: hidden `dark_wilds_ground` from the original authored level was regaining collision while level components initialized. Legacy geometry is now tagged before replacement, disabled at the actor level, and disabled again after component initialization. New match arena and procedural Free Roam geometry retain collision. The temporary attempt to ignore the new floor in lateral bot sweeps was removed; real obstacles still participate in full-capsule movement.

### Update verification

- Development Editor build succeeded. The final MOBA and Free Roam belief-picker checks passed in `Saved/Logs/ModeRecoveryFinalChecks.log`. The MOBA observation recorded all nine bots entering their assigned lanes, 895.1 real hero damage, 27 advanced minions, and twelve combined kills/deaths (six eliminations) after seventy seconds.
- The final Domination check passed in `Saved/Logs/ModeRecoveryDominationIsolated.log`: all four allied bots and all five enemy bots participated on the actual seal, with 1338.3 real hero damage in the observation. A delayed canonical surge inflicted 61.2 HP damage on an isolated physical target after one simulation second. The fixture parks unrelated bodies away from the strike and waits on game time, so neither body occlusion nor an asset-load hitch can invalidate the intended contact setup.
- Both mode tests verify blocked initial belief choices, absent belief pickers, disabled B panels, rejected adoption requests, empty beliefs on both teams, neutral exertion multipliers, genuine allied healing, proactive support-bot healing, rejected match summons/minions/structures, friendly-cult healing in Free Roam, rejection of enemy cults and wild beasts, actual summon damage return, and the five-second recovery delay at 0.5% HP/second.
- Final mode checks ran in an unattended process because earlier interactive runs were interrupted by stopping and restarting Play. Earlier failed routing observations were used to identify and repair the hidden legacy terrain collision; they are not counted as successful tests. The original native Free Roam belief and physical-combat checks passed before that terrain fix; the post-fix native regression passed all three checks (Beliefs, GroundSpawn, SkillCombat) in `ModeRecoveryFreeRoamFinal.log`, including a measured 1191 cm jump rise.
- Follow-up Shift input, tower/nexus, and Cthulhu RMB requests were recorded separately in `PendingGameplayChanges.md`; see the subsequent pass below for their implementation.

Match summon melee now explicitly bypasses the shared cult belief/exertion calculation as well as hero combat doing so. The final mode suites assert neutral summon damage in both competitive modes.

Final release verification: `Saved/Logs/ModeRecoveryReleaseChecks.log` passed both DominationMatch and MobaMatch, with no automation errors or warnings. Domination recorded 4/4 allied and 5/5 enemy bot seal participation and 1415.8 real hero damage. MOBA recorded 9/9 assigned-lane entries, 792.1 real hero damage, 28 advanced minions and eight combined hero kills/deaths in seventy seconds. These are short live observations plus controlled victory/rematch fixtures, not a claim that a full natural twenty-minute game was observed. All temporary blocker diagnostics and floor-ignore workarounds were removed.


## Tower and minion sling pass — 2026-10-18

The Shift gesture and objective rules above supersede the earlier pass. The original character JSON files remain intact; the following RMB is the user-requested Cthulhu MOBA variant only.

Aim at an enemy lane minion within 45 metres and hold RMB to lift it. Release RMB or click LMB to launch after a brief 0.3-second lift. A held minion automatically launches after four seconds. A successful pickup starts the 20-second cooldown. Empty aim, a friendly minion, a hero, or a structure cannot be picked up and does not spend the cooldown. Death, incapacitation, recall, or invocation releases a held minion.

The actual minion follows a straight, non-homing physical projectile at 6000 cm/s, with a 100 cm collision radius, a 0.12-second launch windup, and 80-metre maximum travel. Enemy hero contact deals 20 base competitive damage; tower contact deals 40, both using the existing level/artifact multiplier. Evades and ward protection remain effective. Contact consumes the minion. Other lane minions are passed through; world cover blocks the throw. A miss lands on safe arena ground within 45 metres of pickup, halves remaining HP (never restores wounded health), and resumes its original lane through its midpoint or next waypoint. Cancelled lifts also leave a surviving wounded minion. Cthulhu in Free Roam and Domination, and all other character RMB kits, retain their original behavior.

### Pass verification

- Development Editor build succeeded. `Saved/Logs/TowerSlingChecks.log` passed DominationMatch, MobaMatch and the new MobaMechanics test with no automation errors or warnings. The mode checks include actual victory and rematch through the new outer/inner, all-six-lane, nexus, and god damage guards.
- MobaMechanics tests distinguish long Shift holds, changes of direction, release, four directional taps, and control/cursor cancellation. It checks actual tower HP, persistent hero aggression, minion priority, faster projectile contact, camera-aimed RMB pickup, visible levitation position, release and LMB throwing, physical hero/tower damage, cooldown refusal, missed throws, bounded grounded landings, half remaining HP, real lane rejoining, interrupted lifts and friendly-pickup refusal. Its hero impact reduced 100 HP to 79.6 HP after competitive level scaling.
- `Saved/Logs/TowerSlingFreeRoamFinal.log` passed Beliefs, CombatFeel, GroundSpawn and SkillCombat. The initial combined regression's health-equality assertion was affected by an unrelated six-point hit during its jump observation. The fixture now proves the same low attack contacts a standing hero, then measures that attack's own collider contacts after jumping (zero); the live world remains active and still records the unrelated health change. No gameplay immunity or collision behavior was weakened to satisfy this test.
- A full Shipping package and full natural-length matches were not tested in this pass.

- Rendered `TowerSlingRendered.log` passed MobaMechanics and SkillPreview, including Cthulhu's original Free Roam held wave and 3D thrown-body collision. SkillPreview recorded two movement timestep warnings during a load hitch; neither test reported errors. The final HUD adjustment moved the RMB instruction above the melee recovery bar. `TowerSlingRenderedFinal.log` passed MobaMechanics again with no automation errors or warnings; `Saved/Screenshots/MobaMinionHeld.png` was visually inspected for actual levitation and readable controls.
