# Completed gameplay pass — 2026-10-18

Implemented after the Free Roam-only beliefs, mode AI, healing, and terrain collision pass. This file retains the requested scope as a completed checklist; see [MatchModes.md](MatchModes.md) for exact controls and balance values.

## Shift input

- A single tap of Shift triggers a directional dash.
- Holding Shift triggers sprinting and must not trigger an initial dash or repeated dashes.
- Preserve directional dash choices with W/S/A/D and normal sprint movement.

## MOBA towers and nexus protection

- Greatly increase tower health.
- Make tower projectiles much faster.
- A tower attacking a hero continues to attack that hero until allied minions of that hero enter the enemy tower's area; then switch tower aggression to those minions.
- The first (outer) tower of each lane must be destroyed before that lane's second (inner) tower can receive damage. Preserve and verify the existing outer-to-inner guard.
- In the Great Old One / nexus area, the middle tower and the Great Old One must receive no damage until all lane towers are destroyed. This supersedes the current rule that clearing any one lane exposes the god.

## Cthulhu MOBA RMB — explicit requested variant

- Levitate an enemy lane minion, then launch it as a physical projectile at an enemy hero or tower.
- If it hits an enemy hero or tower: apply ordinary, restrained damage and kill the launched minion.
- If it misses: land the minion within a reasonable distance, toward the beginning of its original lane or its actual nearby landing position; leave it at half health.
- A surviving thrown minion resumes normal marching, reconnecting to the midpoint of its original lane / original destination rather than remaining stuck or becoming a permanent projectile.
- Cooldown: twenty seconds.
- This is the user's explicit MOBA variant; preserve Free Roam and other character kits.

## Implementation and verification

- Shift tap is resolved on release under 0.18 seconds; longer holds sprint with no dash on press or release.
- Lane towers: 3000 HP. Nexus ward: 4000 HP. Tower shots: 15000 cm/s. Great Old One: existing 1500 HP.
- Protection chain: each outer before its inner; all six lane towers before nexus; nexus before god.
- Cthulhu MOBA: hold RMB to lift, release RMB or click LMB to throw; 20-second cooldown; 20 hero / 40 structure base damage plus existing competitive scaling. Misses halve remaining HP and reconnect to the original lane.
- Development Editor build passed. `TowerSlingChecks.log` passed DominationMatch, MobaMatch and MobaMechanics. `TowerSlingFreeRoamFinal.log` passed Beliefs, CombatFeel, GroundSpawn and SkillCombat.
- Rendered MobaMechanics and original Free Roam SkillPreview passed; the final HUD layout also passed a rendered MobaMechanics rerun. See `TowerSlingRendered.log` and `TowerSlingRenderedFinal.log`.
