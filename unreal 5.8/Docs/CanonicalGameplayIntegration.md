# Original controls and character kits

World layout and faction AI have since expanded: see [Living Wilds revision](LivingWildsRevision.md) for the current 3.6 km terrain, caverns, temples, clustered cities, and rival plans.

Source of truth: repository `game-core`, `assets/characters/*/character.def`,
`assets/characters/KITS.md`, `src/input/InputManager.h`, `src/combat/KitCaster.cpp`,
`src/characters/abilities/WaveOfDomination.*`, and `src/commands/CommandSystem.cpp`.

The game reads those original definitions directly; no generated replacement kit values.

| Input | Function |
| --- | --- |
| WASD / arrows | Camera-relative movement |
| Mouse | Camera look |
| Shift | Sprint; original stamina drain / regeneration / exhaustion |
| Space | Jump |
| V | First / third person |
| LMB | Ground smash with the core combo tracker |
| RMB | Character heavy; Cthulhu holds, swings, launches with LMB, drops on release |
| Q / F / R | Original kit slots below |
| E | Altar / captive interaction within 3 m |
| Hold ALT | Release cursor and suspend look |
| ALT + LMB | Contextual cult command buttons; no simultaneous attack |
| Release ALT | Close buttons and capture camera again |
| Tab | Character kit / stats display |
| Esc | Menu (Unreal consumes Esc to stop PIE) |

| Entity | Q | F | R | RMB |
| --- | --- | --- | --- | --- |
| Bast, Queen of the Cats of Ulthar | Claw of the Sphinx (aoe_damage, 7s) | Pounce (dash, 12s) | The Cats of Ulthar (summon, 50s) | Sphinx Swipe |
| Bokrug, the Great Water Lizard | Doomfall Stomp (aoe_damage, 8s) | Curse of Sarnath (debuff, 15s) | Wrath of the Water-Lizard (aoe_damage, 55s) | Lizard Rend |
| Cthulhu, the Dreaming God | Tentacle Slam (aoe_damage, 8s) | Nightmare Veil (fear_aura, 15s) | Call of the Deep (summon, 45s) | Wave of Domination |
| Dagon, the Deep Father | Crushing Maw (aoe_damage, 8s) | Tidal Surge (dash, 14s) | Rise, Deep Ones (summon, 55s) | Father's Wrath |
| The Elder Mind, Protector of the Yith | Temporal Lash (projectile, 8s) | Between Seconds (dash, 14s) | The Library Opens (buff, 55s) | Yith Rebuke |
| Ghatanothoa, Lord of the Volcano | Petrifying Gaze (stun, 9s) | Obsidian Shards (projectile, 10s) | Behold! (aoe_damage, 55s) | Volcanic Fist |
| Hastur, the King in Yellow | Tattered Shriek (projectile, 8s) | The Play Continues (fear_aura, 15s) | Carcosa Manifest (aoe_damage, 55s) | The King's Whisper |
| The Howling Eye, Heart of the Polyp Scourge | Screaming Gale (aoe_damage, 8s) | Storm Surge (dash, 14s) | Eye of the Maelstrom (aoe_damage, 55s) | Polyp Shriek |
| Hypnos, Lord of Sleep | Dream Pierce (projectile, 7s) | Slumber (stun, 16s) | The Dreamlands Open (aoe_damage, 55s) | Nightmare Touch |
| The Mi-Go, Fungi from Yuggoth | Nerve Needle (projectile, 7s) | Cylinder Stasis (stun, 15s) | Yuggoth Harvest (aoe_damage, 50s) | Bio-Acid Jet |
| Mother Hydra, the Deep Mother | Matriarch's Call (summon, 10s) | Brine Blessing (heal, 16s) | Tide of the Brood (summon, 55s) | Brine Spit |
| Nodens, Lord of the Great Abyss | Abyssal Harpoon (projectile, 7s) | Hunter's Wake (dash, 12s) | The Abyss Opens (pull, 50s) | Abyssal Drag |
| Nug and Yeb, the Twin Blasphemies | Twin Fangs (projectile, 8s) | Blasphemous Pact (buff, 16s) | The Twins Unite (aoe_damage, 50s) | Twin Rend |
| Nyarlathotep, the Crawling Chaos | Thousand Lashes (projectile, 7s) | Borrowed Face (buff, 16s) | Unmasked Chaos (fear_aura, 55s) | Crawling Strike |
| Rhan-Tegoth, Terror of the Museum | Crusher Claw (aoe_damage, 8s) | Chitin Wall (shield, 16s) | The Museum Wakes (fear_aura, 50s) | Fossil Slam |
| Sghllor, the Colour Out of Space | Blight Ray (projectile, 7s) | Withering Field (debuff, 15s) | The Colour Falls (aoe_damage, 55s) | Prismatic Beam |
| The Shoggoths, the Shapeless Ones | Protoplasm Slam (aoe_damage, 8s) | Formless Rush (dash, 13s) | Tekeli-li! (fear_aura, 50s) | Amorphous Crush |
| Shub-Niggurath, the Black Goat | Brood Surge (summon, 10s) | Fertile Rot (debuff, 14s) | The Woods Give Birth (summon, 55s) | Brood Spit |
| Yig, Father of Serpents | Serpent Strike (projectile, 7s) | Coil (dash, 13s) | Curse of Yig (aoe_damage, 50s) | Father's Bite |
| Yog-Sothoth, the All-in-One | Gate Lash (projectile, 8s) | Between Spaces (dash, 14s) | The Key and the Gate (pull, 50s) | Gate Maw |

