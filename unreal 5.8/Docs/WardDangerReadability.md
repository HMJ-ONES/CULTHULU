# Ward danger readability

Tower shots already track their target and require a timed dash to break the lock. Running, jumping and walking behind cover do not cancel a released ward shot. Ordinary hero and minion projectiles retain physical collision and can miss.

## Player-facing cue

A compact panel above the crosshair appears only while a living hostile ward has an actual shot locked on the local player. It points toward the firing ward. During charge it shows the release countdown; after release it says the shot is incoming. The second line explains the Shift tap, or reports the remaining dash cooldown. The panel disappears when the shot ends, the source dies, the player dies or the player dashes. Competitive local menus suppress this panel without pausing the match. While the warning is present, routine announcements yield to it, projected goal labels that intersect it are hidden, and hero nameplates reserve its measured bounds.

This does not predict future tower aggro, invent a lock on nearby bystanders, or warn about attacks aimed at minions. It does not change damage, range, cooldowns, projectile speed or the existing 0.25-second tower windup. Range boundaries and persistent tower aggro presentation remain further work.

## Multiplayer timing

`ACultSkillAttack` replicates the actual ward target, locked-shot flag and authoritative remaining charge in its native actor state. The server updates that countdown from the same attack age that controls damage. On receipt, guests subtract local elapsed time and estimated half-round-trip latency. This avoids relying on the slower generic combat snapshot or on a newly joined guest's still-converging GameState clock. A first real guest capture exposed that clock-skew problem: an eight-second fixture incorrectly displayed 9.4 seconds, so the deadline-based version was rejected. The network probe now requires the displayed remaining charge to stay within the server's eight-second fixture duration and records both values.

Packet loss, latency and the accuracy of the ping estimate still affect warning delivery. This is not a claim that a 0.25-second charge is sufficient under every connection condition.

## Verification entry points

- Native `CultUlhu.Menu.WardWarning`: actual FireAt metadata, target/faction/source eligibility, release state, shot expiry and algebraic canvas bounds at 640×360, 960×540, 1280×720 and 1920×720; rendered short editor viewport fixture. The separate network probe renders a full 1280×720 game window.
- `Tools/RunRoomNetworkProbe.py --mode 2`: real standalone host and guest, server-created locked shot on the guest, required guest observation of replicated target and bounded authoritative countdown, and a guest warning screenshot. Existing movement, combat, wave movement, non-pausing menus and bot replacement checks remain required.
- Existing skill-combat, combat-feel and tactical tests cover the underlying combat rules. A warning test alone does not prove those rules or player enjoyment.

Screenshots and logs belong under `Saved/`; do not commit build output, live room codes or personal network addresses.

## Remaining presentation gap

The standalone lane capture still shows repeated masonry, large primitive silhouettes and competing text strips. This warning pass addresses one combat comprehension issue. It does not complete the wider arena art, readability, animation, comfort-settings or fun evaluation work described in `HandsOnReview_2026-10-30.md`.
