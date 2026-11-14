#Latest update: [Initial beliefs, entity preaching, and ALT armies](ArmyCommands.md). Player combat orders bypass loyalty and preserve separate fronts; building still checks loyalty and doctrine.

Latest combat-feel update: [Responsive melee, enemy recovery, and readable combat feedback](CombatFeel.md).

 Aimed combat and clean UI

This update binds the original character definitions to actual UE collision instead of immediate target selection. The original roster, ability names, stamina costs, cooldowns, effect categories, buffs, healing, summons, control durations, and faction beliefs remain the source. Native delivery geometry and PvE health are explicit tuning added for this request.

## Controls and movement

- A fresh Shift press with WASD, or a fresh direction press while holding Shift, starts one directional dash. Continue holding Shift to sprint; release it to walk. The input edge must be pressed again for another dash. Diagonal directions normalize.
- Dodge costs 18 stamina, travels for 0.22s at 3.5 times walking speed, has a 1.15s cooldown, and protects for the first 0.18s. CharacterMovement applies a constant root-motion force and sweeps the actual capsule against walls; it does not teleport through them. The force retains vertical jump movement instead of replacing it, and ground friction cannot erase a dash at low frame rates.
- A valid jump gives 0.14s of takeoff protection. After that, attacks test actual airborne geometry: jumping over low attacks works; jumping into a projectile can still hurt. It is not permanent airborne immunity.
- Original character dash abilities apply their source distance as a 0.30s movement force with collision and 0.18s of protection. Terrain and finishing momentum affect total displacement. Unpossessed AI entities temporarily enable character physics for the same movement.

## Contact rules

LMB winds up for 0.12s and sweeps a visible arm/slash for 0.20s. Its reach scales with the selected entity's real height. Aim controls pitch and direction; the arc locks when the attack starts. Sphere sweeps sample every 0.012s within a frame to avoid tunneling. Damage and combo control apply once per contacted target. Empty swings and blocked swings do no damage. The old omnidirectional ground damage has been removed. Melee belief rewards are emitted once on first contact, not on empty swings. The arm moves with its owner while its aim direction remains committed. The visible articulated vessel arm is a temporary mesh treatment until the player's authored entity rig arrives.

Projectiles launch after their windup, travel on a locked line, use swept sphere collision every frame, stop at the first solid obstruction, and expire at range. They do not select or follow a target. Misses still consume the source cost and cooldown. Friendly bodies can block a projectile. Beams show their lane before a delayed ray-volume test. Delayed areas test current occupants at activation, not the people selected when casting; solid cover blocks them. Ground disks are 120cm tall so jumping over them is meaningful. Cones and spheres have their own 3D volume tests.

Enemy and cultist swipes use this same contact system with visible windups. A small enemy warns for 0.35s, a brute for 0.65s, and a cultist for 0.28s. They lock direction before the strike. Dead owners cancel unfinished attacks. Stun/fear/captivity cancels unfinished melee; projectiles already committed to flight continue. A creature stands and commits while its attack animation plays.

Cthulhu's original held Wave of Domination state machine remains. Its wave candidates are limited by physical sight, vertical overlap and dodge state; held slams and launched victims use swept native capsules against actual 3D bodies and world cover, with 16ms simulation steps; its alternate direct stun now travels as a non-homing control projectile (2.5s control, original 7s cooldown). Generic RMB melee has a 0.24s windup and 0.25s arc; ranged heavies travel at 22m/s with their source damage multiplier and control. Pulls move a hit victim with a blocking sweep.

## PvE and corpses

Civilians have 1 HP: any damaging contact kills them. Unbuffed 135-damage melee baselines are imp 135 HP (one hit), ghoul 270 HP (two hits), pale wight 1350 HP (ten hits), and ossified brute 2025 HP (fifteen hits). These counts are baseline tuning, not damage caps: combos, critical positioning, abilities and belief strength can reduce them. Avatar-vs-avatar melee retains the source 10-damage baseline; destruction retains 135.

Civilian one-hit overrides also publish the actual slain event when a former cultist retained more health; a partial core hit cannot hide the resulting kill.

Death copies the original skeletal pose, materials and visible mesh pieces into an unticked, nonblocking corpse actor, settles it on ground and keeps it for the session. The living original is removed so corpses cannot recruit, receive orders, block shots, or award repeated kill rewards. Session persistence is not save-game persistence. Corpses settle to the floor even when a victim dies in the air.

## UI

White/navy panels, cyan readiness, warm hit feedback, a health block, a separate stamina bar, Q/F/R cards, dash availability and an open crosshair replace the debug-style full-width panels. Aimed targets have a compact health display. Detailed society information is on Tab, belief history on B, and overhead activities appear while using ALT orders. Mode and character screens use the same palette. This is an original interface inspired by hero-shooter clarity, not copied Overwatch assets.

