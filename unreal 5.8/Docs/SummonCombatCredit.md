# Summoned combat attribution

Native baseline tests reproduced an inconsistent reward path. Jungle camps
already resolved a summoned creature to its owner, but hero and lane/structure
rewards searched the hero roster for the creature itself. Summoned hero damage
recorded zero for the owner; a previous direct attacker could receive the kill
instead. MOBA minion last-hits paid only 30 nearby XP instead of the existing
60 last-hit reward, and tower last-hits paid 200 shared XP instead of 800.
Summoned contact also omitted local hit confirmation. Direct friendly summoned
damage bypassed the hero damage gate, and a mismatched summon could still heal
its former owner through Thousand Young.

CultCombatCredit now resolves a direct hero or an explicitly summoned
ACultPopulation with a valid, same-world, same-faction hero owner. It does not
walk arbitrary actor ownership chains or attribute ordinary civilians/minions
to a hero. It is shared by hero damage/deaths, lane and structure rewards,
hit feedback, the existing summon passive, and jungle camp attribution.

The original creature remains the physical source: collision, faction checks
and incoming attack direction do not move to its distant owner. Hit feedback
and damage statistics use actual health lost. Friendly/protected/prevented hits
must not generate damage rewards or healing; overkill cannot inflate healing.
Surviving summons can credit defeated owners, but healing cannot resurrect them.
Bot-owned summons receive the same credit as the human player's summons.

Original ability definitions, damage values, cooldowns and reward amounts are
unchanged. Shub-Niggurath's source definition still specifies ten-percent
healing from summoned damage. The fixtures deliberately create owned creature
actors to isolate attribution; they do not grant other kits a summon ability.

SummonCombatCreditResults.json preserves baseline failures and final native
results. The isolated credit fixtures call the real damage/death/reward path
with prepared shields and a paused world; they are not aiming or movement
tests. Existing match, mechanics and jungle fixtures provide broader regression
coverage, including an actual projectile from a bot-owned summon in the jungle.
These changes have Editor evidence only, without a new independent executable.

The final canonical build and all six Editor checks passed with zero automation
errors: both summon-credit modes, DominationMatch, MobaMatch, MobaMechanics and
JungleCamps. The final fixture also adds ordinary-owned-civilian and posthumous
credit cases beyond the initial baseline. Its 195 recorded hero damage includes
95 in the initial encounter and a further 100 in the posthumous encounter; it
is not a directly comparable damage total with the shorter baseline fixture.

This corrects reward reliability and feedback, not overall combat quality.
Full summon AI/pathing, all released kits, multiplayer attribution and human
balance/fun evaluation still need work. No downloads or new package are added.
