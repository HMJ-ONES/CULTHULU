# Presentation and fun benchmark — 2026-10-31

## Current verdict

CULT-ULHU has more working systems than its visual presentation communicates. The current captures still read as a prototype. It has not reached the requested Overwatch level of refinement, and passing AI or score-rule tests does not establish that Domination is as enjoyable as Paladins Onslaught. No review score is assigned.

This update inspected the current gameplay source, a standalone lane/wave capture, a fresh short-viewport tower-warning capture and a real standalone DOTGOO guest capture. It did not perform a new human-controlled full match, listen to audio, or measure low-spec performance. The eight-second network charge and frozen ten-second editor charge are deliberate warning fixtures, not gameplay balance changes; normal tower windup remains 0.25 seconds.

## Observable gaps

1. **Readability in shadow.** The guest capture has large structures that become nearly black silhouettes. The pale lane is visible, but individual surfaces and nearby combatants are harder to read. A matched-camera host/guest lighting comparison is needed before attributing this to networking. Improve shadow fill and character separation without replacing the dark theme with bright daylight.
2. **Landmark and route identity.** Repeated gray masonry, similar pillars and large simple structures dominate the capture. More geometry alone will not create a polished arena. Give each approach a recognizable silhouette, material family and focal landmark; keep navigation and combat sightlines clear.
3. **Scale and camera composition.** Large architecture occupies a substantial part of the aiming view in the short capture. Hero bodies, elevated geometry and third-person framing need to be evaluated together along real traversal routes, rather than in separate empty fixtures.
4. **Competing information.** Mode title, binding help, first-ability prompt, level progress, relic status, announcements, objective labels and hero plates all compete for attention. The ward-warning pass gives immediate danger priority over overlapping routine messages; the overall hierarchy still needs simplification and contextual onboarding.
5. **Motion quality and asset readiness.** Working locomotion and automatic retopology are separate milestones from convincing character acting. Static retopology copies still require UVs, detail baking, rigs and deformation review. They have not replaced the current game models. Artist-directed motion, contact/IK and consistent presentation across the release roster remain open.

## How to judge the comparison

Blizzard's published map-rework notes identify navigation/readability and access to high ground as design concerns. That is a useful benchmark for our map work: an extra tunnel or tower should create a readable decision with counterplay, not merely fill the scene. [Blizzard: Flashpoint map reworks](https://overwatch.blizzard.com/en-us/news/24215719/undefined/)

For the owner's Onslaught comparison, evaluate the complete point-fight loop: clear approach choices, time to rejoin after defeat, whether a flank produces an understandable advantage, useful ranged/support positions, a readable contest state and opportunities for a losing team to regroup. Matching capture/kill scoring does not prove those qualities. Compare complete owner-played Domination matches after changes; automated checks establish rules and execution, not enjoyment.

## Next substantial review passes

- Run a matched-camera lighting/contrast check, then improve shadow readability and route landmarks with the existing original/approved asset policy.
- Measure full Domination approach/flank/rejoin routes and watch live fights from the capture rectangle, high ground and tunnel exits. Remove positions that produce unavoidable damage or offer no useful decision.
- Reduce permanent instructional strips through contextual prompts, retaining critical ability, target, objective and danger information.
- Add persistent comfort/settings controls and test them through actual resolution, input-focus and camera changes.
- Revisit team positioning and support behavior in complete matches, including how both teams leave spawn, regroup and respond to a failed push.

These remain development work. See `HandsOnReview_2026-10-30.md` for the broader ten-task review and `WardDangerReadability.md` for the focused combat-cue implementation and its verification entry points.