Passives retain the original hook descriptions. Hooks with no original engine behavior are not presented as completed mechanics.

## Living-world binding

Three world-scoped core faith simulations own belief/exertion, loyalty, obedience, autonomous action selection, directive operations, and wars. Event buses die with their worlds so old level subscriptions cannot outlive their cults.

Player creed starts with Dreams + Conversion, matching the original beta. Rival creeds currently use War + Onslaught and Chaos + Magic. Cultists choose among the original sixteen ambient actions on staggered timers; native actors mirror movement/activity while core events handle effects and loyalty. Obeyed / partial / refusal / insurrection use the original CommandSystem calculation.

Randomized replenishment runs every 20 game seconds, with 100 neutral civilians and 36 minor creatures as replenishment targets. Session RNG replaces the prototype fixed placement seed. New settlements form every 120–180 seconds at separated, flat terrain sites (at most twelve living settlements). Destroyed sites remain ruins; new towns use new positions. These settlement timings and caps are the new world-growth binding, not an original ability rebalance.

Nyarlathotep and Yog-Sothoth have separated starting territories, native vessels, original kits, conversion attempts, and rival cults. ALT commands can direct war and raids; rival cultists fight hostile factions. This is a local free-roam first pass, without strategic campaign persistence, full pathfinding, save/load, multiplayer synchronization, or completed 5v5 rules.

Summoned creatures retain original species names and 120 HP from the original driver. Their visible bodies remain temporary proxies until corresponding art is supplied. The incomplete Risen Dead import is excluded from live spawning and preserved on disk. Existing saved Risen actors fall back to ghouls.

## Build / verification

`Tools/BuildCanonicalCore.ps1` builds the current core inside the active project and installs `lib/Win64/cultulhu.lib`. Nine focused original suites passed, including the wave-27 kit effect-execution suite. The general root CMake glob also includes standalone POSIX fuzz executables; the engine-specific CMake target excludes those entrypoints without editing the original game logic.

Development Editor build succeeded. `CultUlhu.CanonicalGameplay`, `CultUlhu.GroundSpawn`, and `CultUlhu.MinorCreatures` all passed together in `Saved/Logs/CultUlhuFinalGameplay.log`. The tests cover all 20 parsed original kits, HP, camera-picked building damage, direct NPC picking, self-centred fear, stamina and cooldown gates, exactly four Deep One summons, separated rivals, timed new settlements, jumping/landing, boundary recovery, imported enemy parts, forward visual alignment, and animated movement. Mother Hydra can cast Brine Blessing while rooted, but cannot cast while stunned. Native control projections refresh separately for each caster so expired roots do not persist with other effects. The final incremental build also restores idle stamina regeneration to rival entities.

The integration test exposed and fixed incomplete summon groups, default Pawn visibility-trace ignores, and rubble colliders trapping giant capsules. Summon placement is now validated before spending stamina. Minor capsules overlap the giant; ruined rubble and scorch are visual geometry.

Existing engine EditorToolset Python startup errors occur in headless `-game` runs and do not fail these tests. Visual menu and command checks are recorded separately.

## Native menu verification

`CultUlhu.Menu.Select` and `CultUlhu.Menu.Enter` passed in the visible editor: mode menu, all twenty character cards, selected-kit world entry, and ALT cursor release. The tests start their own PIE sessions because Unreal ends existing sessions before editor automation. A real pre-existing non-focusable menu-root error was fixed by setting the menu widgets focusable. Native screenshots record character selection and the in-world command menu.

Desktop input could not be verified in this run: the Windows helper twice returned `failed to activate captured window`. Native Unreal automation was used instead; physical ALT/LMB handling remains a user playtest check.

Small runtime buildings and the temple use ordinary LOD rendering to avoid their imported materials falling back through missing Nanite usage flags. Original altar interaction gives ritual guidance; the prototype free heal was removed.

The final visible-editor menu runs passed again with zero automation errors or warnings in `Saved/Logs/CultUlhuReadyEditor.log`. Character-selection text and both screenshots were visually inspected. Physical mouse/keyboard input still needs the user playtest described above. Necromancy and beast-capture buttons explicitly remain unavailable. Population and settlement placement vary; runtime hills, ridges, vales, and three walk-through caverns are now implemented, with further terrain/art refinement still needed. Proxy buildings, summoned bodies, and ability effects need the intended art pass.
