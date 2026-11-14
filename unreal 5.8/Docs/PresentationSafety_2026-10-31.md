# First-spawn, camera and test reliability pass — 2026-10-31

This pass completes the first-spawn and nearby-body camera items from the hands-on review, and repairs the standalone test failure reported by the owner.

## Changes

- Competitive initial possession now uses the same team formation position as respawning. The arena was already built in InitGame; the initial pawn still used an unrelated legacy PlayerStart before the director corrected it. Both routes now share one position helper, so grounding happens in the actual team starting area.
- Friendly meshes intersecting the immediate camera corridor are culled through Unreal's per-view hidden-component list. They reappear when clear of the eye/reticle corridor. Enemy meshes, physical collision and actor visibility flags are preserved. This also covers long tails, wings and weapons reaching into the camera from a more distant character root. The player's own first-person body is excluded before rendering.
- The wave2/wave26 save tests now use unique, portable temporary directories. They stop inspecting loaded data after failed I/O or missing records, instead of indexing empty arrays or dereferencing null pointers. Their temporary data is cleaned up after the test.
- Core runners validate the canonical project, stop after failed builds, use noninteractive CTest error handling and enforce timeouts. Identical engine libraries are not recopied unnecessarily. The full-suite entry point is `Tools/RunHeadlessTests.ps1`; `Tools/BuildCanonicalCore.ps1 -Test` stages the engine-linked library and tests it.

## Verification

- Canonical CultUlhuEditor Win64 Development build succeeds.
- Rebuilt full core suite: 109/109. Engine-linked core suite: 18/18.
- The two repaired suites pass under restricted filesystem permissions. With an intentionally unavailable temporary directory, each exits with an ordinary test-failure code of 1, rather than an access violation or blocking error dialog.
- Eight distinct native checks pass: crowded cameras in all three modes, Cthulhu/Mi-Go framing in both competitive modes, and the Free Roam save/load regression.
- A separate standalone game process navigates the real menu/travel/initial-possession flow through Domination, DOTGOO and Free Roam. Every mode has a walkable capsule, functional held movement input, gameplay input focus and zero initial safe-ground errors.
- Camera fixtures put real allied/enemy rigs in the same camera volume, enclose the first-person eye, move allies out of the corridor, and place a long body part near the camera while its owner is distant. Friendly collision and global visibility remain intact; enemies are never added to the hidden set.

[Machine-readable evidence](PresentationSafetyResults_2026-10-31.json) lists the checks and private local log locations. These checks establish the specific spawn/camera/I/O behavior; they do not score combat fun, certify every kit, or provide a low-spec performance benchmark.

## Next review work

Damage/danger readability is the next open presentation item. Arena landmark/cover composition, squad-level evaluation, action-led onboarding, simpler belief feedback, character-selection readiness and persistent comfort/settings remain on the review list. The earlier rig mechanics/blending pass is documented separately in `RigMotionGuide.md`; artist-directed animation polish remains useful.
