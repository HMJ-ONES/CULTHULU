# CULT-ULHU development priorities

Owner direction, October 16, 2026: **playable, fun, beautiful**, in that order.
The owner leads entity art and modeling; code, integration, tooling, and
repeatable validation should make that work easy to bring into the game.

## Initial inspection

The handoff was read in full. It ends at the heading for previous compilation;
it contains no build output after that heading.

- The canonical project is `unreal 5.8/CultUlhu.uproject`. Preserve the other
  project directories and existing assets during reconciliation.
- Local branch: `game-core`, HEAD `972ccf8`. The cached remote branch has two
  additional commits, including migration `842820e`. This is a cached comparison,
  not a live GitHub verification.
- The entire canonical project is currently untracked. The tracked older
  `unreal/CultUlhu.uproject` is modified. Do not pull the migration over these
  files or stage all untracked directories indiscriminately.
- The canonical folder has Source, Content, Config, and build artifacts, but
  lacks Tools. World-building scripts exist in the older project. Reconcile
  against the migration commit file by file, preserving local compile fixes.
- Menu startup settings point to `L_MainMenu`; preserve them.
- The local pawn scales its whole actor in BeginPlay. Ground placement must
  use the resulting scaled capsule, not a human-sized PlayerStart offset.
- The inspected older terrain builder does not explicitly configure terrain
  collision. Missing collision remains a hypothesis until the actual asset
  and runtime traces are inspected.
- There is already a deterministic terrain generator at
  `assets/world/generate_dark_wilds.py`, currently using a fixed seed, and
  procedural dungeon code in `src/world`. Build on these systems.

No gameplay changes, Unreal compilation, editor playtest, or GitHub sync were
performed during this onboarding inspection.

## Delivery order and acceptance

1. **Preserve and reconcile.** Make a selective checkpoint of local source,
   configuration, and tools; preserve local Content separately before asset
   edits. Inspect live remote HEAD and reconcile the migration without resetting
   local work. Exclude build caches, binaries, Saved, and Intermediate.
2. **Stand and walk.** Make terrain collision deterministic in the canonical
   builder. Resolve a walkable surface using collision, account for the selected
   entity's final capsule size, and check full capsule clearance. Reject missing
   ground and blocked starts with actionable diagnostics. Verify menu-to-world
   travel, movement, jumping, slopes, and small/large roster entities in Unreal.
   Rebuilding the world must preserve collision. Never hide failure with a
   guessed Z offset or invisible substitute floor.
3. **Prove one enjoyable loop.** Use one polished region to test exploration,
   encounters, smash feedback, cult growth, and a clear objective with a reward.
   Confirm which core systems actually reach the Unreal player before promising
   them as playable features. Check readability and pacing with playtests.
4. **Add repeatable world variation.** Implement the seed and layout contract
   below, initially through the existing generation pipeline. Runtime terrain
   generation is a separate implementation decision, not an existing feature.
5. **Integrate the owner's art.** Establish one successful entity import from
   model through scale, materials, animation, camera, collision, and abilities.
   Keep placeholders usable while art develops. Apply lighting, atmosphere,
   landmarks, and performance budgets to a proven playable region first.

## Procedural world contract (proposed)

- Store a world seed and generator version so a layout can be reproduced.
  Use separate random streams for terrain, landmarks, encounters, and decoration.
- Plan terrain and traversable connections before dressing the world. Support
  mountains, cliffs, pits, vales, cave entrances, ruins, cities, and temple
  grounds through placement rules rather than independent random coordinates.
  Overhangs and cave interiors need geometry beyond a single height field.
- Reserve one viable temple territory for each participating entity before
  placing optional landmarks. Define minimum separation in world units using
  territory size and expected travel time. Reject layouts that cannot satisfy
  the required separation; do not silently place rivals next to each other.
- Validate walkable ground, slope, water/hazard exclusions, full creature
  clearance, and a traversable route from each temple to shared objectives.
  Use the largest supported creature footprint where shared access is required.
- Balance access to early resources and objectives without requiring identical
  terrain. Make landmarks readable enough to support navigation and discovery.
- Bound generation attempts and report unsatisfied constraints. Keep a known
  valid seed/layout available, explicitly reporting any fallback.
- Test determinism, separation, connectivity, and generation failure over many
  seeds. Then verify actual Unreal collision, movement, and frame time; abstract
  layout tests cannot establish that a generated map is playable.

## Collaboration expectations

Use the existing character pipeline as a starting point, then simplify it based
on an actual imported entity. Keep technical steps in tooling and development
documentation. Report what was built and played separately from design intent.
GitHub authority does not imply permission to discard local work or publish
generated caches. Do not create recurring background work without a request.
