# Living Wilds revision

The active project remains `unreal 5.8/CultUlhu.uproject`. Press Play, choose a mode, then choose an entity. Original character definitions and kit execution remain authoritative.

The fixed-position impact bug was in the presentation actor: `ACultBlastFX` had no root scene component. Its requested spawn transform was therefore lost before it constructed its visible puffs. The actor now retains its transform, and the gameplay regression checks actual spawned mesh locations in addition to the camera aim value.

The Dark Wilds runtime now creates a 3.6 km square collision mesh before spawning the player. Session noise produces ridges, hills, and broad connecting vales. Three 90 m wide, 60 m high, 160 m long vaults have open passages and walkable ground, surrounded by rock masses and separate creature territories. Imported rock spires, trees, and crystals are instanced across the wider world. The old authored level and original imported assets remain on disk; their old physical meshes are hidden and disabled during this mode.

All three active factions start with fifteen-piece temple compounds: a sanctuary, gate, towers, walls, colonnades, and an obelisk. Seven initial towns contain irregular, separated clusters of eighteen to thirty structures with varied footprints, heights, and a loose local heading. They have central gathering space rather than rows. New towns form at different locations every 120–180 seconds, up to twelve living settlements. Neutral people and creatures replenish toward targets of 100 and 36, respectively; cultists and summoned creatures have their own roles.

Rival entities have their own goals and timers, recruiting, raiding, building, and fighting other entities and monsters. They assign cultists through the original command/obedience system. Construction requires workers to physically reach the site, then uses the original 120 worker-second construction model. Directives persist while other units act independently. Predators select nearby avatars without assuming the human is their only target.

Wars are symmetric per faction pair. War with another bot does not imply war with the human. Aggression against another faction can cause retaliation. The original core WarSystem uses player-relative faction ids; the native binding offsets opponent ids for each faith's private war ledger. Owned temple and constructed parts carry faction tags so their owners do not destroy them with their own area attacks.

The larger population exposed an obsolete 96-person summon cap. Complete summoned groups are now planned and checked against a 512-person safety budget before costs are spent.

This is a local free-roam simulation. Buildings still use modular imported assets, and people/summons retain prototype bodies. Terrain and AI need continued playtesting and art direction. There is no strategic campaign persistence, multiplayer authority, or full navigation-mesh planner yet. Local cultist obstacle steering avoids nearby building collisions.

## Validation

Development Editor build succeeded. Four actual-game suites passed together in `Saved/Logs/LivingWildsReleaseGameplay.log`: CanonicalGameplay, GroundSpawn, LivingWorld, and MinorCreatures. They include actual impact mesh placement, four-creature summons, traversal, cavern clearance, temple ownership/counts, city density, elevation variation, and rival wars independent of the human. Visible-editor menu and extended autonomy checks are recorded separately in `Saved/Logs/LivingWildsVisible.log`.

The visible editor autonomy test also passed after observing 100 game seconds: one completed rival structure, four active builders, and twenty converters, with rival entities pursuing non-player targets. That first visual run exposed imported material usage flags; eight material copies now enable instancing without changing the originals. Dynamic lighting replaces stale baked reflection captures for the generated runtime terrain. Rubble uses ordinary LOD rendering.

Rival raid/war commitments now include long-distance travel time rather than changing goals during the journey. Idle cultists can be reassigned into military tasks while builders keep working. Avatar steering checks slope, ground height and upper-body obstacles, and can smash hostile obstructions. Military orders are refreshed on target changes instead of rolling obedience every two seconds. City and replenishment civilian spawns check a clear capsule before spawning.

Final validation: `CultUlhu.Menu.WorldAutonomy` passed again with no automation errors or warnings in `Saved/Logs/LivingWildsFinalEditor.log`, after 100 game seconds (one completed rival structure, four active builders, nineteen converters). Both menu tests passed without warnings in `LivingWildsReady.log`. The final targeted CanonicalGameplay rerun passed in `LivingWildsCavernAim.log`, including four summons on the aimed cavern floor rather than its roof. Current screenshots: `Saved/Screenshots/LivingWildsVista.png` and `LivingWildsTemple.png`; both were visually inspected after the material/lighting corrections.
