# CULT-ULHU wave 10 — balance sim re-run (R1–R5 applied)

Method: same harness and scenario matrix as wave 9 (`src/sim/`, 18
scenarios × 8 seeds × 3600 ticks, 1 game-second per tick), re-run after
applying all five wave-9 recommendations. Wave-9 CSVs archived under
`sim_reports/wave9_baseline/`. Median time-to-500 across scenarios that
reach it: **2751 ticks** (wave 9: 2331 — the median slowed because R1
diluted every multi-belief loadout).

## 1. Power balance (post-wave-10)

| Scenario | med t500 | vs median | final power | notes |
|---|---|---|---|---|
| Fear×Torture | 1381 | **1.99× faster** | 1000 | Terror synergy now fires (intended); inside the 2× flag — watch item |
| Torture | 1691 | 0.61× | 935 | unchanged |
| Conversion×Trickery×Magic | 2171 | 0.79× | 754 | **was the 2.28× outlier — fixed by R1** |
| Sacrifice×Magic×Dreams | 2251 | 0.82× | 726 | Dark Rites / Martyrs' Visions / Oneiromancy now fire |
| Conversion | 2601 | 0.95× | 646 | unchanged |
| War×Onslaught | 2701 | 0.98× | 636 | Blood Frenzy now fires |
| Trickery | 2751 | 1.00× | 622 | unchanged |
| Sacrifice | 2781 | 1.01× | 565 | unchanged |
| Dreams×Chaos | 2901 | 1.05× | 592 | Nightmare Surge now fires |
| War | 3151 | 1.15× | 555 | unchanged |
| Magic | 3411 | 1.24× | 520 | unchanged |
| Breeding | 3451 | 1.25× | 510 | unchanged |
| Dreams | 3501 | 1.27× | 502 | **was 238, never reached 500 — fixed by R5** |
| Fear / Chaos / Onslaught / Reconstruction / Chaos×Recon×War | never | — | 445 / 444 / 449 / 256 / 488 | Reconstruction 190→256 (R5 power path); Fear/Onslaught unchanged, acceptable support tier |

## 2. What each recommendation did

**R1 — diminishing returns (income ×1.0 / ×0.75 / ×0.5 for 1/2/3 active
beliefs).** The triple-stack outlier is gone: Conversion×Trickery×Magic
1021→2171 ticks (0.79× median). Every multi-belief loadout slowed in
absolute terms, which is why the median itself moved 2331→2751.

**R2 — exertion reaches the gates** (decay 0.5→0.05/s, synergy gate
50→25, tension gate 70→35, plus feed bumps for the three beliefs whose
schedules could never beat the decay: SacrificeCompleted +12→+20,
BuildingRebuilt +2→+8, HealPerformed +1→+2, DreamWhisper +1→+8).
Mean final exertion for fed beliefs is now 70–99 (wave 9: 10–31.5 for the
cold ones). Synergies confirmed live in-sim: Terror, Nightmare Surge,
Blood Frenzy, Dark Rites, Infiltration, Martyrs' Visions, Oneiromancy.
Conflict suppression works as designed: in Chaos×Reconstruction×War the
Reconstruction×War conflict pair pins both at ~24–25 exertion, below the
tension gate.

**Degenerate-loop check (required by R2).** Terror at +25% on every
torture/raid once Fear×Torture goes hot: the loadout is *absolutely*
slower than wave 9 (1301→1381 ticks) — R1's ×0.75 dilution outweighs the
synergy. Relatively it rose 1.79×→1.99× only because the median slowed
more. It sits just inside the 2× flag. Verdict: not degenerate, but it is
now the fastest loadout by design (synergy reward). If the design target
is ≤1.5× for all loadouts, the 2× flag itself needs recalibrating, not
the constants.

**R3 — organic insurrection fuel** (+0.2/s risk while any cultist's
devotion < 20). Chaos scenarios: 2 revolts/run (punishment-only, wave 9)
→ 5 (Chaos), 3 (Dreams×Chaos), 5 (Chaos×Reconstruction×War). Insurrection
now fires organically through sustained Chaos play. Deliberately set at
+0.2/s rather than the +0.5/s sketched in wave 9: at +0.5/s the same sim
produced 11–12 revolts/hour (chained alarm); +0.2/s keeps it a slow burn
(~1 revolt per 12 min of maximal neglect).

**R4 — Onslaught idle decay** (limit 3600s→1200s). Not measurable in this
sim — the bot kills every 50 ticks and never idles. Verified by unit test
(`testOnslaughtIdleLimit`); real-play behavior: 20 idle minutes now bite.

**R5 — power paths.** Reconstruction: BuildingRebuilt +2, HealPerformed
+0.5 → 190→256 final (66/run from its own actions, up from 0). Dreams:
whisper 2.0→13.0 → 238→502, reaches 500 at 3501 ticks. Reconstruction
remains a support-tier belief (same band as Fear/Onslaught, which the wave-9
report already accepted as "can't reach 500, acceptable for their
fantasies") — it now has a real income instead of zero.

## 3. Coverage gaps (unchanged or new)

- **Tension events** still never fire in the sim matrix: no scenario pairs
  a hot conflict (the one conflict pair present, Reconstruction×War, is
  suppression-pinned below the 35 gate — working as designed). Covered by
  unit test `testTensionEvents` (fires at 80/80, cooldown holds).
- **BeliefSystem::tick() deltas** (Fear decay, Onslaught idle decay) are
  still discarded by `SimWorld::tick` (the real driver applies them via
  `power.add`). Pre-existing harness gap, unchanged.
- Same bot caveats as wave 9: fixed ideal-rate schedules, dt = 1.0s,
  melee/Wave-of-Domination abstracted to event rates.

## 4. Files changed (wave 10)

- `src/exertion/ExertionSystem.h` — DECAY_PER_SEC 0.05, SYNERGY_THRESHOLD
  25, TENSION_THRESHOLD 35
- `src/exertion/ExertionSystem.cpp` — `stackedIncomeMultiplier()` (R1)
- `src/exertion/ActionExertionTable.h` — sparse feed bumps (R2)
- `src/beliefs/InteractionMatrix.cpp` — threshold parity 25
- `src/beliefs/BeliefSystem.h` — ONSLAUGHT_IDLE_LIMIT 1200
- `src/beliefs/BeliefSystem.cpp` — Reconstruction power block, whisper 13.0
- `src/cult/CultManager.cpp` — disaffected-flock risk trickle (R3)
- `tests/tests_wave4.cpp` — new R1/R3/R4/R5 tests; updated threshold,
  decay, synergy-gate expectations
- `tests/tests_beta.cpp` — whisper power 13.0
- `tests/tests_directives_wave9.cpp` — assassin spike ×0.75 (R1)
- `tests/tests_content_wave9.cpp` — heal exertion feed ×2 (R2)
- `README.md` — wave-10 section; wave-4 numbers updated

All 18 ctest suites pass.
