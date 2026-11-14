# Population decision scheduling — 2026-10-21

Population actors continue ticking appearance, control projection, order/attack
timers and native character physics every frame. Routine target selection,
obstacle sweeps and ground-lookahead decisions reuse their last movement intent
between refreshes. The default minimum refresh interval is 0.1 seconds; actual
decision frequency also depends on frame time. Movement input is still fed every
frame, so reducing decision calls does not turn walking into intermittent input.

New accepted orders, recruitment, release from captivity and stop-state changes
invalidate the wait. Fear direction is evaluated every frame. Battle ownership,
war/enemy selection rules, loyalty/belief logic and physical attack windups remain
on the existing paths. The ground probe keeps its existing 200 cm lookahead at
the default speed/interval and extends it for longer diagnostic intervals.

The captive-follow branch now respects both stun and root/stop state. Previously
it could add follow movement after stopping the movement component. Active
control snapshots still need refreshing by their caster; scheduling does not
extend their lifetime or clear another caster's state.

## Measured result

The canonical Editor build and these native tests passed:

- CultUlhu.Menu.ArmyCommands
- CultUlhu.Menu.PopulationDecisionProfile
- CultUlhu.Menu.PopulationMotion
- CultUlhu.Menu.StagedHumanPoses

The motion test uses real character movement and collision on a platform. With a
two-second diagnostic interval, a new order reverses movement within 0.4 seconds;
stun/root stop captive following, and release resumes movement. At the default
interval, a zero-loyalty battle order lands a delayed physical hit and a march
toward the edge stops at X=2819.2 cm on a platform ending at X=3000 cm.

The profile alternates per-frame/scheduled/scheduled/per-frame decisions in one
live Free Roam world. Each window contains 120 recorded frames; population stays
182, with three heroes. The total actor count grows from 480 to 492 as the living
world continues. No war, belief, construction or bot simulation is frozen.

| Metric | Per-frame decisions | Scheduled decisions |
| --- | ---: | ---: |
| Population decision time | 12.710 ms | 6.389 ms |
| Total population tick time | 17.534 ms | 10.585 ms |
| Whole frame time | 89.532 ms | 79.758 ms |
| Character movement | 14.195 ms | 13.615 ms |
| End-of-frame updates | 19.288 ms | 18.748 ms |
| GPU time | 10.659 ms | 10.681 ms |

This is a 39.6% reduction in population tick time and a 10.9% reduction in frame
time in this editor fixture. It is not foreground/standalone FPS evidence or a
claim of acceptable combat performance. The world evolves between windows;
returning to per-frame decisions restores the higher cost, which strengthens the
specific scheduling comparison. Decision calls average about 173 per frame in
the per-frame windows versus 87 in the scheduled windows. Some scheduled frames
still contain a full batch of decisions; peak work distribution needs attention.
Nested timing scopes are not additive.

PopulationSchedulingResults.json contains per-window means, p95s, sample counts
and source CSV hashes. Local raw evidence is PopulationSchedulingVerification.log
and Saved/Profiling/CultRuntime/PopulationDecision_*.csv. The summarizer writes
count metrics to a separate _Counters.csv instead of mislabelling them as ms.

The earlier independent 192-person baseline is a different generated scene and
is not used to calculate the percentages above. Long-frame movement warnings,
stuck-unit cases, character movement cost and component/end-of-frame work remain
unresolved. Overall frame time is still far too high for the requested game.
The new scheduling code also needs a later standalone build/runtime check.
