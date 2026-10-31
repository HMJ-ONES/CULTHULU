# Domination: route identity and seal overlooks

## Gameplay change

Two mirrored balconies connect the existing side overpasses to forward seal overlooks. Each is 60m wide and 40m deep, with its floor at the existing 60m upper-route elevation. Physical side cover supports ranged pressure; the forward edge remains open for aiming and dropping toward the fight. The solid slab also provides sheltered ground space beneath it. Neither overlook scores capture points. The original scoring rectangle, keeps, outer ramps, underground junctions and four crypt branches remain in place.

The forward firing lane was tested with a full-height Cthulhu walking from each bridge to its overlook edge, then a real non-homing attack actor contacting an enemy on the seal. This proves physical support and projectile clearance for those fixtures. It does not certify every character/ability pairing or establish human balance and enjoyment.

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
```

`DominationArchitecture` checks actual walks, physical projectile contacts, solid cover below the balconies, non-scoring height, collision-free detail, perimeter placement and appearance-only geometry preservation. It captures Crown, Hollow, crypt-entry and overlook views with the decoration hidden/shown. Both views retain the new gameplay balconies: this comparison isolates the architectural skin and landmarks, not the entire old/new map layout.

`DominationMatch` performs the existing keep-ramp, full catacomb and four-branch traversal, then observes autonomous combat, capture and underground flanking by both teams and verifies match completion. That test does not prove that bots independently choose the new overlooks.

The paired standalone check remains:

```text
python "unreal 5.8/Tools/RunRoomNetworkProbe.py" --mode 1 --lighting
```

The Domination diagnostic camera is raised above the new balconies, and its gray references are elevated above ground-level combat shadows. The analysis projects reference regions from the recorded lens/camera geometry, and the runner rejects obstructed reference rays. Matching roof pixels cannot silently stand in for a lighting comparison. The original minimum-brightness and maximum-difference gates are unchanged.

Local renders and logs belong under `Saved/`. The compact evidence and source hashes are recorded in `DominationArchitectureVerification_2026-10-31.json`.

## Critical limits and next work

The map remains visually blocky, with repetitive masonry and a sparse exterior. These silhouettes and route cues improve orientation but do not reach the requested level of finished art. Artist-directed weathering, architectural refinement, camera composition and contrast across more viewpoints remain open. The new overlooks also need deliberate AI approach/pressure/retreat behavior, human kit matchup testing and a full-match enjoyment review. Existing bots can fight and complete ground/underground routes; that must not be presented as independent use of every new upper position.
