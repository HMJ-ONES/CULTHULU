# Competitive bot defensive reactions

The previous defensive rule dashed at any nearby winding-up skill. It measured
horizontal distance plus a large allowance, without checking direction, cover
or height, and ignored projectiles after their windup. The original native
baseline failed four assertions in each competitive mode: an aimed-away beam,
a beam behind a solid wall and a vertically separated aura all wasted a dash;
an incoming active bolt did not trigger one.

Bots now forecast the existing attack volume against their scaled capsule.
Bolts use their current flying position and remaining path; beams, sweeping
arms, cones, spheres and ground disks use their existing dimensions. Cover
queries reject blocked paths and observations. Predictions consider threats
within 0.55 seconds, sampled at most once per 0.06 seconds per bot. Recognizing
a new threat starts a 0.16–0.21 second reaction delay, mirrored by roster role.
Threat changes reset that delay. This is deterministic defensive behavior,
not an assertion of human-equivalent perception: there is no field-of-view
restriction, and reactions use attack data rather than image recognition.

The bot still needs the same stamina, cooldown and control state as the player.
A standard dash retains its 18 stamina cost, 1.15 second cooldown, 0.22 second
duration, 3.5-times walk-speed impulse and existing 0.18 second evade window.
The shared CanDash/travel-distance queries avoid duplicating those parameters.
Player Shift tap/hold handling and original character kits are unchanged.

Candidate escape directions receive swept-capsule wall and floor checks at
three points, with centre support rays and the movement component's walkable
slope/step limits. A blocked or unsupported path is rejected; opposite and
diagonal alternatives are tried. A short escape commitment stops objective
movement immediately turning back into the observed attack. Respawning clears
the old threat memory. Actual attack collision and damage are unchanged.

## Verification scope

BotThreatResults.json records the final build and native test results. Baseline
evidence is in Saved/Logs/BotThreatBefore.log; it predates the stronger delayed
negative observations and physical/safety fixtures added to the final tests.

The decision fixture invokes the real match BotThink in both modes, observing
the harmless/blocked/high attacks twice across the reaction interval and
checking that a flying bolt causes a delayed, rather than immediate, dash.
The physical fixture then uses an isolated flat platform, a real Dagon capsule,
root-motion movement and ACultSkillAttack projectile sweeps. With objective
navigation and other fighters held, it compares a reacting bot with a stationary
control under the same projectile. Wall and platform-edge queries separately
check landing safety. This isolates defensive behavior; it does not establish
all-kit evasion, all terrain, or human match quality.

The broader regression run also exposed an older SkillCombat fixture error:
its standing control manually ticked an attack in its birth frame. The existing
first-frame safeguard correctly clamped that artificial delta to zero, so the
control never activated. The fixture now lets that control tick in the world
before checking contact and launching the jumping comparison. The original
standing-contact and jump-clearance assertions remain; no gameplay hit rule
was weakened. The report retains that failed run as well as the focused rerun.

All eight distinct Editor checks ultimately passed: the two threat fixtures,
CombatFeel, DominationMatch, EditorInputSafety, MobaMatch, MobaMechanics and
the corrected SkillCombat. The six-test regression batch first passed five
checks and failed that standing control; the focused rerun passed with zero
automation assertion errors. Gameplay source did not change for the rerun.
The final canonical build succeeded in 9.83 seconds. No standalone executable
was produced for this phase. Against checkpoint 8888706, all 423 Unreal Content
files are byte-identical; 115 character-source files have no substantive
differences (110 differ only in line endings).

Forecasts approximate swept arms with 17 spokes and conservatively estimate
bolt entry time. They are not a replacement for the actual hit queries. A
wide/late attack can remain unavoidable, and bodies or geometry can change
after observation. Special thrown-minion transit is not an ACultSkillAttack
and has no new awareness in this pass. Dynamic hazards, tower aggression,
broader tactical decisions and sustained battle performance remain open.

This change adds source and small evidence files only. There are no new models,
textures, downloaded assets or standalone packages. Keep the working Editor
build locally; back up reproducible inputs rather than generated outputs.
