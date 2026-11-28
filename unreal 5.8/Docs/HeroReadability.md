# Competitive hero identity and health

Recent live-combat captures showed a basic readability gap: the placeholder
vessels have similar silhouettes, while their identity and remaining health
were mostly available only through the current-target panel. This makes
choosing a damaged opponent or recognizing a teammate who needs healing harder.
Nameplates address that information gap; they do not replace finished character
art, animation, distinct silhouettes or a human test of combat quality.

Competitive matches now show up to four contextual hero plates, limited to two
on short viewports. The local combat focus takes priority, followed by injury,
screen relevance and enemy presence; injured allies receive greater injury
weight so a healer can find them. Each plate uses the actual hero name and
health. Allies use a plus mark and teal; enemies use a diamond and coral.
Health has five divisions. Focus receives a gold edge. Team recognition thus
has a shape cue as well as a color cue.

Candidates exclude the local hero, defeated/hidden vessels, off-screen heads
and heroes more than 300 metres from the current view. A real unobstructed
visibility ray to the target's centre or head is required. Remembered combat
focus never bypasses that rule. Bodies can occlude each other. No persistent
through-wall outline or new information about invisible targets is added.

Measured names, bounded placement choices and a short leader connect each
plate to its projected head. Reserved areas protect the upper match copy,
tactical map, ability HUD, target details, reticle and projected objective.
Priority chooses which heroes fit; remaining cards can be omitted in a crowd.
The console variable `cult.Match.HeroPlates` disables this presentation only.
Free Roam retains its existing population inspection and combat HUD.

## Render projection correction

Initial dedicated captures exposed a real coordinate mismatch: controller
projection used the native editor viewport while the screenshot rendered a
different view. Plates floated away from bodies and disappeared in the compact
capture. World labels now project through the actual Canvas SceneView. The
same helper corrects projected objective guidance, damage numbers and small
windup labels. Gameplay aiming and mouse-input coordinates are unchanged.

The first fixture also expected a 1920-wide Canvas inside a deliberately
constrained 16:9 camera capture. That wait could not finish: the actual play
Canvas was 1280x720 between pillarboxes. After preserving its log/images, the
specific task-owned test process was stopped. The corrected fixture tests
both an unconstrained 1920x720 view and a 1920x720 image containing the genuine
1280x720 constrained view. It checks PNG dimensions and actual Canvas dimensions
separately.

The next render run caught a different compact-layout defect: two other cards
could be shown while the active target was dropped. Short lateral placement
options were insufficient in MOBA's shorter region above the contextual
minion-throw instruction. Head-centred side positions retain that target
without relaxing the priority or overlap assertions. Leaders join the nearest
plate edge rather than crossing the card. Floating damage numbers and small
windup labels also yield to the lower contextual HUD; normal views retain
their damage numbers. The native fixture checks both cases in actual glyphs.
Visual review after the first fully passing batch still found connecting lines
crossing earlier cards. All leaders now draw beneath all name/health cards;
this preserves the tested layout while keeping later lines off earlier text.

## Evidence and limits

HeroReadabilityResults.json records the builds, rejected runs and final results.
The native fixture enters each competitive mode through its real menu, prepares
four visible heroes on the actual map, applies real damage and freezes the
world. It captures presentation off/on, compact, wide, solid-cover and
pillarboxed views. It checks the real forty-health enemy focus, an injured ally,
plate priority/counts, bounds, pairwise overlap and remembered-focus occlusion
behind a real wall. This is a controlled render/visibility fixture, not a live
human firefight or an all-camera navigation test.

The paired review images use one frozen Domination scene, unchanged geometry,
camera and health. Before uses the plate opt-out in the same new build;
projection/HUD separation fixes are active in both images. Placeholder bodies, unanimated attacks and blockout
architecture remain obvious. Small-screen layout checks do not establish
comfortable text size or accessibility. Crowded-fight stability, all display
setups, sustained performance and human decision-making improvements remain
unmeasured. No final-art or Overwatch-quality claim follows from these checks.

The final canonical Editor build succeeded in 9.67 seconds. All five final
native tests passed with zero automation errors: both HeroPlates mode checks,
CombatFeel, DominationEventReadability and MobaEventReadability. The plate
fixtures produced twelve exact-size PNGs; each mode showed 0/4/2/4/2/4 cards
for opt-out, normal, compact, wide, covered and pillarboxed cases. Actual Canvas
glyphs fit their plates. The native summary's `canvas=...` label reports the
requested image dimensions; separate assertions validate the genuine smaller
Canvas inside pillarboxed captures. The report distinguishes those dimensions.
All 423 existing Content files remain byte-identical to checkpoint ab3ee27;
character inputs have no substantive changes (110 checkout line-ending-only
differences and five byte-identical files).

This pass adds source, small reports and two review images. It downloads no
assets and creates no additional standalone package or duplicate project.
