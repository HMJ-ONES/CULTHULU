"""Summarize the verified combat, HUD and battlefield update without build output."""
import csv
import json
from pathlib import Path

project = Path(__file__).resolve().parents[1]
logs = project / 'Saved/Logs'
def passed(filename, name, path):
    text = (logs / filename).read_text(encoding='utf-8-sig')
    assert f'Test Completed. Result={{Success}} Name={{{name}}} Path={{{path}}}' in text, path
for mode in ('Domination', 'DOTGOO'):
    passed('CombatWorldPolishFinal.log', mode, 'CultUlhu.Playability.Matrix.' + mode)
passed('CombatWorldRosterVerified.log', 'FreeRoam', 'CultUlhu.Playability.Matrix.FreeRoam')
passed('CombatWorldMeleeFixed.log', 'SkillCombat', 'CultUlhu.Menu.SkillCombat')
for mode in ('Domination', 'DOTGOO', 'FreeRoam'):
    passed('CombatWorldPolishFocused.log', mode, 'CultUlhu.World.ExpandedBattlefields.' + mode)
passed('CombatWorldCavernRoofVerified.log', 'FreeRoam', 'CultUlhu.World.ExpandedBattlefields.FreeRoam')
for mode in ('Domination', 'Moba'):
    passed('CombatWorldPolishFocused.log', mode, 'CultUlhu.Menu.HeroPlates.' + mode)
network = []
for mode in range(3):
    for role in ('host', 'guest'):
        path = project / f'Saved/Automation/Room{mode}_{role}.json'
        assert path.stat().st_mtime > (logs / 'CombatWorldRosterVerified.log').stat().st_mtime
        result = json.loads(path.read_text(encoding='utf-8-sig'))
        assert result['ok'] and result['movement'] and result['combat'] and result['hostReplacedByBot']
        network.append(result)
    rows = list(csv.DictReader((project / f'Saved/Playability_Mode{mode}.csv').read_text(encoding='utf-8-sig').splitlines()))
    assert len(rows) == 16 and all(float(row['primary_damage_pct']) > 0 for row in rows)
    assert all(row['qfr_observed_effects'] == '3' for row in rows)

text = '''# Combat clarity and expanded battlefields — 2026-11-22

The canonical project and desktop shortcut use this update.

## Combat and movement

- Walking: +20%; sprinting: +15% relative to the previous build, in all modes.
- Projectile travel: approximately +26%, including tower fire, thrown rocks and minions.
- Base projectile damage: +18%; melee damage: +20%. Existing burst limits, healing rules, windups and cooldowns remain in force.
- Dash travel, dash stamina cost and regeneration delay retain their existing settings.
- Melee collision samples actual authored arm poses between slow frames. Sweeps remain attached to the arm and retain cover, guard, faction and evasion checks; one swing cannot repeatedly damage the same target. Sampled poses are restored immediately.

## Clearer HUD

- Removed the duplicate target name and white health strip near the crosshair.
- Overhead heroes have thin team-colored health bars, without names, identity cards or leader lines. Identity remains available in selection and the scoreboard.
- Nearby creatures use small world-space health bars. Aimed structures and Free Roam rival entities retain contextual health bars above the target.
- Health displays remain occluded by cover.

## Larger playable maps

| Mode | Previous ground size | Current ground size | New routes and detail |
|---|---|---|---|
| Domination | 640 × 600 m | 960 × 900 m | Two outer covered vaults, roof fighting decks, walking ramps, broken side courts, tall sight blockers and jumpable rubble. Flank bots alternate the established crypt and new outer route after deaths. |
| DOTGOO | 800 × 460 m | 1,200 × 690 m | Covered outer jungle routes, roof access, open side portals, ruins and cover courts. Existing lane curves, tower order and objectives are retained. |
| Free Roam | 3.6 × 3.6 km | 5.2 × 5.2 km | Eight four-entrance crossing caverns with sealed junction ceilings, cavern guide lighting, more instanced forests/rocks/crystals and additional distant city founding sites. |

The competitive maps have 2.25 times their previous ground area. Their minimaps, perimeter walls and rally bounds follow the enlarged footprint. Cover is mirrored for fair approaches. Low rubble can be jumped; tall fragments block projectiles and sight. Passage headroom was tested with the largest roster capsule.

To find the additions: in Domination, continue beyond the northern or southern keeps into the new outer vaults and courts. In DOTGOO, explore beyond the outer lanes into the covered jungle routes; ramps near their ends reach the roofs. In Free Roam, M shows the expanded region map and cave markers.

## Verification

- Canonical path validation and CultUlhuEditor Win64 Development compilation passed.
- All 16 characters passed movement/primary/RMB/Q/F/R checks in Domination and DOTGOO. The complete Free Roam roster passed again after the low-frame-rate melee repair. Measured CSVs are under `Saved/Playability_Mode{0,1,2}.csv`.
- Minimal health-bar rendering passed at compact, normal and wide viewport sizes, including occlusion and actual health values.
- Largest-capsule vault and cavern traversal, all creep-route segments, new ground support, roof surfaces and cavern junction ceilings passed collision checks.
- Live skill-combat checks passed for contact, cover, dodging and attack timing.
- Separate local host/client processes passed all three modes: movement, combat, local leaving, bot replacement, DOTGOO creep movement and ward countdown synchronization. These checks do not claim an internet/Radmin or Cloudflare test.

The update uses existing art and generated geometry. No new paid dependency or downloaded asset pack was added. Original assets and pre-update source copies are preserved; build/test output remains local under Saved.
'''
path = project / 'Docs/CombatWorldPolish20261122.md'
path.write_text(text, encoding='utf-8')
print('Verified report:', path)
