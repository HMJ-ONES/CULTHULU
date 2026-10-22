# NPC body clearance and traversable slopes

Routine civilian/cultist steering previously swept a 45 cm sphere at the actor
center. That sphere could pass under an obstruction too low for the actual head.
When every trial direction was blocked, the original target direction survived
and was still sent to native character movement. Walkable incline contacts could
also be treated as obstacles, causing a person to route around a ramp and stop
beneath the intended destination.

Steering now probes the actual scaled capsule, inset slightly to avoid resting
floor contact. A nonpenetrating walkable contact near foot/step height is allowed
so native character movement can traverse an incline. Walls, low ceilings and
initial penetration are rejected. If no trial direction is usable, cached intent
stays zero. Actual movement, ground lookahead, accepted-order interruption,
control-state response and physical attack delivery remain on their native paths.

## Physical evidence

The new PopulationClearance fixture uses real character physics and collision;
it does not teleport or manually tick its subjects. Before the fix, all four
assertions fail: the low-beam actor stops at X=600115.88, the surrounded actor
pushes to X=604055.88 with nonzero input, and the ramp actor reaches X=608892.85
but remains on the lower floor (feet Z=45011.9).

After the canonical Editor rebuild, ArmyCommands, PopulationClearance and
PopulationMotion all pass. The beam actor reaches X=600870.65 around the obstacle;
the surrounded actor stays at X=604000 with zero input. The ramp actor reaches
X=608872.66 with feet Z=45304.1, about 294 cm above its starting floor, and remains
grounded. Existing tests retain immediate order reversal, held stun/root,
recovery, a real delayed battle hit and braking at the actual platform edge
(final X=2805.2 for a floor ending at X=3000).

PopulationClearanceResults.json preserves before/after log hashes, failures and
observations. This verifies those cases; it does not establish complete path
planning, recovery from every initial overlap, all stair/doorway combinations or
absence of stuck units in procedural cities. City population is currently created
during the building loop, before the last building placements; that ordering is
an additional overlap risk to investigate. This later steering change is outside
the preceding independent executable's shared-mesh performance verification.
