# Free Roam: belief screen and follower commands

## Controls

1. Hold **ALT** to use the world cursor. Hover directly over a cultist's body for **1.5 seconds** to inspect their activity, health, allegiance and recent contribution. A brief pass shows nothing. Only one card appears; it fades in and fades out over about 0.22 seconds. Selected or neighboring followers do not add more cards. Scenery blocks inspection.
2. Press **B** to open beliefs. It supplies its own mouse cursor; ALT is not required. Click a belief to see its short benefit and tradeoff. Only currently adopted beliefs show fervor values and filled gauges. Inactive choices are muted; pending choices show their adoption timer.
3. Select an inactive belief, then click the slot to replace. With an empty slot, choose **Adopt into free slot**. Adoption uses the original delay; it is not an instant swap. **Rules** opens the complete existing explanation, **Less** returns to the compact view, and **Outcomes** opens the existing journal. B or Close returns to play. The world continues running.
4. To issue world actions, hold **ALT**, click a target or drag over your followers, release the mouse, and click an order. Keep ALT held until choosing the order. The command panel uses a responsive grid so all actions remain inside the view, including constrained camera views.

## Actions moved from B to ALT

| Order | What actually happens |
| --- | --- |
| Rest selected followers | Selected willing followers rest for 30 seconds. Without a selection, available followers are considered. Dreams receives actual resting-time rewards. |
| Mass convert (entity) | The original entity action converts three real civilians, requiring adopted Conversion and its ready 200-second cooldown. |
| Place capture trap | The original entity action places a single-use trap on grounded terrain. A real capture triggers its reward. |
| Torture captive | Click an owned living human captive within 12m of your temple. A willing selected/available follower approaches and completes 20 seconds of physical work. Damage, reassignment, a lost participant or a destroyed temple interrupts it. Issuing the order gives no power. A captive rewards once. Captured beasts retain their original direct handling rule and once-per-creature reward. |
| Sacrifice at temple | Starts the original 30-second rite with a willing follower and an actual offered person at an intact owned temple. Participants must already be within 12m. The victim dies on completion; interruption follows the original penalty. |
| Necromancy at temple | Starts the original interruptible 45-second Magic rite with a willing follower at an owned temple. |
| Breed captured creatures | The original temple pairing requires adopted Breeding and two owned captured beasts nearby; offspring may be feral. |
| Imprison cultist | Imprisons the targeted owned cultist and applies the existing belief consequences. |
| Punish infringer | Applies the existing punishment/alignment rules only to an eligible targeted cultist. |

Worker orders respect the existing loyalty, cult state and belief obedience model. Refusals produce a disobedience message. Selecting a group limits the workers considered. Unselected battle groups keep fighting; explicit worker reassignment can release a selected fighter, while temple rites ask you to disengage fighters first. Combat orders retain their existing unconditional obedience.

The underlying twelve belief systems, exertion feeds, rewards and adoption rules are preserved. Inactive fervor is hidden in the UI rather than erased from the simulation. Beliefs remain exclusive to Free Roam. Co-op keeps server-owned world actions and host-owned belief changes. Existing version-2 saves remain compatible; human captive work uses the already saved society job structure.

## Verification

See [the verification record](FreeRoamBeliefUIResults_2026-10-31.json) for the canonical build, native input/layout checks, hover rendering, physical job accounting and save regression results. Screenshots are generated locally under `Saved/Screenshots`; personal saves, logs, build output and screenshots are excluded from the source publication. This pass does not claim a packaged release or a full multiplayer playtest.
