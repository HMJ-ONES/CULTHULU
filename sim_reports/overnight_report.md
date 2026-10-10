# CULT-ULHU overnight — balance sim re-run (waves 12–13)

Method: same harness and scenario matrix as waves 9–10 (`src/sim/`,
18 scenarios × 8 seeds × 3600 ticks), re-run after waves 12 (NPCs/creatures)
and 13 (Vale of Pnath). Raw CSVs in `sim_reports/overnight_run/`.

## Result: no balance change — no tuning applied

All 18 scenarios match the wave-10 post-tuning baseline **exactly**
(0 diffs in median time-to-500 and mean final power). This is expected:
waves 12–13 added content (NPC packages, creature species, the Vale dungeon
with its own exertion feeds) but did not touch belief income, exertion
decay/gates, or the synergy matrix, so power curves are unchanged.

Per the conservative-tuning rule (wave-10 pattern: small ±20% adjustments
only when the sim justifies them), **no tuning changes were made**. The
wave-10 balance stands.

## Watch items (carried forward, not actioned overnight)

- Fear×Torture remains 1.99× median (terror synergy firing as intended;
  inside the 2× flag).
- Fear / Chaos / Onslaught / Reconstruction still never reach 500 power —
  accepted support tier per wave-10 analysis.
- The Vale's exertion feeds (AbyssPitFall→Fear 6, MaddeningWhispers→Fear 4,
  DholeAmbush→Fear 8, ValeRelicClaimed→Magic 8) are new since wave 10 but
  only fire on player-driven dungeon traversal, which the headless sim does
  not exercise. Recommend a dungeon-aware sim scenario in a future wave if
  Vale farming becomes a balance question.
