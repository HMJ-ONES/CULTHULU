# CULT-ULHU performance notes (third-shift audit, Oct 7–8 2026)

Method: timed the balance sim, measured real encoded snapshot sizes with a
harness (`encodeSnapshot` over N entities), and grepped tick/update paths for
per-frame allocations and O(n²) loops. No speculative refactors were made.

## Balance sim: 0.14 s for the full matrix
`cultulhu_sim` (19 scenarios × 8 seeds × 3600 ticks = ~550k ticks) completes in
~0.14 s wall-clock on the sandbox VM. The sim is not a bottleneck; it can be
re-run freely (e.g. in CI or pre-push).

## Snapshot sizes (measured, framed wire bytes)
The v1 protocol is text (`key=value;` pairs), ~36–37 bytes/entity on the wire
(vs ~21 bytes packed binary — a 1.7× readability tax, documented in Protocol.h).

| entities | snapshot bytes | @20 Hz per client |
|----------|---------------|-------------------|
| 10       | 370           | 7.2 KB/s          |
| 50       | 1,810         | 35.4 KB/s         |
| 100      | 3,611         | 70.5 KB/s         |
| 200      | 7,311         | 142.8 KB/s        |
| 500      | 18,411        | 359.6 KB/s        |

ClientInput is 50 bytes @30 Hz = 1.5 KB/s — negligible.

**Verdict:** fine on a Radmin virtual LAN at expected entity counts (<200).
The documented v1 limitation ("revisit past ~200 entities") still holds; past
that, the fixes are delta compression + interest management, not micro-opts.

## Tick-path scan: no hot spots found
- `WaveOfDomination::update` — the only nested loop found is
  victims(≤5) × entities, active only during the wave/hold phases. Bounded,
  ~2.5k cheap checks/tick worst case. Not a concern.
- All other per-tick loops sampled (`AmbientBehavior`, `CrowdControl`,
  `BeliefSystem`, `BuilderAI`) are single linear passes; no per-frame heap
  churn in hot paths (`make_unique` only on construction-site creation, which
  is event-driven).
- `encodeSnapshot` does allocate strings per entity per snapshot (20 Hz);
  ~16k small allocs/sec at 100 entities. Negligible on any host from the last
  decade; a binary encoding would halve it if bandwidth ever matters.

## Correctness gap noticed (not fixed — flagged for scheduling)
`Dhole::fearAuraRadius()` / `fearAuraStrength()` (18 m, 12 fear/s, wave 13)
have **no consumer**: nothing in `src/` applies the aura per-tick. The advertised
behavior is data-only. Implementing it is a small feature (one system iterating
entities near a surfaced Dhole), not a perf fix — left for a content wave.

## Recommendations (future, not done tonight)
1. Past ~200 entities: delta compression + interest management (as documented).
2. If bandwidth ever matters: packed-binary snapshot encoding (~40% smaller).
3. Implement the Dhole fear-aura application (correctness, not perf).

## Wave 19 measurements (2026-10-08)
- `mapinfo` (driver): instant. `dungeon pnath 4242`: 7 ms.
- MapLoader: ruined_city.map (90 placements) 2 ms, eldritch_battlefield.map (72→120 placements after wave-19 art) 1 ms.
- ValeOfPnath(4242) generation: <1 ms (20 rooms).
- Full balance sim (18 scenarios x 8 seeds x 3600 ticks, 144 runs): 0.11 s.
- decode_assets.py: 0.47 s for 82 assets; now skips byte-identical outputs (no redundant rewrites).
- Decoded asset payload: ~5.0 MB (glb+png, excl. .b64 sidecars). Enforced by tests_wave19: world/ <= 2000 tris/model, characters+creatures <= 8000, textures <= 1024px, total <= 12MB.
- Asset loading review: ModelCatalog uses function-local static maps (loaded once, cached). MapLoader parses per call (1-2 ms — no cache needed). Driver is headless (paths only, no GLB binary loads). No redundant-load issues found; only fix was the decode skip above.
