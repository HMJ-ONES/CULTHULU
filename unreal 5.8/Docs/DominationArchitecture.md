# Domination: route identity and seal overlooks

## Gameplay change

Two mirrored balconies connect the existing side overpasses to forward seal overlooks. Each is 60m wide and 40m deep, with its floor at the existing 60m upper-route elevation. These are crossing and drop positions: their height puts ground-level targets beyond most original 16–25m ranged kits. The forward edge remains open for dropping toward the fight. The solid slab provides sheltered ground space beneath it. Neither overlook scores capture points. The original scoring rectangle, keeps, outer ramps, underground junctions and four crypt branches remain in place.

The original clearance test used a full-height Cthulhu walking from each bridge to its overlook edge, then a synthetic non-homing attack actor with 120m reach contacting an enemy on the seal. This proves physical support and projectile clearance for those fixtures; it does not prove the original kits can use that firing position. Treating that test as evidence of useful authored-kit ranged pressure would be a design error. No authored weapon range has been increased to fit the architecture.

Four lower side perches have 9m floor height and broad ground ramps. Their centres are at X=±33m, Y=±50m, leaving the central covered corridor open. They are intended to bring the original ranged kits into useful range of targets near the seal edge. Autonomous approach, actual kit contact and departure require the separate `CultUlhu.Menu.DominationUpperCombat` runtime observation; a clearance trace alone is insufficient evidence.

## Bot route policy

An eligible bot considers a lower perch only while another living teammate occupies the scoring rectangle. One bot per team can own this role. Eligibility uses the original Q/F projectile and acid-spit ranges, plus capsule contact and solid-cover checks from the proposed position. Melee kits, dedicated underground flankers, retreating bots and rally orders keep their existing jobs.

The direct home-side ramp is preferred. The long high crossing is considered only when the home-side perches cannot reach a target, an opposite-side perch can, at least two allies occupy the seal, the team leads by 25 points, and the candidate is near its own base. Its approach goes around the keep before reaching the outer ramp; it crosses above the fight and drops to the opposite lower perch. The high deck is not treated as an unlimited-range weapon platform.

Pressure ends after eight seconds, three seconds without a reachable target, losing the last allied capturer, a rally order or retreat. Bots leave through a physical ramp or drop and return to the normal capture/retreat brain. Enemy-side perches are exited toward the seal, rather than farther into enemy spawn. Upper-position recovery is restricted to grounded actors at the actual high-walk elevation so it cannot suppress normal tactics on unrelated elevated platforms.

## Original environment and navigation work

- The north **Crown** and south **Hollow** have different skyline silhouettes and matching neutral symbols. They provide orientation independently of team/faction color.
- The skyline structures sit on the existing perimeter and use solid collision. They do not obstruct the ground approaches or underground paths.
- Four keep facades receive shallow masonry ribs, roof-edge bands and inset symbols. Those decorative skins add no movement or attack collision and do not seal the existing passages.
- Small inlaid chevrons identify upper-ramp climbs and crypt descents. They follow the actual ramp slopes and have no collision.
- The minimap carries Crown/Hollow orientation symbols and a bounded aim-direction arrow. Local route hints replace the existing hint strip near ramps, crypt entries, high walks and overlooks; they do not create another permanent HUD panel.

The additional architecture is original procedural geometry using the project's existing stone and signal materials. It is merged into seven components, with roughly three thousand triangles. No model or texture downloads are required. This is a geometry budget, not a low-spec frame-rate certification.

## Verify

After the canonical validator and `CultUlhuEditor` build:

```text
Automation RunTests CultUlhu.Menu.DominationArchitecture
Automation RunTests CultUlhu.Menu.DominationMatch
Automation RunTests CultUlhu.Menu.MatchHUDCoordinates
Automation RunTests CultUlhu.Menu.DominationUpperCombat
Automation RunTests CultUlhu.Menu.CombatTactics.Domination
```

`DominationArchitecture` checks actual walks, physical projectile clearance, solid cover below the balconies, all four full-height low-perch ramp climbs, non-scoring height, collision-free detail, perimeter placement and appearance-only geometry preservation. It captures Crown, Hollow, crypt-entry, overlook and two lower-perch views with the decoration hidden/shown. Both views retain the gameplay platforms: this comparison isolates the architectural skin and landmarks, not the entire old/new map layout.

`DominationMatch` performs the existing keep-ramp, full catacomb and four-branch traversal, then observes autonomous combat, capture and underground flanking by both teams and verifies match completion. That test alone does not prove that bots independently choose the new upper positions.

`DominationUpperCombat` starts original-kit Mi-Go bots at their own bases, with unrelated fighters made inert. It observes both teams' independent lower-perch approaches, contact damage from an actual authored 20m projectile while the shooter is on the perch, departure without a target, low-health retreat and an opposite-side high crossing/drop. Targets stand outside Mi-Go's physical arm reach. Competitive heroes have 100 HP, so fixtures are healed between frames until the projectile contact is observed; the test wraps and preserves the original hit callback and requires that callback to actually reduce the victim's health. The bot receives no supplied path, movement commands or relocations after setup. `CombatTactics.Domination` retains the existing ranged spacing, teammate support and supported-ground escape-dash checks. It also tests an even-slot Hydra with Q/F/R unavailable: its original acid spit must launch and cause physical contact damage from the perch. The perch role can therefore use the acid weapon that qualified it for the position, regardless of the older alternating-slot attack gate.

The paired standalone check remains:

```text
python "unreal 5.8/Tools/RunRoomNetworkProbe.py" --mode 1 --lighting
```

The Domination diagnostic camera is raised above the new balconies, and its gray references are elevated above ground-level combat shadows. The analysis projects reference regions from the recorded lens/camera geometry, and the runner rejects obstructed reference rays. Matching roof pixels cannot silently stand in for a lighting comparison. The original minimum-brightness and maximum-difference gates are unchanged.

Local renders and logs belong under `Saved/`. The compact evidence and source hashes are recorded in `DominationArchitectureVerification_2026-10-31.json`.

## Critical limits and next work

The map remains visually blocky, with repetitive masonry and a sparse exterior; the new lower-perch renders confirm this. These silhouettes and route cues do not reach the requested level of finished art. Artist-directed weathering, architectural refinement, camera composition and contrast across more viewpoints remain open. Controlled AI scenarios establish execution, not full-roster balance or the value of a long flank in human play. Human kit matchup testing, route timing under a contested fight and a full-match enjoyment review remain necessary. A 60m upper deck must never again be described as useful authored-kit ground pressure on the strength of a synthetic 120m clearance projectile.
