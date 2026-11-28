# Settlement population and occupied footprints

City generation previously spawned residents inside the same loop that placed
houses. A person could pass the initial capsule check, then intersect a house
placed later. The native city check reproduced two embedded civilians in the
first initialized snapshot for RNG input 29. Other sampled inputs were clear;
that did not invalidate the failing case.

Cities now finish placing their geometry before spawning residents. Each intended
resident receives up to 18 bounded attempts using real terrain and capsule
clearance. Spawn points are distributed through the settlement's 60 m ellipse
rather than concentrating everyone in the central seven metres. The original
resident target remains approximately one person per two attempted buildings.
No residents are created for a settlement with no successfully placed buildings.

New building candidates also check a conservative oriented volume around the
actual imported mesh bounds. The shared transform accounts for source-mesh
pivot, nonuniform dimensions and yaw. Live pawn bodies and non-pawn feral
creatures reject the candidate; placement searches another position. No existing
character is teleported, damaged or deleted by this rejection. A final guard runs
before materializing the building. The checks apply to generated settlement
placement; cult construction completion retains its existing separate workflow.

## Verification

The canonical Editor build succeeds. CityPopulationClearance observes actual
initialized Free Roam worlds with RNG inputs 11, 29, 73 and 151. The before run
records 0 / 2 / 0 / 0 civilians intersecting LivingCity collision. After the change,
all four snapshots record zero intersections, while retaining substantial cities:

| RNG input | Buildings | People | Embedded people |
| --- | ---: | ---: | ---: |
| 11 | 146 | 180 | 0 |
| 29 | 169 | 192 | 0 |
| 73 | 164 | 189 | 0 |
| 151 | 174 | 193 | 0 |

These are first initialized snapshots (world times about 0.04–0.40 seconds), not
long-session navigation tests. Changing the random draws changes layouts; the
before and after worlds are not geometrically identical controlled pairs.

SettlementFootprints additionally uses an actual imported wall stretched and
rotated 90 degrees. It refuses to appear over a real civilian, allows a nearby
clear placement, preserves the civilian's position and verifies its capsule
remains unblocked. A live ghoul also prevents placement. The final strengthened
check verifies the spawned mesh's actual ground-aligned base and requested
rotated dimensions. ArmyCommands and PopulationClearance also pass after the
placement refactor, retaining the preceding command and movement checks.

SettlementPopulationResults.json preserves results, counts and before/final log
hashes. The helper uses conservative bounds; it can reject a geometrically empty
archway if a character occupies that volume. Four samples do not prove every
procedural arrangement. Full path planning, all initial-overlap recovery,
construction completion around non-worker bodies, long-session population cost,
and visual/environment refinement remain further work. These later edits have
Editor evidence; they are outside the previous independent executable checkpoint.
No content asset, download or new standalone package is needed for this change.