## Source-kit delivery table

Values below are source base powers and cooldowns; native hit delivery applies the source belief calculation once. Area damage fades from full at center to half at the edge. Twin Fangs divides its original damage between two independently colliding bolts. Heal/buff/shield/summon are support effects, not damaging projectiles.

| Entity | Key | Source ability | Base power | Cooldown | Delivery |
|---|---|---|---:|---:|---|
| Bast, Queen of the Cats of Ulthar | Q | Claw of the Sphinx | 110 | 7s | 45-degree cone; 0.30s warning |
| Bast, Queen of the Cats of Ulthar | F | Pounce | 14 | 12s | Swept movement; original distance |
| Bast, Queen of the Cats of Ulthar | R | The Cats of Ulthar | 5 | 50s | Safe ground placement |
| Bokrug, the Great Water Lizard | Q | Doomfall Stomp | 120 | 8s | Self ground disk; 0.55s warning |
| Bokrug, the Great Water Lizard | F | Curse of Sarnath | 5 | 15s | Slow bolt; 22m/s, 40cm radius, 0.25s windup |
| Bokrug, the Great Water Lizard | R | Wrath of the Water-Lizard | 230 | 55s | Ground disk; 0.50s warning (R: 0.90s) |
| Cthulhu, the Dreaming God | Q | Tentacle Slam | 120 | 8s | Ground disk; 0.50s warning (R: 0.90s) |
| Cthulhu, the Dreaming God | F | Nightmare Veil | 4 | 15s | Sphere; 0.45s warning |
| Cthulhu, the Dreaming God | R | Call of the Deep | 4 | 45s | Safe ground placement |
| Dagon, the Deep Father | Q | Crushing Maw | 125 | 8s | 45-degree cone; 0.30s warning |
| Dagon, the Deep Father | F | Tidal Surge | 12 | 14s | Swept movement; original distance |
| Dagon, the Deep Father | R | Rise, Deep Ones | 4 | 55s | Safe ground placement |
| The Elder Mind, Protector of the Yith | Q | Temporal Lash | 105 | 8s | Lash; 28m/s, 45cm radius |
| The Elder Mind, Protector of the Yith | F | Between Seconds | 14 | 14s | Swept movement; original distance |
| The Elder Mind, Protector of the Yith | R | The Library Opens | 8 | 55s | Self buff |
| Ghatanothoa, Lord of the Volcano | Q | Petrifying Gaze | 1.25 | 9s | Control bolt; 22m/s, 40cm radius, 0.25s windup |
| Ghatanothoa, Lord of the Volcano | F | Obsidian Shards | 110 | 10s | Shard; 24m/s, 70cm radius |
| Ghatanothoa, Lord of the Volcano | R | Behold! | 190 | 55s | Ground disk; 0.50s warning (R: 0.90s) |
| Hastur, the King in Yellow | Q | Tattered Shriek | 100 | 8s | Wide cry; 18m/s, 100cm radius, 0.30s windup |
| Hastur, the King in Yellow | F | The Play Continues | 2 | 15s | Sphere; 0.45s warning |
| Hastur, the King in Yellow | R | Carcosa Manifest | 220 | 55s | Ground disk; 0.50s warning (R: 0.90s) |
| The Howling Eye, Heart of the Polyp Scourge | Q | Screaming Gale | 115 | 8s | Ground disk; 0.50s warning (R: 0.90s) |
| The Howling Eye, Heart of the Polyp Scourge | F | Storm Surge | 16 | 14s | Swept movement; original distance |
| The Howling Eye, Heart of the Polyp Scourge | R | Eye of the Maelstrom | 230 | 55s | Ground disk; 0.50s warning (R: 0.90s) |
| Hypnos, Lord of Sleep | Q | Dream Pierce | 100 | 7s | Bolt; 34m/s, 24cm radius |
| Hypnos, Lord of Sleep | F | Slumber | 2 | 16s | Control bolt; 22m/s, 40cm radius, 0.25s windup |
| Hypnos, Lord of Sleep | R | The Dreamlands Open | 190 | 55s | Ground disk; 0.50s warning (R: 0.90s) |
| The Mi-Go, Fungi from Yuggoth | Q | Nerve Needle | 95 | 7s | Needle; 48m/s, 14cm radius, 0.12s windup |
| The Mi-Go, Fungi from Yuggoth | F | Cylinder Stasis | 1.5 | 15s | Control bolt; 22m/s, 40cm radius, 0.25s windup |
| The Mi-Go, Fungi from Yuggoth | R | Yuggoth Harvest | 200 | 50s | Ground disk; 0.50s warning (R: 0.90s) |
| Mother Hydra, the Deep Mother | Q | Matriarch's Call | 2 | 10s | Safe ground placement |
| Mother Hydra, the Deep Mother | F | Brine Blessing | 150 | 16s | Aimed ally or self |
| Mother Hydra, the Deep Mother | R | Tide of the Brood | 5 | 55s | Safe ground placement |
| Nodens, Lord of the Great Abyss | Q | Abyssal Harpoon | 105 | 7s | Harpoon; 34m/s, 24cm radius |
| Nodens, Lord of the Great Abyss | F | Hunter's Wake | 14 | 12s | Swept movement; original distance |
| Nodens, Lord of the Great Abyss | R | The Abyss Opens | 12 | 50s | Sphere; 0.45s warning |
| Nug and Yeb, the Twin Blasphemies | Q | Twin Fangs | 110 | 8s | Two bolts at +/-2 degrees, half power each |
| Nug and Yeb, the Twin Blasphemies | F | Blasphemous Pact | 6 | 16s | Self buff |
| Nug and Yeb, the Twin Blasphemies | R | The Twins Unite | 220 | 50s | Ground disk; 0.50s warning (R: 0.90s) |
| Nyarlathotep, the Crawling Chaos | Q | Thousand Lashes | 100 | 7s | Lash; 30m/s, 55cm radius |
| Nyarlathotep, the Crawling Chaos | F | Borrowed Face | 6 | 16s | Self buff |
| Nyarlathotep, the Crawling Chaos | R | Unmasked Chaos | 3 | 55s | Sphere; 0.45s warning |
| Rhan-Tegoth, Terror of the Museum | Q | Crusher Claw | 135 | 8s | 28-degree cone; 0.30s warning |
| Rhan-Tegoth, Terror of the Museum | F | Chitin Wall | 150 | 16s | Self ward |
| Rhan-Tegoth, Terror of the Museum | R | The Museum Wakes | 2.5 | 50s | Sphere; 0.45s warning |
| Sghllor, the Colour Out of Space | Q | Blight Ray | 90 | 7s | Beam; 25cm radius, 0.35s warning |
| Sghllor, the Colour Out of Space | F | Withering Field | 6 | 15s | Slow bolt; 22m/s, 40cm radius, 0.25s windup |
| Sghllor, the Colour Out of Space | R | The Colour Falls | 240 | 55s | Ground disk; 0.50s warning (R: 0.90s) |
| The Shoggoths, the Shapeless Ones | Q | Protoplasm Slam | 130 | 8s | Self ground disk; 0.55s warning |
| The Shoggoths, the Shapeless Ones | F | Formless Rush | 12 | 13s | Swept movement; original distance |
| The Shoggoths, the Shapeless Ones | R | Tekeli-li! | 3 | 50s | Sphere; 0.45s warning |
| Shub-Niggurath, the Black Goat | Q | Brood Surge | 3 | 10s | Safe ground placement |
| Shub-Niggurath, the Black Goat | F | Fertile Rot | 5 | 14s | Slow bolt; 22m/s, 40cm radius, 0.25s windup |
| Shub-Niggurath, the Black Goat | R | The Woods Give Birth | 6 | 55s | Safe ground placement |
| Yig, Father of Serpents | Q | Serpent Strike | 100 | 7s | Bolt; 36m/s, 28cm radius |
| Yig, Father of Serpents | F | Coil | 10 | 13s | Swept movement; original distance |
| Yig, Father of Serpents | R | Curse of Yig | 180 | 50s | Ground disk; 0.50s warning (R: 0.90s) |
| Yog-Sothoth, the All-in-One | Q | Gate Lash | 110 | 8s | Lash; 28m/s, 45cm radius |
| Yog-Sothoth, the All-in-One | F | Between Spaces | 15 | 14s | Swept movement; original distance |
| Yog-Sothoth, the All-in-One | R | The Key and the Gate | 12 | 50s | Sphere; 0.45s warning |

