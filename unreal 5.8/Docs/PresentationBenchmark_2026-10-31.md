# Presentation and fun benchmark — 2026-10-31

## Current verdict

CULT-ULHU has more working systems than its visual presentation communicates. The current captures still read as a prototype. It has not reached the requested Overwatch level of refinement, and passing AI or score-rule tests does not establish that Domination is as enjoyable as Paladins Onslaught. No review score is assigned.

This update inspected the current gameplay source, a standalone lane/wave capture, a fresh short-viewport tower-warning capture and a real standalone DOTGOO guest capture. It did not perform a new human-controlled full match, listen to audio, or measure low-spec performance. The eight-second network charge and frozen ten-second editor charge are deliberate warning fixtures, not gameplay balance changes; normal tower windup remains 0.25 seconds.

## Observable gaps

1. **Readability in shadow.** Matched-camera checks subsequently confirmed a real guest ambient-light omission: a server-only setup selected the cubemap, while guests captured a nearly black sky. Shared runtime lighting now corrects that omission; Free Roam also receives the missing client presentation and movable moonlight setup. See `RuntimeLightingParity.md`. This resolves a visibility mismatch, not the wider need to audit character separation, roofed areas and all creature materials while retaining the dark theme.
2. **Landmark and route identity.** An initial Domination pass now adds distinct Crown/Hollow silhouettes and symbols, keep detail, ramp/crypt inlays, contextual route hints and two physically tested forward overlooks. Repeated masonry and large blocky structures still dominate; this is not finished architecture. See `DominationArchitecture.md`. Continue refining approach identity, materials and focal landmarks while preserving navigation and combat sightlines.
3. **Scale and camera composition.** Large architecture occupies a substantial part of the aiming view in the short capture. Hero bodies, elevated geometry and third-person framing need to be evaluated together along real traversal routes, rather than in separate empty fixtures.
4. **Competing information.** Mode title, binding help, first-ability prompt, level progress, relic status, announcements, objective labels and hero plates all compete for attention. The ward-warning pass gives immediate danger priority over overlapping routine messages; the overall hierarchy still needs simplification and contextual onboarding.
5. **Motion quality and asset readiness.** Working locomotion and automatic retopology are separate milestones from convincing character acting. Static retopology copies still require UVs, detail baking, rigs and deformation review. They have not replaced the current game models. Artist-directed motion, contact/IK and consistent presentation across the release roster remain open.

## How to judge the comparison

Blizzard's published map-rework notes identify navigation/readability and access to high ground as design concerns. That is a useful benchmark for our map work: an extra tunnel or tower should create a readable decision with counterplay, not merely fill the scene. [Blizzard: Flashpoint map reworks](https://overwatch.blizzard.com/en-us/news/24215719/undefined/)

For the owner's Onslaught comparison, evaluate the complete point-fight loop: clear approach choices, time to rejoin after defeat, whether a flank produces an understandable advantage, useful ranged/support positions, a readable contest state and opportunities for a losing team to regroup. Matching capture/kill scoring does not prove those qualities. Compare complete owner-played Domination matches after changes; automated checks establish rules and execution, not enjoyment.

## Next substantial review passes

- Continue shadow/character contrast review across real combat positions, and improve route landmarks with the existing original/approved asset policy. Matched-camera host/guest checks now cover the initial lighting mismatch.
- Measure full Domination approach/flank/rejoin routes and watch live fights from the capture rectangle, high ground and tunnel exits. Remove positions that produce unavoidable damage or offer no useful decision.
- Reduce permanent instructional strips through contextual prompts, retaining critical ability, target, objective and danger information.
- Add persistent comfort/settings controls and test them through actual resolution, input-focus and camera changes.
- Revisit team positioning and support behavior in complete matches, including how both teams leave spawn, regroup and respond to a failed push.
- Teach eligible ranged bots when to approach, pressure from and leave the new Domination overlooks, while retaining capture and underground-flank roles. Controlled traversal/projectile fixtures do not prove independent tactical use.

These remain development work. See `HandsOnReview_2026-10-30.md` for the broader ten-task review and `WardDangerReadability.md` for the focused combat-cue implementation and its verification entry points.
