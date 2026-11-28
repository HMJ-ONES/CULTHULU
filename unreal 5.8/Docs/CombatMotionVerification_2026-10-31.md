# Combat, aiming and coded motion verification — 2026-10-31

Completed in the owner's requested order: tower/tactical AI changes; facing, crosshair and animation-state fixes; hands-on review with ten prioritized tasks; then measured rig mechanics and fluid animation transitions.

- Ward shots track their chosen target through running/circling. A timed dash after windup breaks the shot; jumping does not. DOTGOO uses an eight-second shared dash cooldown; other modes retain 1.15 seconds. Wave aggro and tower protection remain.
- Competitive bots use authored ranges, teammate-pressure target priorities, spaced firing positions, safe ambush/escape dashes and occasional repositioning jumps. Ranged Domination support can stand outside the seal while a teammate captures, and returns to capture when needed. MOBA route duties remain.
- Human facing stays with aim while backing up. Screen-center deprojection supplies projectile aim in both views; melee offsets no longer rotate sideways with the body. The crosshair is compact white.
- Mi-Go and Yig face forward. Twenty-seven small original gameplay clips supplement the supplied Prowl/idle clips. Measured joint axes, bounded tail waves, eased strike phases and four hand-driven swords replace the initial generic bends. Native pose/loop blends preserve continuity; movement cadence follows actual travel speed and direction.

## Evidence

Canonical CultUlhuEditor Win64 Development build succeeds; both validators pass. **21 distinct native checks have successful final results**, including both live match modes, tower mechanics, input-independent combat opportunities and threat responses, four camera cases, real crosshair hits while backing up in both views, all approved models, rigid weapon geometry and pose continuity/articulation. **1,024 numeric motion samples pass**, with maximum authored joint delta 34 degrees and floating-point-only contact/weapon-follow error in the checked recipes.

All **564 inherited Content files outside the deliberately updated registry and credits remain byte-identical** to the prior checkpoint. Source meshes, skeletons, textures, maps, authored kits, beliefs and engine-agnostic core are preserved. The 27 generated animation assets total about 2 MB; previous owned generated clips and original metadata are preserved locally. No generated build output is published.

The asset authoring script saved all clips and its completion report, but that process returned an access-violation status after its shutdown log. Fresh rendered editor sessions subsequently loaded the saved assets and passed their native checks. Initial fixture/compile/capture timing failures and corrections remain in Saved/CombatTactics20261030 rather than being hidden from the evidence.

Reviewed real rendered poses: [Mi-Go strike](CombatMotionEvidence/mi_go_attack.png), [Yig strike](CombatMotionEvidence/yig_attack.png), [Yig locomotion](CombatMotionEvidence/yig_walk.png). These are pose samples, not a claim of cinematic animation quality.

## Owner documents

- [Honest hands-on review and ten prioritized tasks](HandsOnReview_2026-10-30.md)
- [Rig mechanics, code files and simple reproduction steps](RigMotionGuide.md)
- [Tower and tactical AI behavior](CombatTactics_2026-10-30.md)
- [Machine-readable verification](CombatMotionResults_2026-10-31.json)

The review found a transient standalone spawn warning and camera obstruction by nearby bodies. Those remain explicit backlog items, alongside deeper onboarding, environment identity and comfort settings. The bounded UI playtest does not certify every kit, sustained low-spec performance, audio or multiplayer. Terrain-adaptive foot/coil IK, dedicated first-person arms and artist-directed acting remain further work.