## Original RMB profiles

Generic heavy swings use the selected entity's body radius plus its source reach, so the same source reach still produces a different physical envelope on each body. The table lists source multipliers: non-avatar base damage is 135 times this value; avatar base damage is 10 times this value. Beliefs and active buffs then apply. Only a confirmed contact applies damage/control.

| Entity | Heavy attack | Shape | Source reach | Multiplier | On-hit control |
|---|---|---|---:|---:|---|
| Bast, Queen of the Cats of Ulthar | Sphinx Swipe | 52-degree swept arm arc | 3m | 1.2 | none 0s |
| Bokrug, the Great Water Lizard | Lizard Rend | 52-degree swept arm arc | 3m | 1.3 | Stun 1.0s |
| Cthulhu, the Dreaming God | Wave of Domination | Source held wave / physical direct-stun projectile | 25m | 0.4 | Stun 2.5s |
| Dagon, the Deep Father | Father's Wrath | 52-degree swept arm arc | 3m | 1.4 | none 0s |
| The Elder Mind, Protector of the Yith | Yith Rebuke | 40cm control projectile | 22m | 0.9 | Slow 2.5s |
| Ghatanothoa, Lord of the Volcano | Volcanic Fist | 52-degree swept arm arc | 3m | 1.2 | Slow 2.0s |
| Hastur, the King in Yellow | The King's Whisper | 40cm control projectile | 20m | 0.8 | Fear 2.0s |
| The Howling Eye, Heart of the Polyp Scourge | Polyp Shriek | 65cm projectile | 22m | 1.0 | Slow 2.0s |
| Hypnos, Lord of Sleep | Nightmare Touch | 40cm control projectile | 20m | 0.7 | Stun 2.0s |
| The Mi-Go, Fungi from Yuggoth | Bio-Acid Jet | 65cm projectile | 20m | 1.0 | Slow 2.5s |
| Mother Hydra, the Deep Mother | Brine Spit | 65cm projectile | 20m | 0.8 | Slow 2.0s |
| Nodens, Lord of the Great Abyss | Abyssal Drag | 40cm pull projectile | 18m | 1.1 | Root 1.5s |
| Nug and Yeb, the Twin Blasphemies | Twin Rend | 52-degree swept arm arc | 3m | 1.3 | none 0s |
| Nyarlathotep, the Crawling Chaos | Crawling Strike | 52-degree swept arm arc | 3m | 1.4 | Fear 1.5s |
| Rhan-Tegoth, Terror of the Museum | Fossil Slam | 52-degree swept arm arc | 3m | 1.5 | Root 1.0s |
| Sghllor, the Colour Out of Space | Prismatic Beam | 65cm projectile | 24m | 1.1 | none 0s |
| The Shoggoths, the Shapeless Ones | Amorphous Crush | 52-degree swept arm arc | 3m | 1.5 | none 0s |
| Shub-Niggurath, the Black Goat | Brood Spit | 65cm projectile | 22m | 0.9 | Slow 2.0s |
| Yig, Father of Serpents | Father's Bite | 52-degree swept arm arc | 3m | 1.2 | Slow 2.0s |
| Yog-Sothoth, the All-in-One | Gate Maw | 40cm pull projectile | 20m | 1.0 | Slow 2.0s |

