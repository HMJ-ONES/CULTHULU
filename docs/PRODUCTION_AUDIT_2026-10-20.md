# Cult-Ulhu production audit â€” 20 October 2026

## Assessment

The native Unreal build is a playable systems prototype. It is not a polished hero shooter, a production MOBA, or an art-complete game. Working score rules and automated matches establish basic functionality; they do not establish that combat is enjoyable or that competitive balance exists. No measured human playtest evidence currently supports a comparison in fun with Paladins Onslaught. The root README's old headless-beta description does not describe the native build accurately.

Overwatch is a reference for readability, animation, audio, level composition and interaction consistency, not a quality claim about this project. Blizzard's [Flashpoint map rework discussion](https://news.blizzard.com/en-us/article/24215719/weekly-recall-flashpoint-map-reworks) explicitly prioritizes readable entrances, distinct areas, fewer frustrating choke points and useful connections. The lesson here is to provide understandable choices and test their timing. More geometry or a larger footprint alone can make a map worse.

## Comparison with the references

| Area | What the references establish | Cult-Ulhu evidence | Development consequence |
| --- | --- | --- | --- |
| Objective tension | The [Hi-Rez player guide](https://support.hirezstudios.com/en/articles/9516132-new-returning-player-guide) describes Onslaught as scoring from holding a stationary objective and eliminating players. | Native Domination uses both incentives, with source rules of 400 target score, +1 per second held and +5 per hero kill. | Rules are a recognizable starting point. Their presence does not recreate the excitement of readable team fights, coordinated pushes and recoverable defenses. |
| Team roles | Hi-Rez describes damage, flank, frontline and support as different jobs within a team. | A support bot can heal allies, but compositions are fixed mirrors and decisions are local. | Distinct source kits need actual tactical jobs and readable delivery. Do not add arbitrary abilities just to imitate another roster. |
| Spatial choices | Blizzard describes flank routes, connected paths, accessible high ground and readable entrances. | Previously one flat arena; this pass introduces real collision, sheltered passages and elevated choices. | Test route timing, advantage and counter-routes. The goal is useful choices, not the highest object count. |
| Feel and readability | The references expose distinct weapons, roles and abilities within coherent combat spaces. | Collision and cooldown behavior have scoped automation evidence; visual attack distinction and reaction quality remain inconsistent. | Time-to-kill, misses, evasions and impacts need controlled encounters and player observation for every released kit. |
| Visual refinement | The requested Overwatch benchmark includes finished character art, animation, effects, lighting and interface integration. | Placeholder entity capsules, blockout structures and basic effects remain visible. | Current visuals are substantially below the benchmark. Art integration and animation cannot be replaced by darkening the image or adding decorative assets. |

A larger Domination footprint has a cost: dead travel can reduce engagement. Track spawn-to-first-contest time, time spent in combat, successful flank frequency, deaths without a readable threat, objective turnover and player confusion. No human ratings or retention measurements have been collected in this pass. Do not assign a fabricated fun score.

## Source evidence and gaps

- `CultMatchArena.cpp`: the previous Domination arena was a flat 460m square, a circular seal and four cover blocks. Decorative architectural assets had no collision. At the start of the audit, MOBA had straight parallel lanes and extremely sparse jungle obstacles; the route/terrain pass below replaces that layout. A scenic silhouette does not provide a usable interior or a combat route.
- `CultMatchPawn.cpp`: bots previously imposed a constant ground height while moving. They could not legitimately use elevation. Any elevated arena needs physical movement verification with the retained 20â€“30m character dimensions.
- `CultUlhuPlayerPawn.cpp`: release entities still use placeholder vessels. Final character meshes, animation state machines, expressive anticipations, impact reactions, silhouettes and properly authored first-person arms are missing. A font or palette change cannot close this gap.
- `CultMatchBattle.cpp`: bot decisions are direct-goal steering and local target selection. Domination now has verified physical flank connections and a simple route role; adaptive team strategy, flank timing and role coordination remain absent. Current bots are useful opponents for development, not evidence of a complete competitive experience.
- `CultFeralBeast.cpp`: actual modular creature art and physical delayed swipes exist, but the initial native MOBA lacked configured neutral camps, sourced jungle kill XP, camp respawn and leash rules; those are now covered by the encounter pass below.
- `CultMatchState.h`: competitive rules retain the original source adapter. MOBA route representations, tower placement, minion route progress, thrown-minion return paths and tactical map must change together; a curved painted lane alone would be misleading.
- UI uses packaged fonts and a coherent dark palette, but still needs readable feedback at multiple resolutions, focus/controller behavior, reduced visual noise and stronger objective hierarchy. Small rendered editor viewports are a separate constraint from full-screen play.
- Collision attacks and cooldowns are implemented, but time-to-kill, ability contrast, fairness, combat readability and roster balance need measured encounters and human playtests. Distinct source kits must be preserved while their physical delivery is improved.
- Network fairness, replication, performance under a full war simulation, controller support, accessibility and packaged release behavior are not established by local editor tests.

## Current implementation pass

Domination has been expanded to 640m Ã— 600m, with a 104m Ã— 74m rectangular scoring seal. Four keeps provide upper fighting decks and covered lower passages, with normal walking ramps and two connecting overpasses. Broken nave walls and side cover split approaches. Physical boundaries back up the existing recovery guard. The tactical map and capture predicate share the same dimensions. Bot movement now uses CharacterMovement floor finding, ramp handling and gravity instead of fixed-height teleports. Obstacle avoidance allows low step edges rather than rejecting the entrance to an otherwise walkable deck.

These are blockout improvements, not finished castle art. Covered passages beneath the keeps are not a completed subterranean cave/tunnel network. Verification results must be recorded after runtime checks; planned work is not a passed test.

## Remaining requirements and useful acceptance evidence

1. Domination: verify rectangle scoring at corners and edges, blocked sight lines, full-size capsule clearance, actual ramp traversal, both-team objective participation, route timing, spawn safety, and renders from gameplay viewpoints. Add true subterranean connections where they create useful flanks. Measure respawn-to-contest time; do not inflate empty travel distance.
2. MOBA: maintain the shared curved routes across bots, waves, towers, minimap and minion landing/rejoin. Extend runtime evidence for terrain interactions, route timing and tower combat as geography changes.
3. Jungle: refine the terrain and encounter presentation, validate contested camp fights and human balance, and profile long sessions. The creature/damage/XP/reset/respawn functionality is covered by the verification record below.
4. Combat feel: compare timed encounters for each released kit, check anticipation and recovery, readable attack volumes, miss feedback, hit reactions, camera comfort and defensive counterplay. Preserve original ability identities. Collect player observations rather than treating a numeric test as a fun score.
5. Presentation: final entity art and animations, meaningful materials and lighting, authored landmarks, restrained effects, coherent audio, resolution and accessibility checks. Capture before/after gameplay renders and profile representative battles.
6. Production: native packaged smoke tests, reproducible build instructions, save/data compatibility, multiplayer/replication audit and performance budgets. A prototype that works in PIE is not a verified published version.

Keep the full objective active. Do not label the project finished or comparable in refinement to the references until broad runtime, visual and human playtest evidence exists.

## Verification record for this pass

- Editor build succeeded after source changes.
- `ArenaFinalTraversal.log`: rendered DominationMatch and MobaMatch both passed. The passage accepted the full 30m hero capsule. Expanded-corner presence earned actual score and standing beyond the painted edge did not. The real ramp walker reached the 60m deck. This is functional evidence, not a balance or fun verdict.
- The same run recorded 9/9 MOBA bots reaching assigned lane entrances. This establishes compatibility of physical bot movement with the existing lanes; it does not verify curved lanes or jungle camps, which do not exist yet.
- `BuildArenaMaterial.log`: M_ArenaSurface was authored and its Color vector parameter verified. Existing imported materials were preserved. Packaging settings already include the material directory.
- Visual inspection rejected the initial bright material/exposure result and the first overly dark fixed-exposure result. Ambient light and exposure were corrected. The tactical map now draws the real collision geometry and raised walking surfaces.
- `DominationPresentationFinal.log`: the final rendered DominationMatch passed after those visual changes. Ramp arrival feet were X=10500, Y=13861.25, Z=6002.15 cm. Four allied and five enemy bots participated on the seal; 942.8 actual damage was recorded. Gameplay and overview screenshots were inspected. The result is a more navigable blockout with legible floor and routes, still visibly primitive and far below the requested final art target. Entity vessels still lack finished character art.

## MOBA route and terrain pass

One native route definition (`CultMobaRoute.h`) now supplies 32 connected segments per lane. The outer curves reach 175m either side of mid, with both temples retained at X +/-300m. The original rules adapter receives these same world-space routes, so its arc-length tower placement agrees with road geometry. Bots measure progress and lane distance by projection onto their assigned route; waves traverse actual route points. The tactical map uses the same curves. Thrown minions project back onto their original lane rather than using the old fixed Y coordinate.

Four jungle plateaus now stand between the lanes, mirrored between teams. The lower side is 25m high and the higher side 35m. Each has two ramps, collision cliffs and retained rock art. This remains a geometric blockout, with regular plateau shapes; it is not finished organic terrain. Neutral encounters are implemented in the subsequent camp pass below; the terrain itself still needs substantial art and composition work.

Minion movement now checks a full swept collision capsule and samples supporting ground. Returning minions can use explicit ramp paths, including crossing to the opposite jungle plateau when necessary. Relics have moved onto the plateau surfaces; claim checks include height, and eligible bots approach via entrances instead of targeting buried relic positions.

The first terrain test caught four outer-lane contacts with raised ramp side edges. Rather than accepting a partially blocked combat route, the outer curves were widened away from the jungle. `MobaLaneClearanceDense.log` then passed with full-size capsule ground sweeps at 129 positions per lane, including between geometry waypoints.

`MobaCurvedRoutes.log` passed before the jungle additions: all nine bots entered their assigned lanes, both teams' waves advanced on all three curves, all twelve source-owned towers aligned with the routes, and tower targeting/Cthulhu minion-throw regressions passed. This record does not by itself prove the later terrain revision; the rendered final terrain verification is recorded separately after it finishes.

## Final MOBA verification record

- `MobaLaneClearanceDense.log`: passed full-size ground/capsule clearance at 129 samples per lane after widening the outer corridors. The full match repeats these physical checks on the final terrain.
- `MobaTerrainFinal.log`: the normal match, tower targeting/minion-throw mechanics and route-geometry tests passed. The separate all-path return test failed 8/12 cases at the lower ramp seams. This failed evidence was retained and the geometry corrected; it was not treated as a successful release gate.
- `MobaRampSeamsFinal.log`: the corrected rendered build passed `MobaJungleReturns` (12/12 physical plateau-to-original-lane returns) and `MobaMatch`. Surviving thrown minions retained half their remaining health. The real 30m player climbed the 35m plateau, reaching feet Z=3501.94 cm. Relics rejected claims from below the plateau and accepted a real channel at the intended elevation.
- All nine bots entered their assigned lanes. Both teams' waves physically advanced along all three routes, with observed progress ranging from 0.492 to 0.617 of their route. This is observed movement; it is not proof of balanced strategy or enjoyable pacing.
- Final gameplay/overview renders were inspected. The roads, tower colors, raised ground and minimap are consistent, but regular plateau geometry, sparse decoration and capsule entities still read as prototype art. Further work must improve the environment and combat presentation without hiding movement defects behind scenery.

## Neutral jungle encounter pass

Four neutral camps now use the retained skeletal creature art. Hunters use 180 HP, nominal 20-damage swings and 180 XP for a hero killing blow; wardens use 300 HP, nominal 28-damage swings and 300 XP. This exceeds the existing 60-XP lane-minion killing blow. Nearby eligible teammates share half the camp reward; opposing heroes receive none. These are starting balance values, not measured player-approved tuning.

Both teams can aggro camps. Attacks use the existing physical, delayed creature sweeps with windup and recovery; they are not automatic damage ticks. Creatures leash to their plateau, cancel pending attacks on reset, recover health without paying XP, and respawn after 60s/90s for hunters/wardens. Source credit follows owned summons and projectiles, including lethal hits after an owner moves away. Neutral camps cannot become Free Roam captives or receive competitive creature healing. Bots can retaliate with their actual kits.

The first render exposed inadequate creature scale beside the 30m entity capsules. Camp variants were enlarged to approximately 8m hunters and 13m wardens without altering Free Roam species scale. Packaging settings explicitly include the four retained creature asset folders. This is not a packaged-build verification.

`JungleCampVerification.log` passed the initial physical combat, dash avoidance, reset, XP-sharing, summon ownership, corpse presence and accelerated-respawn assertions, plus normal MOBA play and all twelve terrain return paths. `JungleCampFinal.log` passed the larger creature revision, actual bot retaliation, ranged/source-credit checks, full MOBA match and tower/minion-throw mechanics. The test checks configured 60s/90s respawn values, then accelerates a timer to verify the respawn transition; it does not wait ninety real seconds.

A separate Free Roam regression initially missed the buffered follow-up slash. A diagnostic run observed correct opposite physical angles, but exposed an attack timing problem on slow frames: a newly spawned actor could consume frame time which preceded its creation. The first-tick timing correction then passed its focused creation-frame check and projectile regressions. The old Free Roam test also used a manual 0.7-second Tick without advancing world time; that step was replaced with normal runtime ticks and a source-confirmed creature-hit observation.

## Final encounter and shared-combat checks

- `JungleCombatPolishFinal.log`: camp combat and MOBA projectile/tower mechanics passed after the birth-frame correction. The corpse's posed render bounds measured 3.0cm above the actual plateau surface, replacing the previous -55.6cm clipping result. Corpse grounding now uses the rotated mesh bounds rather than a scale-dependent fixed offset.
- Competitive poise thresholds now scale with camp HP: 30% for hunters and 45% for wardens. A real warden windup resisted the first impact, then was interrupted by sufficient cumulative damage while the creature was still alive. Free Roam species thresholds were retained.
- `FreeRoamRuntimeContactFinal.log`: Free Roam CombatFeel passed. It observed exactly one buffered follow-up with the opposite physical angle, checked evasion and feedback, retained the Free Roam heavy-creature poise/recovery behavior, and confirmed the small ghoul's real runtime hit against the tall player. Earlier failing logs remain available; they are not represented as green results.
- `JungleCadenceFinal.log`: the final camp revision passed after decoupling visual enlargement from attack cadence. Hunters retain a 0.4s windup and 0.5s recovery; wardens retain a 0.72s windup and 1.05s recovery. The check also covers actual hits, a lateral dash miss, poise interruption, both teams, native bot retaliation, projectile and summon XP attribution, duplicate-payment rejection, reset, fresh respawn, corpse persistence and grounded bounds.

These checks establish specific mechanics. They do not establish final difficulty, enjoyable contested fights, organic terrain quality, final animation quality, sixty-FPS performance or a production-ready release. The environments and entity vessels remain visibly provisional. Reset has a six-second stuck-recovery fallback; navigation still needs broader contested-play validation. Corpse rendering cost over a full session is unmeasured.

## Next implementation dependency

Domination now has a connected north/south catacomb below the seal. A tiled collision floor leaves two actual 70m-wide entrance openings; 26.6-degree ramps descend 50m, with 47m of clear height below the central roof. The surface objective, keeps and ordinary bot approaches remain intact. Domination's lower rescue threshold is now -65m; MOBA retains its original threshold. Underground floors participate in the player's terrain bounds, so legal traversal no longer triggers a return to spawn.

`DominationCatacomb.log` passed the first rendered runtime regression. The actual full-height player walked from the south surface to feet Z=-4997.85cm, across the covered corridor, and out at the north surface at Z=2.15cm. No fixed-height movement or teleport was used along the route. The existing keep-ramp test also passed at Z=6002.15cm. One allied and three enemy bots recorded capture participation, with 191.6 actual damage in that run. This established preserved surface bot behavior at that stage; independent underground rotations were added in the subsequent crypt pass.

A final navigation/readability pass adds dashed tunnel routing and entrance brackets to the tactical map, plus a context hint below ground. Its runtime test also observes the actual player directly beneath the seal, checking no capture score and no safety relocation. `DominationCatacombFinal.log` passed the final rendered DominationMatch test, including direct-beneath-seal score rejection and the complete walking traversal. The final run recorded one allied and three enemy capture participants and 171.2 actual damage. The inspected `Saved/Screenshots/DominationCatacomb.png` shows a readable floor, walls, exit and route markers, with the context hint and tactical map visible. This is an offscreen editor render, not a foreground performance measurement.

That initial tunnel was a long rectangular blockout corridor. The connected crypt pass below adds decisions, cover and limited authored portal art; architectural rhythm, richer surfaces and measured player feedback are still needed before calling it an enjoyable competitive flank. The larger production goal remains open. Next: address the route's pacing and visual composition, then improve combat presentation and continue finished art integration.


## Connected crypt flank pass (in verification)

The two-end tunnel was physically traversable but strategically weak: its entrances were far from the objective and it provided no intermediate decisions. Four new side ramps now connect the catacomb junctions to the covered lower keeps. Their floor openings and side-wall openings are real collision geometry. Junction flats preserve the north/south walking path; the new ramps rise 50m over 95m. The existing surface objective and direct approaches remain untouched.

One bot per team receives a physical keep-to-keep flank route, while the remaining bots pursue the seal. Opposing flankers start on opposite sides. Underground retreat uses the connected exits instead of steering directly into a roof. Domination's pursuit clamp now only applies while a bot is actually inside the capture area; previously even distant flank encounters were pulled toward the seal. These are still simple scripted route roles rather than adaptive team tactics.

The expanded runtime verification walks all four side connections in both directions with the actual 30m player, then gives the bots 65 seconds of independent play. It checks underground visits, completed flank routes, capture participation and actual damage. `DominationCryptConnections.log` passed all eight full-height side-ramp walks and both-team underground visitation. It recorded four allied and five enemy seal participants and 2387.4 actual damage over the longer observation. The run failed its route-completion assertion: navigation only advanced on combat-free frames, and a bot fighting on the seal could retain its final waypoint indefinitely. This was a genuine state problem, despite physical traversal working. Arrival now advances independently of combat, and reaching the capture rectangle completes the final objective approach after all flank waypoints. Final verification follows below.

Retained ruined-column meshes now frame the portals and the recessed central cover, with dedicated matching collision proxies. The tactical map includes the four branch connections. This is a small improvement in material detail and landmarks; the environment remains visibly blockout architecture and the player entities remain placeholder vessels.


The second rendered run (`DominationCryptFinal.log`) again passed the eight full-height portal walks with the retained column art in place, and passed MOBA tower/projectile/minion-throw mechanics. It still failed the both-team route-completion requirement. Diagnostics showed an allied flanker outside the point near the enemy temple with no capture time: combat pursuit could override the flank approach and pull it away from the objective. Domination bots now keep their approach/route movement while aiming and attacking nearby threats; holding bots still fight from inside the seal. MOBA pursuit remains unchanged. The completion predicate still requires finishing the physical flank waypoints and entering the seal.

The independent-play assertion now subtracts all setup damage and snapshots each bot's capture time before the 65-second live observation. The slab and recessed cover also have attack-occlusion checks. This prevents scripted fixture damage or earlier presence from satisfying the autonomous-combat requirement. A fixed 60Hz physics run with NullRHI is verifying these behavioral revisions; its result is not a rendering or performance result. The portal render was inspected separately in the preceding rendered run.


`DominationObjectiveFlanks.log` passed the complete revised DominationMatch at a fixed 60Hz physics timestep with NullRHI. Both flankers visited the underground route, completed the physical approach, and earned capture time (5.2s allied / 33.9s enemy in that run). All four allied bots and all five enemy bots recorded new capture participation. The 65-second independent-play interval recorded 1817.7 additional actual damage after subtracting setup damage. All eight side-ramp walks, original north/south traversal, upper keep ramp, attack occlusion, below-seal score rejection, death/respawn, match completion and rematch assertions passed. These are scoped mechanics and navigation results, not human fun ratings or foreground performance measurements.

`DominationCryptProductionCheck.log` passed the final normal-frame rendered regression against the same behavior revision. Both flankers completed the physical route and earned fresh seal time (11.8s allied / 13.5s enemy); all nine bots contributed new capture time. The independent-play damage delta was 2563.4. All four side ramps passed uphill and downhill with the full-height player, and the central tunnel, upper ramp, surface-slab/cover attack blocking, scoring, death/respawn, victory and rematch checks passed. The portal and map renders were inspected. This establishes the scoped behavior under both fixed and variable frame timing; it does not establish a human fun rating, final balance, frame-rate target or packaged-build quality. No art, source character kits, MOBA tower rules or Free Roam belief rules were replaced by the Domination AI changes. Remaining priorities are combat readability, adaptive route timing, more varied environment composition, finished entity art and animation, measured player feedback, long-session performance and packaged verification. The full production objective remains active.


## Combat signal readability pass

The previous effects used the same eye material for almost every attack, forty separate sphere markers for areas, and flat HUD projections even for spherical and pitched attacks. Continuous, depth-tested procedural edge geometry now distinguishes ground discs, three-dimensional auras, spherical-sector cones, pitched swing fans, dashed beam charges and swept projectile trails. The collision radii, trajectories, damage, cooldowns and canonical spell definitions are unchanged. Friendly signals use teal and hostile signals coral. Player swing arms retain their simple articulated cylinders; these are not finished arm meshes or animation.

Area warnings now use one outline component rather than forty separate static marker components. This reduces those component counts; no frame-rate improvement is claimed without profiling. The HUD no longer draws a second projected ground fan over the world, and limits its small timed labels to two. World geometry provides depth occlusion rather than drawing screen-space boundaries through cover.

The first rendered gallery was too distant and captured during shader preparation. A closer fixture with completed shader compilation then exposed insufficient contrast from additive outlines on bright ground. That candidate was rejected. The generated signal-edge material now uses opaque unlit color, retaining depth testing. Its thin tubes avoid covering the interior of the attack volume. A screenshot callback now checks actual colored pixels and saves the inspected PNG, instead of accepting file presence as visual evidence.

`AttackSignalsRuntime.log` passed the original geometry/timing checks, Free Roam CombatFeel, JungleCamps and MobaMechanics. That did not prove the first gallery looked correct. `AttackSignalVisibility.log` intentionally failed the stronger pixel visibility checks on the pale additive candidate. The final candidate passed the verification recorded below, including an actual pitched-swing warning test, no-warning-phase contact, active contact, projectile trail alignment, beam charge/core transitions, area component counts, faction color distinction and rendered contrast. These are gameplay cues, not final spell VFX or a claim of Overwatch-level art quality.


`AttackSignalsFinal.log` passed all four rendered tests: AttackSignals, Free Roam CombatFeel, JungleCamps and MobaMechanics. The signal fixture recorded 648 teal and 86 coral pixels on bright ground after the contrast revision. Its geometry checks distinguish true aura height, flat ground impact, cone sector boundaries and a pitched swing arc; contact occurs only after windup. Projectile trails follow actual swept positions and beam cores remain hidden during charge. The inspected Free Roam render also shows the creature's committed swing warning in the game environment.

A subsequent buffer update uses UpdateMeshSection when the topology is stable, avoiding render-proxy recreation on each moving projectile/swing frame. A second-step projectile test checks the trail's moved bounds. The final gallery and normal-frame Domination regression passed against that revision in `CombatSignalBattleFinal.log`. The moving trail bounds matched its next real projectile position. The image contained 652 teal and 87 coral pixels; the screenshot was inspected separately. Both flankers completed their underground route and earned seal time (9.1s allied / 41.8s enemy), all nine bots contributed fresh capture participation, and the independent battle interval recorded 1790.1 additional actual damage. This pass does not add physical hitboxes to warning meshes, alter source abilities, or establish final VFX quality, color-vision accessibility, complete animation or overall frame-rate targets.

The next combat dependency is measured delivery and pacing across the released kits: projectile travel relative to scaled movement, practical time-to-kill, interruption chains and avoidable/counterable threat. Ability names and original identities must remain canonical. Human playtest evidence is still missing; neither passing damage callbacks nor a clearer outline is a fun verdict.


The closer live battle artifact (`Saved/Screenshots/DominationCombatSignals.png`) shows a real committed warning in the match, but it is a spectator view with foreground cover obscuring part of the fight. It does not prove player-view readability for every kit. Placeholder entity vessels overlap, corpse/body silhouettes remain difficult to distinguish in this view, and animations are far below the requested benchmark. The Free Roam creature view provides a clearer local example of the committed swing cue. The next pass should measure actual attack delivery and fight pacing, improve body/state readability, and integrate final authored character animation rather than treating these outline guides as completed spell art. The production objective remains active.


## Hero corpse placement and input dispatch

The earlier jungle corpse check covered creature bounds, not hero components. A new visible-surface regression failed in `HeroCorpseBaseline.log`: the true stretched-sphere surface was 12436.45cm above terrain and the corpse retained the two living eye meshes. Diagnostics in `HeroCorpseBoundsProbe.log` identified a fourth copied static component: Unreal's hidden editor camera proxy, far down the giant character's long camera boom. The old blanket exception for player meshes copied this helper despite it being hidden/editor-only. Once rotated, its low bound raised the entire corpse over 120m. The five-degree tilt also produced a smaller conservative-bound gap, but it was not the primary cause.

Corpse copying now excludes editor-only and hidden helper meshes and vessel eye lights. The real vessel body has an explicit retention tag so first-person death still creates its body. The pose is horizontal at 90 degrees instead of 85 degrees, matching the sphere's true contact support on flat terrain. Bodies remain persistent and nonblocking. This is a posed corpse treatment, not ragdoll physics or a verified solution for every steep slope and cliff.

`CorpseStateFinal.log` passed rendered CombatFeel, JungleCamps and MobaMechanics. The actual visible hero sphere surface measured 3.00cm above terrain in all five fixtures: original size, 20/28/30m variants with different headings, and first person. Each corpse contained exactly one body sphere, no living eyes and no camera proxy. Source location and scale stayed unchanged. Creature corpse presence, respawn, XP, attack cadence, native tower targeting and minion-sling mechanics remained covered.

A duplicate LMB-release binding was also found in the native pawn. It is now bound once. The editor input regression checks the actual runtime binding count in addition to backward melee, ALT commands, the Simulate guard, and intentional F8 eject/repossess. `CorpseBattleInputFinal.log` passed the rendered EditorInputSafety test: exactly one runtime LMB-release binding, actual backward melee, ALT cursor/selection, no accidental Simulate, intentional F8 eject/repossess and restoration of the editor shortcut policy. The normal-frame DominationMatch also passed, retaining both-team physical flanking, all nine bots' fresh capture participation and 1975.2 additional battle damage.

## Projectile delivery measurements

`Saved/Reports/ReleasedProjectileDelivery.csv` records nine released projectile spells (eight travelling bolt profiles and one beam), measured from instantiated native attack actors using the original character definitions. Twin Fangs creates two bolts. Measured bolt speeds span 1800-4800cm/s, radii 14-100cm, ranges 1600-2000cm and windups 0.12-0.30s. The beam activates after 0.35s and has no flight interval. The report's walking/sprinting columns are calculated from each canonical kit/height using the current pawn movement formula; they are not measured moving-target encounters.

Every measured bolt is slower than its caster's calculated sprint speed, and several are slightly or substantially slower than walking. This is a pacing concern to investigate, not sufficient evidence that every projectile should be accelerated: short range, giant capsule sizes, muzzle offsets, target travel, damage, cooldowns and evasion all matter. No source powers, cooldowns, ranges or native projectile speeds were changed during this measurement. The next useful combat evidence is controlled aimed encounters with moving targets, actual contact times, misses and successful evasions before choosing balance changes. No human fun or balance verdict is claimed.


The inspected close match render after this fix shows resting eye-free vessel bodies on the arena floor, with living vessels remaining upright and lit-eyed. The previous artifact is retained as `Saved/Screenshots/DominationCombatSignals_BeforeCorpseFix.png`; the newer view uses a closer camera, so these are not a controlled identical-camera visual comparison. The visible-surface measurements provide the grounding evidence. Large persistent bodies still accumulate on the point and obscure parts of the fight, despite being nonblocking. Corpse clutter, long-session rendering cost, steep-slope support and final character animation remain open. No claim of finished character art, enjoyable balance or production readiness follows from these scoped fixes.


## Moving-target ranged combat

A new controlled encounter uses the actual released Mi-Go Q, a native competitive hero target, and real strafe/sprint key polling and dash root motion. The initial synthetic key press was lost by the offscreen input fixture; the driver now sustains held key input while retaining normal acceleration, stamina, capsule physics and movement code. The earlier input-fixture failures (`RangedEncounterBaseline.log`, `RangedEncounterRenderedBaseline.log`) are not evidence of projectile accuracy.

`RangedEncounterHeldInputBaseline.log` established the actual behavior: the stationary control received one contact and 19.4 competitive damage, but steady strafing at 2708.33cm/s received zero contact. Dash, inserted cover and sprint cases also received zero. Projectile direction stayed fixed throughout. The reported damage includes the native level-one match multiplier; source damage was not increased.

Bot bolt casts now make one bounded planar prediction from the visible target's current velocity. Forecast is capped at 0.6 seconds and displacement at 60% of the projectile reach, with a 16m ceiling; predicted paths through solid cover fall back to the existing aim. There is no input anticipation, perfect intercept solver or in-flight steering. Human aim, beams, ground effects and melee delivery are unchanged. All canonical powers, cooldown values, projectile speeds and sizes remain unchanged.

`RangedEncounterPrediction.log` passed the first rendered prediction revision. Stationary and steady-strafe cases each received one actual contact and 19.4 damage. A real dash beginning after release, newly inserted cover and sprinting all avoided physical contact and damage. Direction change remained zero in every case. The manual direction probe also passed. These are representative Mi-Go encounters, not proof of balanced delivery for every released kit or a human fun rating.

A follow-up range check rejects bot bolt attempts whose target collision body lies beyond physical reach, preserving cooldown and stamina. Human blind shots remain permitted. The RMB direct-stun bridge now only starts its cooldown and emits its cast event when delivery actually succeeds. The final regression checks out-of-range rejection, resources, moving-target contact, physical counterplay and retained MOBA/jungle mechanics. `RangedCounterplayFinal.log` passed the rendered RangedEncounters, JungleCamps and MobaMechanics tests. Stationary and steady-strafe contacts each dealt 19.4 damage, while dash/cover/sprint cases produced zero contacts and zero damage. Range rejection preserved cooldown and stamina, spawned no stray actor, and the human direction probe passed. A later-counter timing revision and complete Domination/MOBA match regression follow below.


The final encounter revision records first physical contact time and starts its dash counter at 0.32s after cast (about 0.20s after the needle's nominal release), rather than the earlier 0.14s counter. The inserted-cover counter starts at 0.24s after cast. These are controlled timings, not a claim about average human reaction. `RangedMatchVerification.log` passed the normal-frame rendered RangedEncounters, DominationMatch and MobaMatch tests. The later dash avoided all physical contact after starting at the tightened counter time. Stationary first contact measured 0.418s and steady-strafe first contact 0.496s after cast, each with 19.4 damage. Dash, cover and sprint cases had zero contacts and zero damage; direction change stayed zero. The unreachable-shot resource assertions and exact human-aim probe passed. Domination retained four allied and five enemy capture participants and 1913.7 additional live battle damage; both physical flanks completed. Full MOBA routing, objective and match lifecycle assertions also passed. The original source `src/modes/MatchKits.cpp` explicitly converts Domination summons to a frontal mauling, so that existing mode behavior was retained rather than mistaken for an invented kit change.


The encounter results are saved in `Saved/Reports/MovingTargetEncounters.csv`. They demonstrate a specific improvement in aimed bot pressure and preserve representative counterplay; they do not establish the right difficulty for human players, moving-target balance for every released kit, or network fairness. No in-flight homing was introduced. The next production work needs warmed gameplay profiling, continued body/state readability and actual character art/animation integration, alongside broader controlled kit encounters and human playtest feedback. The full playable/fun/beautiful objective remains active and unproven.

## Warmed Free Roam runtime profiling

A new `CultUlhu.Menu.PerformanceFreeRoam` fixture enters the real menu and Free Roam, waits at least 18 simulation seconds and for shader compilation to settle, then captures 180 observation frames through Unreal's CSV profiler. Society, population, wars, construction, movement and rival hero ticks remain enabled. Its CPU timing scopes cover native kit Tick/Snapshot/Apply/Context; further scopes cover hero, population, creature, faith, society and world ticks. These are offscreen editor fixture measurements, not foreground or packaged-game FPS promises.

`PerformanceFreeRoamBaseline.log` passed with 459 actors, 188 population members and three heroes at capture start. `Saved/Profiling/CultRuntime/FreeRoam_20261020_232000.csv` measured mean kit Tick 0.8608ms, Snapshot 0.6014ms and Apply 0.2495ms per captured frame (all hero instances aggregated). Mean editor FrameTime was 61.4178ms and GameThreadTime 61.3548ms; the aggregate kit cost is only a small part of that budget. Engine exclusive scopes recorded TickActors 23.534ms, CharacterMovement 10.620ms and EndOfFrameUpdates 12.723ms. Those engine scopes have different boundaries from the new native scopes and must not be blindly added together. The capture also logged repeated civilian/scenery penetration; ordinary movement, navigation and modular visual-update costs need further investigation.

The native kit tick now skips Snapshot/Apply when it has neither a stateful RMB nor active core crowd-control effects. Actual casts, melee contacts and input still take fresh snapshots. Cooldowns/buff timers and MOBA minion hold/release always advance. Synchronization is decided before effect expiry so the last effect frame explicitly clears this caster's state without clearing other casters. The original Cthulhu wave state machine remains fully synchronized, including idle/cooldown updates; that path is not optimized by guessing whether its animation is idle. Follow-up measured results and test outcomes are recorded below when available.

`RuntimeSyncVerification.log` passed PerformanceFreeRoam, including native stun application after an idle frame, continued active synchronization, independent overlapping-caster state, explicit final-frame expiry, idle-state persistence and cooldown advancement. EditorInputSafety and MobaMechanics also passed in the same rendered run. The optimized capture `FreeRoam_20261020_232239.csv` had 475 actors, 184 population members and three heroes at capture start. Mean KitTick fell to 0.0208ms (95th percentile 0.0326ms), with Snapshot 0.0013ms and Apply 0.0005ms. There were occasional real casts, so those paths were not disabled. This is evidence that redundant idle synchronization was removed, not a strictly controlled full-frame A/B: the procedural population and scene changed between sessions.

Mean full FrameTime was 61.6913ms (95th percentile 74.4594ms), versus 61.4178ms in the baseline. No overall frame-rate improvement is established. New scopes measured aggregate PopulationTick 13.5694ms (95th percentile 18.0273ms), WorldTick 2.9267ms, including FaithTick 2.7063ms and SocietyTick 0.0065ms; CreatureTick 0.6193ms; HeroTick 0.0529ms. Population authored-pose updates, repeated enemy searches, collision avoidance and component work warrant focused measurement next. Offscreen editor Slate/view work and end-of-frame component transforms are also material. The retained modular NPC art uses many poseable components; changing its tick or leader-pose policy requires visual and corpse regression evidence. Repeated civilian penetration remains unresolved. No fun, visual-finish, stable-frame-rate or production-readiness claim follows from this pass.

The final `RuntimeSyncVerification.log` run completed all four selected tests successfully: EditorInputSafety, MobaMechanics, PerformanceFreeRoam and RangedEncounters. The moving-target delivery, dash/cover/sprint counterplay, human aim, out-of-range resource preservation and nonhoming assertions remained intact. Development Editor compilation succeeded. No canonical kit values, population limits or simulation rules were reduced to obtain the scoped CPU savings. The larger development goal remains active.

## NPC presentation cost and inspection preservation

The preceding goal turn made concrete progress: it removed idle kit scans and produced warmed profiling evidence, while leaving the full goal active. This continuation targets the larger measured population cost. New appearance subscopes separate authored pose sampling, primitive fallback limb animation and overhead inspection labels.

`PopulationVisualBaseline.log` passed with 473 actors, 187 population members and three heroes. `FreeRoam_20261020_232515.csv` measured aggregate PopulationTick 12.8898ms, Appearance 4.0068ms, AuthoredPose 1.5037ms, FallbackPose 0.7921ms and Labels 0.9995ms per frame. FrameTime averaged 60.3682ms. These are nested scopes: appearance subtimings are part of Appearance and PopulationTick, not additional cost to add on top.

Code inspection confirmed that every imported humanoid still animated its invisible primitive arms and legs, and invisible ALT labels still formatted captions and updated camera-facing transforms every frame. Fallback limbs now animate only when an authored body is absent. Authored animation, rest poses, faction badges, conversion/model changes and contribution timers still update normally. Inspection labels hide immediately when ALT is released, and format current activity, HP, construction progress and contribution text on the first visible tick. No population, AI, belief or simulation limit was reduced. Profiling-header include order was corrected to satisfy Unreal's matching-header-first rule after makefile regeneration exposed the issue; the subsequent Editor build succeeded.

A new rendered `CultUlhu.Menu.PopulationPresentation` fixture checks animated prayer/build/conversion poses with ALT released, fresh labels after opening inspection, all six modular body parts sharing a pose leader, immediate overlay hiding and nonblocking persistent corpse parts. Its screenshot is an isolated three-person inspection fixture on a temporary raised platform, not evidence of ordinary-world density or final environment art. Results follow after live verification.

`PopulationVisualVerification.log` passed ArmyCommands, CombatFeel, PerformanceFreeRoam and PopulationPresentation. The capture started with 445 actors, 176 population members and three heroes, compared with 473/187/3 in the preceding baseline. The first visual fixture passed its functional assertions but its screenshot was too distant and its temporary platform mesh assignment failed due to Static mobility. Its screenshot-existence check was insufficient to establish useful visual evidence. The fixture now sets movable platform geometry, validates the assigned mesh, uses a closer 60-degree camera and checks that the active camera really matches. A temporary fixture assertion compile error (TObjectPtr passed to a pointer-deducing TestNotNull template) was corrected to a null comparison. The next Editor build succeeded, and `PopulationPresentationClose.log` passed the corrected rendered fixture without the platform warning. The inspected `Saved/Screenshots/PopulationPresentation.png` visibly contains all three authored cultists with ALT activity/HP labels and construction progress/contribution text. The art remains stylized retained placeholder NPC art. Text is still small/grainy and lacks the legibility expected of polished tactical inspection UI; screenshot inspection does not justify a finished-UI claim.

The raw CSV files append a repeated header and positional metadata footer, including numeric fields. The initial ad hoc per-column numeric conversion could treat footer numbers as timing samples. `Tools/SummarizeRuntimeProfile.py` now explicitly excludes these records and requires a positive FrameTime. It was run against all four actual captures; `Saved/Reports/RuntimeProfileSummary.csv` contains 180 genuine frame samples per scope. Earlier figures in this audit have been corrected where the footer affected them. No raw capture was edited.

Corrected optimized capture `FreeRoam_20261020_232834.csv`: aggregate PopulationAppearance mean 1.7608ms / p95 2.2156ms, AuthoredPose 1.1051ms / 1.5203ms, PopulationTick 8.4401ms / 10.8991ms, WorldTick 2.3138ms / 2.5572ms and EndOfFrameUpdates 11.2708ms / 12.5016ms. Hidden FallbackPose and Labels scopes no longer execute in ordinary play. Full FrameTime was 51.0905ms / p95 57.9977ms, versus baseline 60.3682ms / 68.0267ms. These sessions had different procedural scenery and population, so this is not a controlled whole-frame speedup claim. Appearance cost normalized by capture-start population fell from about 21.4 to 10.0 microseconds per person, but population can change during capture and this is only a rough normalization. What the evidence directly supports is removal of invisible work while retaining authored animation, immediate inspection readouts, command behavior and corpse parts.

The next work should address inspection-label legibility and crowd overlap, remaining NPC perception/obstacle-query costs, component end-of-frame work and repeated civilian/scenery penetration. The overall game remains a prototype: final hero art/animations, visual coherence, human combat/balance feedback, sustained performance and packaged/network validation remain incomplete. The larger playable/fun/beautiful goal remains active.

## ALT inspection readability and crowd layout

The preceding goal turn made verified progress by removing invisible NPC updates and adding rendered animation/inspection/corpse checks. Its final screenshot exposed that world-space labels were still too small and grainy. This continuation replaces NPC TextRender components with on-demand HUD inspection cards. Runtime cards use staged project fonts, dark backing, current activity, faction/role, numeric HP and health bars. Selected or hovered people can show their recent belief contribution. Current work progress is formatted when inspected, rather than cached on every NPC tick. No authored NPC animation or command behavior is intentionally changed.

The renderer caps individual cards at eight, prioritizes hovered/selected people, tests at most 32 sightlines from the actual camera and excludes occluded, behind-camera and distant bodies. Cards have bounded screen-space placement and avoid one another, the top/bottom HUD and the command panel. Existing selection markers and selected-count summaries remain available for larger armies. Cards are Canvas presentation and do not create clickable hit boxes or consume input. Their renderer has its own CSV timing scope, and runs only during Free Roam inspection.

The real PopulationPresentation fixture now inspects actual HUD cards rather than invisible TextRender metadata. It also adds a crowded group, a wall-hidden unit and a behind-camera actor, and checks selected-worker retention, nonoverlap, occlusion, fresh construction/contribution data and ALT release. A separate geometry check exercises usable layout bounds and command-menu exclusion at 640x360, 1280x720 and 3840x2160. Build, rendered artifacts and results remain unproven until the running checks finish.

A separate source inspection found a packaging gap to address after this pass: authored-human clip loading still points outside the project to ../assets, while staging explicitly covers only some clips for two cultist skins. Editor animation results do not prove that all civilian/ritual/work/conversion animations survive packaging. This is a concrete outstanding item for the full goal, not a completed release validation.

`InspectionCardsVerification.log` passed ArmyCommands, EditorInputSafety, PopulationPresentation and InspectionLayout. Render inspection showed a substantial improvement in contrast and text clarity over the old world labels, but the selected builder's progress and contribution were visibly truncated despite green model assertions. The first-card images are retained as `PopulationInspection_FirstCards.png` and `PopulationInspectionCrowd_FirstCards.png`; these are intermediate evidence, not the accepted final UI. Focused cards were widened, and card snapshots now contain exactly the fitted strings passed to Canvas drawing. The regression requires the full rendered `48%` progress and `CONSTRUCTION + BELIEF` receipt. Layout checks now include both compact and detailed cards. Hover priority uses the closest cursor candidate; affiliation colors consult current war state instead of treating every rival cult as hostile. A local variable named Owner briefly triggered Unreal's shadowed-member compile error; it was renamed Hero and the subsequent build succeeded. The revised rendered regression is running in `InspectionCardsFinal.log`.

Source inspection also suggests the older command/selection UI needs a separate coordinate audit: several paths use full viewport mouse/projected positions while Canvas can use a constrained camera rectangle. The new inspection cards account for the letterbox offset; existing drag selection and command-panel hit testing have not been proven across aspect ratios. This is an outstanding usability check, not evidence that the new read-only cards consume clicks.

`InspectionCardsFinal.log` passed all four selected tests after the focused-card revision. Manual review of `PopulationInspection.png` confirmed full `Building 48%` and `CONSTRUCTION + BELIEF` text, along with clear faction/role and HP. The crowd image exposed a second limitation: a fixed-width placement search suppressed neighboring compact cards beside a wider selected card even when usable screen space remained. Layout now also searches actual occupied-card edges. The crowd regression requires at least three retained cards beside the selected worker, and multi-resolution checks require inspectable neighbors rather than merely one successful placement. The revised Editor build succeeded; `InspectionCardsCrowdFinal.log` is the running final layout/scene verification.

`InspectionCardsCrowdFinal.log` passed its strengthened neighbor-count and nonoverlap assertions. Visual review still showed that fallback placements could extend down over character bodies and that later connector lines crossed previously drawn text. The intermediate image is retained as `PopulationInspectionCrowd_BeforeBodyClearance.png`. The final renderer uses compact vertical spacing, requires cards to stay above their own head anchor, and draws every connector before all panel backgrounds/text. The 640x360/1280x720/3840x2160 geometry tests retain their neighbor-count requirement; it was not weakened to accept the first card only.

The final Development Editor build succeeded. `InspectionCardsBodyClearance.log` passed PopulationPresentation and InspectionLayout, including full fitted progress/receipt strings, native animated bodies, selected-worker retention, retained neighbors, wall and behind-camera exclusion, pairwise card nonoverlap, above-head placement, ALT clearing and persistent corpse parts. ArmyCommands and EditorInputSafety passed in `InspectionCardsFinal.log` before the final geometry/layering-only refinement. The final screenshots were inspected after their new modification times: `Saved/Screenshots/PopulationInspection.png` clearly shows the full builder progress and belief receipt, and `PopulationInspectionCrowd.png` shows six readable nonoverlapping cards above the crowded bodies, with no connector drawn over text and no card for the wall-hidden unit. These are isolated art/inspection fixtures with retained NPC art, not representative city-density or final-environment renders. No full-frame speedup, finished overall UI, human usability score or Overwatch-level polish is claimed.

The old TextRender allocation and per-NPC label update path are removed; the original text material asset remains in the project. Inspection is bounded to eight cards and 32 camera sightlines per draw, with no limit imposed on army size, selection or independent battle orders. Longer unfocused text may be ellipsized; focused cards have room for the verified representative receipt. Production work remains on broader HUD typography/spacing, command hit testing with constrained viewports, packaged animation paths, NPC movement/perception cost and physical scenery penetration. Final hero art/animation, human fun/balance feedback and sustained packaged/network validation remain unproven. The full development goal remains active.

## Camera-constrained HUD input alignment

The preceding goal turn made verified progress on readable, occlusion-aware ALT inspection cards. This continuation addresses the command-coordinate gap discovered during that work. Engine `GameViewportClient.cpp` initializes HUD Canvas from the camera's UnscaledViewRect and offsets rendering by that rectangle's origin; it then clears the HUD Canvas pointer. Existing native army/faith/result controls instead tested full-viewport mouse positions against their local drawn rectangles. Selection markers/footprints used full-viewport projection while Canvas drew local coordinates. This is a concrete coordinate-space mismatch; the new live-input regression is being run before changing those handlers.

A shared helper reads the actual LocalPlayer projection rectangle without depending on a Canvas pointer retained between frames. The test uses actual controller mouse warps and key events, the real ALT/LMB/B/ESC bindings, real drawn button rectangles and real director state. It exercises side bars, top/bottom bars and an unconstrained camera: two-unit drag selection with an unselected third follower, Prepare for battle, belief Outcomes/first-belief/Close clicks, and paused-match Resume. Reports record raw pointer versus local drawn-button positions. A test-only friend grants observation of existing button geometry; it does not bypass input dispatch or execute the tested commands directly. Baseline behavior remains unproven until the running regression completes.

`HUDCoordinatesBaseline.log` reproduced the mismatch using real mouse/key dispatch. The pillarboxed fixture had origin 221,0 and size 820x504; the letterboxed fixture had origin 0,42 and size 1261x420. Visible drag origins and belief/Prepare clicks failed with those offsets. The unconstrained view's belief clicks worked, but follower selection still returned zero in this raised scene; that also exposed the old visibility check's viewpoint mismatch. `ArmyHUDCoordinates_Baseline.csv` records actual raw pointer positions versus local drawn-button centers, and `ArmyDrag_*_Baseline.png` retains the rendered offset drag/footprint evidence.

The native handlers now convert the pointer using the live constrained view rectangle for army commands, belief hit testing/hover, match pause/result hit testing/hover and inspection hover. Selection drag, selected markers and ground-footprint projections use local projection consistently. Selection visibility uses the active camera, matching what is displayed. Full-viewport cursor deprojection for world aiming remains unchanged. A new click outside the camera rectangle is ignored; an existing drag can clamp to the view edge. Selected markers and footprint segments outside the rendered view are omitted to prevent drawing into camera bars. The regression now explicitly checks that camera-bar clicks cannot begin selection or open orders, and fails promptly if a misrouted UI click unexpectedly leaves gameplay. The shared mapping does not retain the engine's cleared Canvas pointer. Live post-fix checks follow below.

### Verified HUD-coordinate outcome — 2026-10-21

Development Editor builds succeeded. `HUDCoordinatesVerification.log` passed ArmyHUDCoordinates, MatchHUDCoordinates, EditorInputSafety and PopulationPresentation. Actual input selected exactly the intended two followers, left the third unselected, prepared the selected pair, clicked belief Outcomes/first belief/Close, and resumed an actually paused Domination match in pillarboxed, letterboxed and unconstrained views. Three NPC movement-timestep warnings occurred during the later population fixture load/settling sequence; no automation errors were reported. This is not evidence of stable overall frame time or resolved civilian/scenery movement issues.

Visual review caught a fixture state error despite those green assertions: between camera shapes the test called AltReleased to hide the cursor but did not release the simulated held ALT key. The letterboxed screenshot consequently lacked the visible drag overlay even though game input was dispatched. The fixture now releases the key as well and asserts both actual held ALT and the displayed command cursor during each drag. The following build succeeded. `HUDCoordinatesFinal.log` passed the strengthened ArmyHUDCoordinates and MatchHUDCoordinates tests in all three camera shapes. Only the test setup changed after the four-test verification run; runtime input mapping remained the verified implementation.

Final `ArmyHUDCoordinates.csv` contains 12 real command/faith clicks and `MatchHUDCoordinates.csv` three real Resume clicks. Raw cursor minus view origin matches the drawn button center within 0.4 pixels of recorded integer-cursor rounding. This is spatial agreement, not input-latency evidence. Camera-bar presses/releases did not begin selection or open orders. The final pillarbox, letterbox and unconstrained `ArmyDrag_*.png` images were inspected: the drag outline surrounds the intended pair and stays aligned with their bodies, while the third follower remains outside. The footprint is projected consistently and no longer drawn into camera bars. The isolated platform/art fixture and changing procedural background are not a final-game environment comparison. No claim is made about live window-resize races, split-screen, other platforms, every menu action's gameplay semantics or human usability ratings.

The remaining packaging inspection is now more specific: `CultPopulationAuthored.cpp` uses repository-relative ../assets animation paths without a packaged path, and DefaultGame.ini does not explicitly always-cook the civilian_villager art folder. The latter needs a cooked-content audit before claiming missing assets in an actual package. `CultMatchMinionArt.cpp` already uses staged CharacterAnimations outside editor builds for its four supported clips; it should not be falsely treated as having the same loader gap. Full packaged animation/asset verification, remaining movement/perception costs, final hero art/animations, broader HUD polish and human fun/balance feedback remain outstanding. The original playable/fun/beautiful and map-development objective remains active.

## Standalone animation and asset delivery — 2026-10-21

The preceding goal turn made verified progress on camera-constrained input and rendered selection alignment. This continuation audits the discovered packaged-human gap. Free Roam loaded every authored clip from ../assets, while build rules staged only four clips for each of two cultist skins. Civilian art also lacked an explicit cook entry. The new shared CultHumanAnimations.inl manifest lists the three existing NPC skins and their 16 original clips; build rules require and stage all 48 as NonUFS because the core parser uses std::ifstream. A byte comparison of all staged copies against the source found no mismatches. Native population and lane-minion art now share a validated loader/cache, preserving source-file authoring in editor and using only Content/CharacterAnimations outside editor builds. The existing 16-hero release allowlist is unchanged and deferred hero files are not newly staged.

Native file/rig verification covers all 48 clips, all six body parts/accessories per skin and actual imported bone matches. Native pose verification covers civilian/cultist/sorcerer activity poses and compares them with the expected original clip at actual observed sample times. Runtime mode-start selection is also checked through the real game-mode API. Editor builds can force staged clips; the standalone test refuses an adjacent repository animation tree. These checks are not yet final packaged evidence until cooking/staging and executable verification succeed.

The first actual game-target build failed because its V5 build settings conflicted with UE 5.8's shared UnrealGame environment. The target now uses V7 rather than suppressing validation. The next compile exposed unguarded editor-only actor-label functions; failures are retained in Saved/Logs/StandaloneCompileBaseline.log. Debug label writes are now editor-only, and rubble/scorch debug names use the runtime-safe ActorNameOrLabel accessor. Mode spawns and the menu camera were also identified by editor labels, so those lookups are being migrated to serialized PlayerStartTag/Actor.Tags rather than silently falling back to arbitrary runtime object names. PrepareRuntimeAnchors.py changes only that metadata on the existing maps; Content is backed up in Saved/Backups/BeforeRuntimeAnchorTags.zip. The standalone Development executable has compiled successfully, with the final runtime verification test update still to be included by the next build/cook step. A compiled executable alone does not prove packaged assets or gameplay.


## Independent standalone delivery and visual baseline — 2026-10-21

The preceding goal turn made authoritative progress by backing up the playable
project and authored assets in GitHub draft PR #2, preserving the live upstream
migration, correcting generated-output exclusions and removing 2.47 GiB of
obsolete editor PCH/object cache. This continuation completes actual Windows
Game compilation, cooking, staging and executable verification, rather than
inferring packaged behavior from PIE. The missing upstream validation/setup
scripts were restored. The retired local unreal directory was moved without
copying or deleting its contents to Source_Backup_LegacyUnreal_20261021; the
canonical validator now passes. No retired project is built.

The initial standalone run passed the humanoid file/pose tests but reported 16
unique Nanite usage fallbacks across original and living rock/tree/crystal
materials. It also could not load the dynamically referenced daylight skylight
cubemap. These were real presentation defects despite the narrow tests passing.
PrepareNaniteMaterials.py persists Nanite usage on the 16 actual material
interfaces, using UE 5.8 per-instance flags for imported materials with engine
parents. The eight living base materials also retain their usage flags; textures,
colors and graph connections are preserved. BuildLivingMaterials.py now sets
Nanite usage when recreating living copies. Explicit cooking of the existing
engine sky directory supplies the runtime cubemap. The strengthened native test
asserts that the actual TextureCube loads.

The final Editor and Game builds succeeded. Windows BuildCookRun succeeded;
the temporary stage was 638.1 MiB with 48 loose .canim files and no PDBs. Its
independently launched executable, without a source-project argument or adjacent
assets tree, passed CultUlhu.Runtime.HumanAnimationFiles and
CultUlhu.Runtime.StagedHumanPoses with Editor=0. That verifies all three imported
humanoid rigs and original clip tracks, six observed activity poses per skin,
actual tagged mode-start selection, the grounded player and cooked skylight.
Final runtime counts were zero material-usage fallbacks, zero missing cubemap
warnings, zero automation errors and zero warnings in this short run. Earlier
movement warnings remain an unresolved concern; no movement or performance fix
is claimed here. Baseline/intermediate/final logs remain distinct.

Saved/Reports/StandaloneDeliveryFinal.json records the executable SHA-256
051549730480aa8b08834d5f80a6b1a6cfb910e5f7228ae247c334e4b4826a5c
and screenshot hash. Saved/Screenshots/StagedFreeRoam_20261021.png is an actual
initialized game frame captured by the runtime fixture before it raises the
isolated pose platform. It was visually inspected. The earlier startup
HighResShot was black and is not evidence of presentation quality. The useful
frame shows a bright blue/gray blockout, pale monumental columns and temple,
sparse visible ground detail, the placeholder Mi-Go vessel and a prototype HUD.
The Nanite fix removes material substitution; it does not supply final art or
make this dark Lovecraftian presentation coherent. It is plainly below the
requested professional refinement. A consistent darker world palette, authored
hero silhouettes/animation, better environment detail and HUD hierarchy remain
necessary, as do sustained performance and human combat/match evaluation.

After the final process exited, its useful screenshot, coverage CSV, hashes and
logs were preserved outside staging. The temporary standalone package, cooked
metadata, Game-target build cache and Game executable/debug outputs were removed,
freeing another 3.74 GiB. The current editor DLLs and editor build cache remain.
No package or debug symbols are uploaded to GitHub. This is Free Roam/asset
runtime evidence; actual packaged competitive matches and all 16 individual
hero kits are still unverified. The full objective remains active.


## Free Roam dusk and cave readability — 2026-10-21

The preceding goal turn made authoritative progress on independent executable
asset delivery and preserved an initialized standalone image. That image showed
a bright blue/gray blockout, not a coherent dark Lovecraftian world. The current
source confirmed that Free Roam retained the old adaptive-exposure/daylight rig,
while competitive arenas already used fixed exposure. CultWildsPresentation now
applies a runtime-only Free Roam exposure/moonlight/fog profile, a generated dusk
background and bounded cave fill. Imported art and authored maps are preserved;
competitive modes retain their existing path. No ability, AI, movement or
collision dimensions are changed.

Rendered comparisons rejected the first dark pass: cave creatures and floor
were too difficult to read, and the background became a black void. A subsequent
sky material introduced a coverage warning and still rendered black. A focused
bright-material diagnostic proved the dome itself was visible: its registered
bounds were centered at the origin with 500000 cm extents, and the diagnostic
covered the intended background. The generated gradient was rebuilt after an
8 KB backup, with explicit single-input pin connections, an explicit G-only
UV mask and assertions for every connection. Traversal verifies all seven nodes
are connected to emissive. The final ordinary unlit material is 9485 bytes and
uses no new textures. The temporary magenta diagnostic was removed from the test.

Two nonshadowing fill lights per existing cavern now make the native creatures
and floor visible; each is limited to a 100 m radius and 250 m draw distance with
a 50 m fade. Their cost is bounded by those settings but has not been measured.
The sky is noncolliding and casts no shadow. Native comparison checks an upward
visibility trace through it so background art cannot become an aiming target.

The final canonical path validation and CultUlhuEditor build succeeded.
WildsPresentationFinal.log passes WildsPresentation and PopulationPresentation,
with zero automation errors or material-usage fallback warnings. Six underlying
NPC movement-timestep warnings appeared during world entry (and were repeated
in automation event reporting); no movement/performance fix is claimed. The
comparison pauses one generated world, captures courtyard/cave before and after,
checks the intended actual camera, the player's physical transform and supporting
floor collision, then restores test state. The final four frames were inspected:
the dusk background renders and cave creatures/ground are distinguishable. Their
hashes are recorded in Saved/Reports/WildsPresentationSummary.json. Frame-rate
proof, all displays/angles and human playability are outside these checks.

This is a real mood/readability improvement, but the environment still has
blockout shapes, flat material treatment, large sparse areas and placeholder
hero art. It remains well below the requested professional presentation; combat
fun is not established by a lighting test. The new profile has not yet been
recooked/run in a standalone package. No new standalone package, download or
large asset archive was created in this pass. The full original goal remains
active, including mode quality, living-world behavior, visual refinement,
performance and human combat/match evaluation.


## Population decisions, physical response and CPU cost — 2026-10-21

The preceding goal turn made verified progress on Free Roam lighting, a working
dusk gradient and visible cave actors, while explicitly leaving frame time and
movement warnings unresolved. The current warmed 192-person editor baseline
averaged 90.060 ms/frame, with PopulationTick 17.955 ms, CharacterMovement 14.525 ms
and EndOfFrameUpdates 19.406 ms. GPU time was 10.113 ms. That is plainly inadequate
for responsive combat; this fixture is not a foreground FPS measurement.

Population decision work is now separated from continuous movement. Appearance,
control projection, order/attack timers and native physics keep ticking every
frame. Routine target/obstacle/ground decisions have a default minimum interval
of 0.1 seconds, with cached movement input fed every frame. Accepted orders and
role/control transitions bypass the wait. Fear is still evaluated each frame.
Enemy/war selection, battle ownership, loyalty/belief rules and native attack
windups are preserved. The captive-follow branch also now respects both stun and
root/stop, rather than adding follow input after the movement stop. No control
snapshot lifetime is extended.

The final canonical Editor builds succeeded. PopulationSchedulingVerification.log
passes ArmyCommands, PopulationDecisionProfile, PopulationMotion and
StagedHumanPoses with zero automation errors. The actual movement fixture verifies
continued walking between two-second diagnostic decisions, reversal within 0.4 s
of a new order, held stun/root preventing captive following, recovery, a delayed
physical hit from a zero-loyalty battle order, and braking at the real platform
edge. The final root X was 2819.2 cm on a floor ending at 3000 cm. Original human
activity poses remain verified. This is narrower than all NPC navigation cases.

The new profile alternates A-B-B-A in the same evolving Free Roam world, retaining
all AI/faith/war/construction. Four 120-frame windows have a constant population
of 182 and three heroes; total actors rise 480→492 as construction continues.
Per-frame-decision windows average 12.710 ms decision time, 17.534 ms PopulationTick
and 89.532 ms FrameTime. Scheduled windows average 6.389 ms, 10.585 ms and 79.758 ms,
respectively. PopulationTick drops 39.6%, with a 10.9% frame-time reduction in this
fixture. Mean decision calls fall from about 173 to 87 per frame. The final A
window returns to higher costs; the earlier separate 192-person baseline is not
used for these percentages. Counts are exported separately from ms so the new
counter fields cannot be mistaken for durations. Per-window p95s, raw CSV hashes
and context are in Docs/PopulationSchedulingResults.json.

This is a useful reduction, not a performance solution. CharacterMovement remains
13.615 ms and EndOfFrameUpdates 18.748 ms in scheduled windows; GPU time is about
10.68 ms. Decision work still clusters on some frames. Movement-timestep warnings
and occasional stuck-unit cases are not resolved by this change. Overall editor
frame time remains far above a responsive-action target. Further investigation
of movement/collision, component updates and work distribution is required.
Standalone scheduling behavior/performance, broader navigation and human combat
quality remain unproven. No package/download was added; the full goal remains
active.

## Shared humanoid rendering — 2026-10-21

The previous goal turn recorded the standing storage policy. Current work targets
measured component-update cost rather than adding visual assets. Civilian,
hooded cultist and magus bodies now share one runtime merge per skin, retaining
all six original source meshes and the modular fallback. The same path supplies
MOBA minion bodies. No baked mesh duplicates or external downloads are added.
Only the CPU-access setting changes on the 18 source mesh assets; their previous
bytes were verified recoverable from the existing GitHub checkpoint.

The original rest positions, primary UVs, materials, reference bones, bounds and
vertex/triangle totals pass native geometry checks for all three skins. Original
activity clips also pass on real combined population actors. Engine merge code
handles skin weights; this is not an exhaustive pixel-identity proof.

In one evolving Free Roam Editor world, four 120-frame modular/merged/merged/
modular windows retain a constant 191 people. Actual skinned components fall
from 1,146 to 191. Paired whole-frame means fall from 74.239 to 53.193 ms (28.3%),
and end-of-frame updates from about 17.43 to 8.98 ms (48.5%). These are warmed,
offscreen Editor measurements with native simulation active. Per-window timing,
counts and raw CSV hashes are in Docs/HumanMeshEditorResults.json. This is still
poor action-game performance. It does not establish enjoyable combat, balanced
modes or a production-quality visual result.

The first combined regression batch passes army commands, editor-input safety,
merged original activity poses, MOBA match and MOBA mechanics. PopulationMotion
fails before creating its subject, followed by a PopulationPresentation timeout.
The log and Engine source identify asynchronous test startup: FStartPIECommand
queues Play and completes immediately, so the next observation can issue travel
into the preceding test's closing world. The affected native fixtures now wait
for a fresh initialized PIE world, and match tests request cleanup. These failed
results remain part of the record; a rerun must pass before calling those checks
verified. A stricter minion check additionally requires actual spawned lane
bodies to contain one certified, complete skinned component.

Cooked CPU-buffer availability and standalone performance on this newer shared
mesh path remain unproven pending an independent executable check. Movement
warnings, stuck-navigation cases, remaining frame time, final art and human match
quality remain unresolved. The full development goal is active.

Final Editor reruns: HumanMeshFinalEditorRerun.log passes MergedHumanPoses,
PopulationMotion and PopulationPresentation. Native order/control/combat/cliff
checks finish at X=2808.1 on the same real floor edge. The rendered inspection
fixture was inspected: complete bodies, original materials and activity poses
remain visible. It is an isolated presentation fixture, not normal world density.
The new minion check initially ran before the first wave had deployed; it now
waits for real lane units. HumanMeshMinionFinal.log passes MobaMechanics, including
one certified complete skinned component on each observed spawned lane minion,
physical tower shots and the actual held/launched-minion impacts. Failed logs
are retained; final verification scopes and SHA-256s are in the results JSON.
The canonical Editor rebuild succeeds. Standalone verification is next.

## Independent shared-mesh executable check — 2026-10-21

The preceding turn made verified progress on shared human rendering and backed
its explicit source/asset manifest to GitHub (6ae14be). This turn resumes the
same confirmed-live build rather than restarting it. The Development Game target
compiles in 708.4 seconds and Windows cook/stage succeeds (842.4 seconds total).
VerifyHumanMeshRuntime.ps1 runs the staged executable without a source-project
argument and checks native test results rather than trusting exit code alone.
HumanMeshMerge, HumanMeshProfile and MergedHumanPoses all pass with Editor=0 and
zero automation errors. Cooked source CPU buffers, original mesh data and the six
activity poses on complete one-component native bodies are verified.

Four 120-frame modular/merged/merged/modular windows retain 190 people and three
heroes. Skinned-component snapshots are 1,140 / 190 / 190 / 1,140. Mean frame time
is 32.260 ms modular versus 24.697 ms merged (23.4% reduction). Game-thread means
are 32.030 / 24.607 ms; GPU means 11.160 / 10.871 ms. Movement remains 6.838 ms in
merged windows. The live actor count grows 496 to 499 as construction continues.
This is still CPU-limited and above a 16.7 ms frame budget; it is not foreground
FPS or human match-quality evidence. Per-window p95s, count data and executable,
CSV, build/runtime-log and image hashes are in HumanMeshStandaloneResults.json.

The actual initialized standalone image was inspected. The dusk background,
terrain and temple render; broad empty ground, simple temple shapes, plain
materials and the placeholder player remain far below the requested visual
benchmark. This run does not prove packaged cave visibility or competitive-mode
quality. The runtime log contains 36 stuck-under-building warnings, which justify
physical NPC-clearance investigation rather than suppressing the warnings.

The temporary stage is 638.93 MiB, with 48 original animation clips byte-compared
against source and zero PDBs. Measurements, logs and the image are retained; all
checked task-owned standalone/cook/Game build output is removed (3.749 GiB).
The working Editor and authored/user assets remain. Later NPC navigation edits
are outside this executable checkpoint's scope. The full goal remains active.

## Reproduced and corrected NPC-clearance failures — 2026-10-21

A physical fixture reproduces four failures before editing steering: the native
person cannot pass a low head obstruction, a surrounded person keeps sending
movement input and pushes against its wall, and a ramp approach ends below the
ramp rather than climbing. The code used a center-height sphere rather than the
body capsule, retained its original direction when every trial was blocked, and
could reject walkable floor contact as an obstacle.

The corrected steering probes the scaled body capsule, accepts nonpenetrating
walkable contact near step height, rejects walls/ceilings/initial penetration and
leaves intent zero if all directions are blocked. The canonical Editor build
succeeds. ArmyCommands, PopulationClearance and PopulationMotion all pass. The
beam subject reaches the far side; the boxed subject remains at its origin with
zero input; the ramp subject climbs about 294 cm while grounded. Existing native
order/control/recovery/physical-combat and real cliff-braking checks remain green.
Docs/PopulationClearance.md and PopulationClearanceResults.json record the exact
positions, before failures, final successes and log hashes.

These are bounded local-steering cases, not full navigation or universal overlap
recovery. City civilians are also spawned during the building-placement loop;
later placements can invalidate an earlier clearance check. That separate risk
needs investigation. No independent executable or frame-time claim is made for
this later NPC change; the shared-mesh standalone measurements precede it.

## Settlement birth and occupied footprints — 2026-10-21

The preceding turn made verified progress on standalone shared-mesh rendering,
physical NPC clearance and temporary-output cleanup. This pass tests the separate
city population-before-final-building risk. In four actual initialized worlds,
the baseline input 29 has two civilians intersecting completed LivingCity meshes;
inputs 11, 73 and 151 have no intersections. The failed result is retained.

Cities now place all their geometry before creating residents, using bounded
terrain/capsule-clear attempts across the settlement's 60 m ellipse. The target
resident count is retained. Imported mesh pivot, dimensions and yaw are factored
into one shared placement transform. A conservative oriented volume rejects
building candidates occupied by existing pawn or live non-pawn creature bodies,
then the generator searches another candidate. Existing characters are not
teleported or removed by that rejection. Temple and construction callers retain
their existing placement workflow through the extracted shared spawn helper.

The canonical Editor builds succeed. CityPopulationClearance passes all four
sampled RNG inputs with zero embedded people, retaining 146/169/164/174 city
buildings and 180/192/189/193 people. Snapshot times are about 0.04–0.40 seconds;
this is initial placement evidence, not a long-session simulation claim. Layouts
change with random draw consumption, so these are not identical before/after
geometry pairs. SettlementFootprints passes actual civilian and ghoul protection,
clear nearby placement and the civilian's unblocked capsule. The final rerun also
checks actual spawned mesh dimensions, yaw and a base on the requested ground.
ArmyCommands and PopulationClearance remain green after the refactor.

Docs/SettlementPopulation.md and SettlementPopulationResults.json record the
scope, failure, counts and final log hashes. Conservative bounds can reject an
otherwise empty archway. Full navigation, every procedural arrangement, later
construction completion around non-workers, sustained population performance,
final art and human match quality remain unresolved. These later edits have
scoped Editor evidence, not a new independent executable. No content asset,
download, duplicate model or standalone package is added. The full goal is active.

## HUD contrast, measured rows and capture fidelity — 2026-10-21

The preceding turn made verified settlement-safety progress and backed it to
GitHub. This pass investigates suspected right-edge control-hint clipping.
Actual Editor Canvas evidence does not reproduce misalignment: measured/drawn
widths both equal 291 px, with the intended margin, runtime font caching and
recorded DPI 1. The observed bright-column frame instead loses contrast. No
arbitrary font offset is introduced as a fictitious correction.

Compact dark backing and a stronger mode title improve header contrast. Narrow
competitive controls wrap clear of the score panel. The first passing header
matrix nevertheless reveals bottom collisions visually at 640x360: wrapped
ability names enter their state rows and movement instructions enter the first
card. Measured key/name/state heights replace fixed shrinking coordinates;
dash/sprint cues fit the gap between vitals and abilities; RMB/notice rows stack
above the cards. ALT inspection respects the current combat exclusion. Footer
bindings and movement cues receive backing against bright effects as well.

A PNG-header audit then rejects an insufficient capture method: logical canvases
could be larger than the captured editor backbuffer after input/eject tests.
Earlier successful geometry checks do not prove those larger screenshots.
Dimensions/hashes of the cropped files remain in the record. The corrected
fixture uses engine dedicated render targets, samples those exact HUD draws,
requires actual PNG width/height inside the native test, and restores capture
settings. This is exact-size offscreen rendering, not universal physical-window,
DPI, projection or frame-rate proof.

The final canonical Editor builds succeed. HUDContrastFinal.log passes all eight
requested tests with zero automation errors: ArmyHUDCoordinates,
EditorInputSafety, HUDTextRendering, MatchHUDCoordinates, MatchTextRendering,
MobaLiftTextRendering, MobaTextRendering and PopulationPresentation. Twelve final
PNGs are independently checked at 640x360, 1280x720 and 1920x720 for Mi-Go across
all modes plus Cthulhu in MOBA. Actual glyphs retain margins, mode/score clearance,
wrapped ability/state separation, movement cues and the explicit native minion
instruction. Normal-view cursor, ALT selection/inspection and persistent corpse
checks also pass. Selected final small, normal, wide, Cthulhu and inspection
renders were viewed; the targeted collisions and bright-background readability
are improved. Two small separate-live-capture images make the card change
reviewable; they are not an identical-world pixel comparison.

Docs/HUDReadability.md and HUDReadabilityResults.json preserve the bounded scope,
intermediate insufficient capture evidence, observations and final hashes.
Long competitive messages still overflow at small width, allied name tags can
overlap, and placeholder vessels obscure the camera near grouped spawns. Overall
professional presentation, full roster/display coverage, final art, human combat
quality and sustained performance remain unproven. Runtime.HUDTextRendering can
verify a later independent build, but these edits have not been recooked/run
there in this pass. No content download, model duplicate or standalone package
is added. The full goal remains active.

## Match spawn camera follow-up (2026-10-21)

Native baseline camera checks reproduced an allied hero under Mi-Go's initial
crosshair in Domination and MOBA; Cthulhu did not reproduce that specific hit.
Mirroring the spawn row cleared the ally hit, but visual review found a temple
column blocking the new view. The row is now also shifted inward, and the
camera fixture rejects solid obstructions behind the player's body plane.
Scale, capsule height, forward spawn distance, first/third-person switching and
real wall retraction are preserved. No camera collision was disabled.

Docs/MatchCameraFraming.md and CameraFramingResults.json record baseline,
intermediate and final evidence. The reviewed final competitive entry frames
move that column off the crosshair, but nearby placeholder bodies still occupy
the lower-left view. This is a bounded spawn-view correction, not a complete
camera/presentation solution or all-roster test. Only C++ and small text records
are added; the earlier temporary standalone/cooked outputs remain absent.
Independent verification of these newer edits and the broader goal remain work.

## Objective information during combat (2026-10-21)

Source inspection found that every credited hero kill replaced the same notice
used for captures, overtime and defense announcements. Eliminations now use a
bounded, timed feed while retaining scoring/XP/assist/respawn behavior. Pause
preserves objective copy and feed lifetime. Measured competitive copy wraps
clear of the tactical map, progress/relic rows retain contrast, and projected
objective guidance stays within the play area and clear of the announcement.
Backplates fit their copy instead of occupying unnecessary full-width space.

Docs/MatchEvents.md and MatchEventsResults.json distinguish normal death-handler
render fixtures from the physical Domination damage/death check, and preserve
the initial failed test assumption and later evidence. An existing Cthulhu
render assertion incorrectly required the MOBA-only minion instruction in
Domination; its mode guard is corrected. The actual game ability is unchanged.
Small screens show one recent elimination; larger samples show three. Long
names and optional compact metadata may abbreviate, with match statistics still
available on the scoreboard. This is a match-readability improvement, not new
proof of human fun, final art, complete navigation or professional game quality.
Only source and small evidence records are added; no new package or download.

## Summon reward consistency (2026-10-21)

Native baseline fixtures reproduced zero owner damage statistics for summoned
hero damage, missing local feedback, a prior direct attacker taking a summon's
kill, shared rather than last-hit MOBA rewards, friendly summoned hero damage,
and stale-owner healing. A common explicit-summon attribution helper now routes
credit and feedback to a valid same-faction hero owner, while preserving the
creature as the physical damage source. Jungle attribution uses the same rule.
Direct hero behavior, reward values, kit definitions and projectile geometry
are retained. A defeated owner may receive a surviving summon's kill; the
healing passive cannot resurrect that owner. Ordinary owned civilians and
arbitrary actor ownership chains do not become hero damage sources.

Docs/SummonCombatCredit.md and SummonCombatCreditResults.json distinguish the
isolated damage/reward fixtures from actual projectile and full-match coverage.
The baseline is preserved rather than inferred solely from source. This makes
summoned combat rewards and feedback more dependable, without establishing
complete summon AI, all-kit balance, multiplayer credit or human combat fun.
No new assets, download or independent game package are added.

## Competitive surface treatment (2026-10-21)

The arena still exposed untextured collision blocks and broad flat floors. A
small original world-aligned material now gives tagged walls/floors/ramps
masonry scale, joints, roughness and subtle normal/weather variation. MOBA
roads remain paved while surrounding ground and mesas use a non-masonry
profile. The initial all-paved MOBA ground was rejected in visual review.
No imported art or gameplay geometry was replaced. 145 existing World/material
assets compare byte-for-byte with the previous checkpoint.

Native paired scenes validate unchanged actors, player transform, meshes,
visibility and collision, and an actual compiled material shader. All nine
final camera/surface/physical-mode/lane-clearance tests pass. Eight dedicated
1280x720 captures and selected reviewed gameplay/overview frames make the
material improvement concrete. Frozen spawn-camera A-B-B-A windows measure
GPU differences of +0.026 ms in Domination and +0.013 ms in MOBA on this host;
they do not establish live-battle or low-spec FPS. A first link failure exposed
and resolved the Editor verification's missing RHI dependency.

Docs/MatchSurfaces.md and MatchSurfaceResults.json retain the scope, rejected
initial presentation, hashes, timings and limitations. This improves surface
detail while leaving primitive silhouettes, repeated masonry, smooth mesas,
sparse dressing, placeholder entities and crude art transitions visible.
Professional game presentation, human combat quality and the full goal remain
unproven. Only a small material/source/evidence change and two paired images
are added; no texture download or standalone package is created.

## Jungle cliff silhouettes (2026-10-21)

Smooth cylindrical jungle platforms now use closed procedural cliff meshes.
An initial conservative contour passed checks but was rejected visually as too
similar to the cylinder. Stronger side erosion, layered faces and protected
rock/ramp pockets improve the silhouette while preserving camp heights and
access routes. Rendering and collision use the same triangles. The old cylinder
exists only in the Editor comparison fixture, with only one representation's
collision active at a time. Materials and the tactical map now support the
procedural components; map footprints remain bounds rather than exact contours.

All seven final Editor checks pass: triangle/surface correspondence (1,440),
30-metre capsule support (20 samples), mirrored contour rays (24), encounters,
all 12 minion returns, lane clearance, a full-size high-ramp climb, physical
MOBA play, material invariants and HUD rendering. Four exact-size captures and
paired west-face review images make the shape change reviewable. Frozen-view
A-B-B-A windows measure GPU means of 3.217 versus 3.228 ms; those numbers are
not live-battle or low-spec performance proof. Docs/JungleCliffs.md and
JungleCliffResults.json preserve scope, the rejected initial shape and hashes.

Existing models/materials and encounter balance are reused. No model download,
new static asset or standalone package is added. Stylized crowns, regular ramps,
sparse dressing, crude props and placeholder entities still prevent a claim of
professional final art; all-kit traversal, human balance and the full goal
remain unproven.

## Competitive bot defensive reactions (2026-10-21)

The old proximity-only windup rule reproduced four failed assertions per mode:
aimed-away, blocked and vertically separated attacks wasted dashes, while a
flying bolt was ignored. Bots now forecast existing skill geometry against
their scaled capsule, require clear observation/attack paths and react after
0.16–0.21 seconds. Stamina, cooldown, control state and swept support/obstruction
checks gate escape choices. Short escape commitment prevents immediately
walking back into the same danger. Respawn clears the defensive memory.

Both focused Editor tests pass: delayed negative/positive decisions through
real BotThink, wall and ledge rejection, and freely ticking physical projectile
controls on an isolated platform. Dagon moved laterally about 19.6/20.8 metres
without projectile contact in Domination/MOBA; each stationary control received
the same real 20-damage projectile. Objective navigation and other fighters
were held for that isolated physical comparison. Original dash parameters,
player Shift handling, damage volumes and authored ability definitions remain.

This is neither a human perception model nor proof of general evasion. There
is no field-of-view restriction, swept-arm forecasts use spokes, and special
thrown-minion transit is outside this skill-attack observer. All-kit/terrain
behavior, tactical depth, sustained battle cost, balance and human fun remain
open. BotThreatResponse.md and BotThreatResults.json record the scoped evidence.
No new art assets, downloaded models or standalone package are added.

Eight distinct Editor checks ultimately pass. The initial six-test regression
batch passed five and exposed a standing-hit control that manually ticked an
attack in its birth frame; the existing first-frame safeguard discarded that
artificial delta. SkillCombat now waits for real world ticks, preserving its
standing-hit and jumping-clearance assertions. Its focused rerun passes with
no gameplay edits. Domination independently observed four allied and five
enemy seal participants; MOBA observed all nine bot lane entries. The final
canonical build succeeds. All 423 Content files remain byte-identical to
checkpoint 8888706; character inputs have only checkout line-ending differences.
This verifies bounded behavior, not human match quality or final art.

## Competitive hero readability (2026-10-21)

The reviewed combat image exposed another fundamental gap: similar placeholder
vessels had little nearby identity or health information except for one central
target panel. Competitive heroes now receive bounded contextual name/health
plates, prioritizing combat focus and injury. Team marks differ in shape and
color. Real camera-to-body/head rays gate visibility; remembered focus cannot
bypass cover. Names use measured glyph widths, layout avoids reserved HUD
regions, and only four cards (two on short views) appear. This does not supply
the missing character silhouettes or animations.

Initial renders exposed controller-versus-Canvas projection mismatch and an
invalid wide-capture fixture wait. Actual Canvas projection now also positions
objective guidance, damage numbers and small windup labels. Subsequent tests
caught compact target-priority failures, especially beside MOBA's contextual
minion-throw instruction. Head-centred side placement corrected those cases;
floating damage labels yield to that lower instruction. Visual review after
five passing tests still found leaders crossing earlier names, so all leaders
now draw beneath all cards. Rejected evidence remains recorded.

The final canonical build and all five final native tests pass with zero
automation errors. Twelve plate captures cover real health, actual glyph
advances, focus styling/priority, compact/wide/constrained layouts, bounds,
overlap and real-wall occlusion in both modes. Combat feedback and competitive
event glyph regressions also pass. Two frozen-scene review images provide the
comparison; HeroReadability.md and HeroReadabilityResults.json record scope.
All 423 Content files remain byte-identical to ab3ee27, with no substantive
character-input changes. This adds source, reports and two review PNGs, with
no asset download or standalone package.

The control scene is prepared and paused, rather than a human firefight. Text
comfort on tiny displays, crowded-fight stability, sustained performance,
all-camera coverage and human decision quality remain unmeasured. Placeholder
bodies and crude scenery are still far below the requested Overwatch benchmark;
fun comparable to Onslaught and the full objective remain unproven.
