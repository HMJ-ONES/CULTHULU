# Solid MOBA objectives and usable routes

An architecture review exposed a gameplay defect before an art upgrade: lane
wards, nexus wards and Great Old One objectives blocked attack traces but
ignored character movement. The original movement fixture reproduced walking
and root-motion dashing through all three bodies. Minions passed straight
through their centres with zero lateral detour. There were nine failed
assertions across the three original body checks.

These stationary objectives now use the solid-world collision object type and
block the pawn channel. Player movement and native minion steering therefore
query their existing gameplay capsules. Capsule dimensions, visual meshes,
health, damage, firing speed, range, protection order and cooldowns are retained.
This is a physical-presence correction, not a new architecture asset or a claim
that decorative geometry has precise per-triangle collision.

## Routing regression found and corrected

The first integrated run passed all three body checks and MOBA mechanics, but
failed the full match. Middle waves reached only 0.058 route progress on both
teams. Making the nexus solid exposed an existing route knot inside its body:
the movement code kept asking units to reach that inaccessible location.

A focused real-director reproduction confirmed that melee, ranged and siege
minions on both teams stopped at waypoint 2. Their X coordinates stayed near
±26,504 cm (±26,580 for siege) while the target was ±26,250 cm inside the allied
nexus. No attack or opponent was involved in that isolated fixture.

Native minions now select a later route knot when their current knot intersects
an allied objective's capsule, with a one-metre clearance margin. The test uses
both capsules' vertical spines and radii. Selecting a target does not relocate
the actor: existing swept movement still walks around the solid objective.
Enemy objective selection and protection rules remain unchanged.

The focused rerun observed all six probes beyond their nexus at roughly
±19,091 cm (±19,172 for siege), advancing to waypoint 6. The normal match also
observed all nine bot lane entries and waves advancing on all six team/lane
routes. ObjectiveCollisionResults.json retains the failed intermediate run
instead of treating the small body fixture as proof of integration.

## Lane and body verification

The old terrain check demanded an unobstructed lane centre at every sample,
including the intentional tower bodies. It still samples all 387 positions
with the full-size capsule. Where the first blocker is a solid objective, it
now requires a supported, nonpenetrating side passage using that capsule.
Cliff/terrain failures do not get this fallback, and objectives are never
ignored in the queries. Both the standalone and integrated checks found real
side passages at 68 objective-occupied samples. These queries do not alone
prove continuous human navigation, so actual movement checks are separate.

The body fixtures enter the real MOBA map, stop competing AI activity and spawn
each real objective class in an isolated central lane position. Raw character
input tests walking, and TryDash tests the actual root-motion dash. A native
minion uses the same MoveTo implementation as the director. An additional
full-size hero detour uses actual movement, rather than teleporting between
query samples. The player's retained capsule height is 2,933.3 cm. A final
control removes the objective and verifies the route is walkable again.
That removal uses actor destruction; normal damage/protection transitions are
covered separately by MobaMechanics.

The seven-test integration batch passed with zero automation errors. After
expanding the physical fixture to include actual hero detours, the test-only
build and all three expanded body checks also passed. Gameplay source did not
change between those runs. Walking stopped near X=-1,016.9 cm and dashing near
X=-1,017.7 cm. Minions detoured roughly 5.2 metres, and the full-size hero used
11.6–11.8-metre lateral detours before reaching the far side. Removal restored
free walking. The final test build took 9.62 seconds; the gameplay/routing build
took 19.99 seconds.

The isolated route fixture uses real director ticks, six living minion probes
and inactive combatants. It covers every minion class on both teams against
the original allied nexus. The full match separately checks real wave movement,
physical combat, progression and existing tower/minion-throw rules. These are
bounded Editor observations, not a human playtest, multiplayer verification or
a full-length balance study.

## Remaining work

The gameplay bodies are still capsules, and the visible towers remain simple
blockout models with decorative parts outside those bodies. Clearer tower/nexus
silhouettes, better temple architecture and visual damage states remain needed.
Collision gives these objectives physical presence; it does not make their art
professional. Long-session congestion, all-kit movement, battle pacing and
human fun remain unmeasured. The full production goal remains open.

This change adds source and compact evidence only. It creates no new art asset,
download, standalone package or duplicate project.
