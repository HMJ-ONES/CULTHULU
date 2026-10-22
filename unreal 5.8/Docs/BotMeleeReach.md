# Bot melee reach and approach

Bots previously started primary swings whenever a target's horizontal centre
was within 3,000 cm. MOBA bots also stopped approaching enemies at a fixed
1,800 cm, independent of their own size or the target's height. Neither value
described the physical arm sweep. The twenty-metre Mi-Go could commit a swing
that could not reach another hero, or stop outside contact with a small minion.

The corrected native baseline reproduces the unreachable swing in both modes.
The nearby control swing still makes real contact and deals damage. Against a
small minion in MOBA, thirty manual 16ms movement steps produce only 2.1 cm of
advance, leaving 1,797.9 cm of separation. This is a controlled real-character
movement fixture on an isolated platform, not a human match measurement.

CultMeleeGeometry now supplies the physical primary attack and bot decisions
from one shared definition: origin at 48% of actual capsule height, reach at
90%, and the existing clamped sweep radius. The numeric physical definition
is unchanged. The bot only starts a primary swing when the target's actual
capsule, or colliding bounds fallback, plausibly intersects that reach.

The MOBA approach distance includes the target capsule's radius and vertical
spine clearance relative to the arm origin. It targets 80% of the possible
horizontal contact extent, bounded between 200 and the original 1,800 cm.
This gives a short-reach giant room to approach a low target. It does not
teleport the bot or extend an attack: normal character physics still walks,
collides and stops. Jungle-monster spacing, assigned lanes, wave/tower rules,
Domination's scoring/flank routing and retreat decisions are retained.

The decision is a range opportunity, not a guaranteed hit. The committed swing
still has a windup, moving arc and actual swept collision. Cover, changes of
position, elevation, jump/dash evasion and timing can defeat it. Ability/RMB
ranges, original kit files, damage, stamina, cooldowns and player input are
unchanged. This does not implement complete role-aware spell selection.

## Verification

BotMeleeReachResults.json records the corrected baseline, accepted build,
native outcomes and exact source/input hashes. The focused fixtures in both
modes use the real BotThink path, suppress spell cooldowns to isolate primary
decisions, and advance the actual attack for the nearby damage control. The
MOBA fixture additionally advances the real character movement component.
After the fix it moves 454.0 cm and reaches 1,346.0 cm separation in the same
thirty-step window. The assertions and near-control requirements are retained.

An earlier baseline setup incorrectly placed the MOBA hero outside the bot's
assigned lane. That case tested lane discipline rather than attack reach and
is excluded from the corrected comparison. The fixture assigns the middle
lane before rerunning the original production behavior. Both corrected
baseline tests fail on the actual reach defect; the MOBA stopping check also
fails. The report preserves both logs and distinguishes their scope.

Separate native tests exercise both match integration fixtures, physical hit/miss combat,
combat feedback and delayed defensive-dodge decisions in both modes. This
is stronger than checking the helper's returned numbers alone, but it does
not prove all-character tactics or human battle quality. The match fixtures
include short live-play windows and scripted transitions, not uninterrupted
human matches from start to finish. No art assets,
downloads, images, package archives or standalone build are added in this pass.

## Remaining gaps

Bots still use broad shared spell/RMB triggers rather than a complete model
of each original kit's best engagement distance. Enemy selection, retreat
timing, support positioning, high-ground pressure and tactical coordination
need broader work. Focused controlled contact and two match checks do not
prove long-session balance, all-size navigation, human fun or Onslaught-level
play. Finished animation, audio/VFX cohesion and environmental art remain
substantially below the requested production reference. The full goal stays open.