## Validation

Verified on 2026-10-17:

- Canonical library: all **18 suites passed**, including original Wave of Domination behavior and the optional native collision hooks.
- `Saved/Logs/SkillCombatVerified.log`: all **seven game suites passed** — Beliefs, CanonicalGameplay, GroundSpawn, LivingWorld, MinorCreatures, SkillCombat and Society.
- SkillCombat checks cover wall blocking, finite projectile travel, a sidestep miss, actual contact, jump clearance, persistent corpses, one hit per melee swing, dash cooldown/protection, stun interruption and attack cleanup on defeat.
- The 130-second society observation recorded six torture completions, six sacrifices, nine prayers, twelve conversions, two constructions and twenty-one destroyed city buildings. Three actual war kills had already happened in this world before its observation window. Strength ended at x1.50 / x1.72 / x1.48. This checks that the new combat does not stop the living-belief simulation.
- Editor `Menu.Select` passed with all twenty selectable characters. `Menu.SkillPreview` passed actual aimed LMB contact, delayed Q activation, Temporal Lash projectile contact, and physical throws that miss a raised structure and strike one in their path.
- Final screenshot review caught and repaired a Canvas runtime-font binding issue. `Saved/Screenshots/SkillCombatHUD.png` now shows crisp composite-font text, health/stamina, cooldown/readiness cards and compact controls. `CodexCharacterSelect.png` shows uniform character cards. `SkillCombatSwing.png` captures the temporary arm/slash.

The movement-art test uses a verified flat clearing and deterministic escort motion instead of relying on random wander direction at an unchecked procedural spawn. The society war assertion accepts actual kills earlier in the same live world, rather than requiring its already-defeated garrison to die again.

This is a first balance pass: the collision and event logic is implemented, while the entity vessel and articulated arm remain temporary art awaiting the owner's models. Navigation around dense structures and session-only world persistence retain the prototype limits documented in the society guide.

### Final targeted verification

`Saved/Logs/SkillCombatAccounting.log` passed the added civilian kill-credit check. After the final root-motion correction, `Saved/Logs/SkillCombatHandoff.log` reports **Menu.SkillCombat and Menu.SkillPreview both successful, with zero warnings/errors**. These rerun human dash/jump protection, contact, defeat cleanup and kill credit in PIE, plus actual roster melee, area damage, projectile contact and 3D throws. The Elder Mind's Between Seconds produced 1671.4cm of displacement including finishing momentum; its cast effect stayed at its actual caster location. The prior velocity-only AI dash failure was repaired, not accepted as a fixture exception.

The final editor is left on `L_MainMenu` with PIE stopped. Press Play to select a mode and entity.
