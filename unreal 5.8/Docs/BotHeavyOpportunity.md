# Useful heavy attacks and solid melee cover

Bots used a shared 27-metre trigger for alternate attacks even when their
source-defined melee heavy had a much shorter physical reach. Native baseline
checks in both competitive modes reproduce an unreachable committed sweep. The
revised decision checks the actual heavy origin, reach, radius, direction and
arc before committing a bot melee heavy. A nearby enemy in that arc can make
the attack useful even when another enemy is the selected aim target. An ally
alone or an enemy outside the committed arc does not.

The preflight first looks for a plausible nearby enemy body. If one exists, it
samples the radial swing and moving-tip chords at the physical attack's
.012-second angular resolution, using the same visibility channel, sphere
radius, friendly-body exclusions and blocking-hit behavior. Each candidate
contact must also have clear cover from the arm origin. No body or warning
actor is spawned, and no heavy cooldown is spent, when the check fails.
Successful heavies retain the original six-second cooldown. Human input can
still commit a deliberate miss. This observes current bodies and cover, not
future movement; the original windup remains a chance to evade.

The investigation also found a physical cover bypass. A tip chord could sweep
on the far side of a wall even while the radial part of the swing was blocked.
An independent outer-tip probe reproduced 33.6 damage through an unbroken wall
in both modes. Physical melee sweep contacts now require a clear connection
from the actual arm origin to the contact point. This applies to the shared
melee sweep path, including primary and heavy attacks. Bolts and beams retain
their existing swept cover behavior. This is a collision correction, not an
increase in reach or damage and not a change to source kits.

## Evidence and limits

BotHeavyOpportunityResults.json records exact log hashes, failed iterations,
baseline defects, corrected fixtures, source-preservation checks and final
native outcomes. The final fixture checks an unreachable BotThink heavy, an
immediate reachable heavy without resetting its cooldown, real nearby damage,
successful-cast cooldown enforcement, blocked versus uncovered attacks, a
different enemy in the actual arc, an outside-arc enemy, allies and deliberate
human misses. A separate physical launch bypasses bot choice to test the
outer-tip cover defect itself. Removing the wall must allow real damage to
the same target, rather than merely produce a warning actor.

Initial fixture errors are not counted as production defects. Respawning the
bot reset its heavy cooldown; final placement relocates the existing bot.
The first giant target protruded toward/through the cover plane; the final
cover fixture uses a small body entirely behind the wall. An early alternate
body stood below the committed arc; the final control places it in the arc.
An independent inner-arc cover check passed but did not cover tip chords. The
outer-tip reproduction failed with health 66.4 instead of 100 before the cover
guard. Earlier mixed batches and that inadequate inner-only pass remain in
the report.

Canonical validation and CultUlhuEditor Win64 Development compile on UE 5.8.3
in 29.42 seconds. The final batch covers both heavy-opportunity modes, prior
area-opportunity and primary-reach checks, defensive reactions, physical skill
combat, combat feel and both mode integration fixtures. The report records
all twelve terminal successes with no automation errors. Integration includes physical walking/live combat
windows and scripted transitions, not uninterrupted human matches.

A source identity check removes only the added bot helper/gate and the new
physical melee cover guard, then verifies the rest against 3986369 after
whitespace normalization. Shape math, spell parameters, damage, windup and
duration are retained. All 425 backed-up Content files remain byte-identical;
the sixteen original released definitions remain identical after line-ending
normalization. No assets, downloads, archives or standalone package are added.

The extra opportunity-query cost is unbenchmarked. Sampling is a current-pose
check and is not a forecast of the full moving-owner attack. Full per-kit
tactics, ground-impact/summon choices, team roles, human match pacing, sustained
low-spec performance, multiplayer and authored art/animation remain unfinished.
BotThink still uses shared Q/F/R distance thresholds and only odd roster slots
attempt RMB; this pass does not establish complete use of every character kit.
The full production-quality and fun objective stays open.
