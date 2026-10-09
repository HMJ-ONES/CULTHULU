# CULT-ULHU — Playable Beta (free roam + Dreams + driver)

Cosmic-horror ARPG (first/third person) core: choose your cosmic horror
entity, gather a cult following, destroy cities, wage war against other
cults, and find relics / cursed artifacts. Two 5v5 modes: capture-the-point
and MOBA defense of your Great Old One — plus a **free roam** mode with a
playable headless driver.

This is an **engine-agnostic C++17 core** — pure standard C++, no
Unreal/Unity headers — so it can be bound to Unreal later (see Roadmap).

## Build & test

Requires: g++ (C++17), cmake >= 3.16, make.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cd build && ctest --output-on-failure
```

Tests: 357 checks (87 v0.1 + 68 wave 2 + 70 wave 3 + 81 beta + 51 wave 4), 0 failures.

## Playable driver

`build/cultulhu_play` is an interactive console REPL — the game is playable
headless right now:

```sh
./build/cultulhu_play
```

Commands: `move n/s/e/w`, `camera fp|tp`, `look`, `spawn <cultist|civilian|`
`monstrosity|sorcerer>`, `belief <name> [replace <old>]`, `beliefs`,
`rest <i>`, `command <raid|war|convert|sacrifice|defend|relic>`,
`attack`, `cast <fireball|fear>`, `tick <n>`, `save <file>`,
`load <file>`, `myip`, `discover`, `host`, `join`, `ready`, `players`,
`startgame`, `chat`, `netent`, `leave`,
`build <wall|barracks|watchtower|trap|portal|altar>`,
`menu [cultist|location|enemy|altar]`, `combo`, `chars`,
`addchar <folder>`, `validate <id>`, `kda`, `interact`, `jump`,
`sprint <on|off>`, `rmb <press|move|launch|release|stun>`, `status`,
`help`, `quit`.

The beta starts in free roam at night with the Dreams and Conversion beliefs
adopted, 3 cultists, and a sorcerer attendant.

## Plugging in your models

The core never touches raw art files — it refers to **model slots**
(`src/assets/AssetManager.h`): `PlayerAvatar`, `Cultist`, `Monstrosity`,
`Civilian`, `Adventurer`, `Creature`, `Sorcerer`, `Mimic`, `GreatOldOne`.

1. Drop your rigged FBX files into `assets/models/` (create the folder; it
   ships with a `.gitkeep`).
2. Name them however you like, e.g. `assets/models/cultist_rigged.fbx`.
3. Bind them in code (or later in the engine binding):
   ```cpp
   assets.bindModel(ModelSlot::Cultist, "assets/models/cultist_rigged.fbx");
   assets.bindRig(ModelSlot::Cultist,
                  {"mixamo_x_bot", 65, "MixamoToUnreal"});
   ```
4. The animation state machine (`src/animation/`) uses **Mixamo-standard
   clip names** (`Idle`, `Walking`, `Running`, `SwordAndShieldSlash`,
   `Spellcast`, `Stunned`, `Death`, `Channel`) so downloaded Mixamo packs
   bind with zero renaming — see
   `AnimationStateMachine::mixamoClipName()`.

Everything compiles and runs with **no model files present**: unbound slots
simply report procedural fallback (`<procedural:Walk>` etc.), and the game
logic is unaffected.

## Layout

- `src/core/` — `Vec3`, `RNG` (seeded), `GameClock` (game-time seconds),
  `EventBus` (sync pub/sub), `Events.h` (all gameplay event types), `Logger`
- `src/entities/` — `Entity` base (id, type, faction, position, HP,
  one-hit-kill tracking); `Units.h` (`GreatOldOne`, **`EldritchAvatar`** —
  the player's entity, linked to the `PowerSystem`, `Cultist` with states
  Loyal/Infringer/Lunatic/Imprisoned/Converted, `Civilian`, `Adventurer`,
  `Creature`, `Monstrosity` with feral flag, `Mimic`, `Sorcerer`);
  `Structures.h` (`Building` with Intact/Damaged/Destroyed + timed
  auto-rebuild, `Relic` power amplifier, `Artifact` cursed trap)
- `src/beliefs/` — **12-belief** enum + `BeliefSystem`: max 3 active,
  adoption timers, punishment mechanics, per-belief `onEvent()` power rules,
  `tick()` time-based effects (Fear decay/generation, Onslaught idle decay),
  mass-conversion cooldown, belief modifier hooks
- `src/dreams/` — `DreamSystem`: the Dreams belief. Resting cultists
  generate power; dream-whispers convert distant civilians (routed through
  `ConversionPerformed` so Conversion synergizes); nightmares wake cultists
  Lunatic (×3 with Chaos active)
- `src/power/` — `PowerSystem`: Cthulhu's power 0–1000, clamped
- `src/cult/` — `CultManager`: follower roster, conversion campaigns,
  insurrection risk 0–100, revolt events at 80+
- `src/modes/` — `GameMode` base; `CapturePointMode` (5v5 Onslaught: single
  point, hold ticks, kill points, first to 400); `MobaDefense` (lanes, minion waves,
  base structures, Great Old One survival); **`FreeRoamMode`** (open map,
  no objectives, civilian/creature spawn points, relic sites, day/night
  clock — never ends)
- `src/camera/` — `CameraSystem`: switchable FirstPerson/ThirdPerson at
  runtime (`switchCamera()`); FP eye + yaw/pitch + crosshair; TP
  over-shoulder boom follow; pure logic, no rendering
- `src/assets/` — `AssetManager`: named model slots, `bindModel()` /
  `bindRig()` (`RigDefinition`: skeleton name, bone count, retarget
  profile); `assets/models/` directory convention
- `src/animation/` — `AnimationClip` (name, duration, loop, optional source
  path); `AnimationStateMachine` (Idle/Walk/Run/Attack/Cast/Stunned/Death/
  Channel, transitions, Death terminal, one-shots auto-return to Idle,
  Mixamo naming hooks, procedural fallback); `suggestAnimState()` maps
  entity motion to states
- `src/combat/` — damage calc with belief modifiers, CC durations, kill
  events; `CrowdControl.h` (`CCType`: Stun/Slow/Root/Fear, `ActiveEffects`
  per-entity timed tracker, Slow stacks toward a 0.2 floor); `Attacks.h`
  (`DamageType`, `Attack` struct, `resolveAttack`/`strikeMelee`/`castSpell`)
- `src/commands/` — `CommandSystem`: eldritch directives (`GoToWar`,
  `RaidCity`, `ConvertCampaign`, `MassSacrifice`, `Defend`, `GatherRelic`,
  `AssassinateProphet`, `BlightLand`, `GrandSummoning`, `OneiricHarvest`,
  `RebuildSanctum`)
  with a computed **obedience chance** → outcomes Obeyed / PartiallyObeyed
  / Refused / SparksInsurrection; refusals raise insurrection risk; all
  published as events
- `src/rituals/` — interruptible `Ritual` base; `SacrificeRitual` (30s),
  `NecromancyRitual` (45s)
- `src/save/` — `SaveSystem`/`GameState`: human-readable text save/load
- `src/breeding/` — `BreedingSystem`: species compatibility matrix,
  feral-birth chance, `MonstrosityBred` / `FeralRampage` events
- `src/traps/` — `TrapSystem`: single-use traps capturing civilians /
  adventurers, publishing `TrapSprung`
- `src/ai/` — steering behaviors (follow, flee, rampage targeting) plus
  `AmbientBehavior.h`: `AmbientDirector` — every cultist on a randomized
  timer picks one of 16 ambient actions (Pray, Patrol, Gather, Preach,
  Brawl, Desecrate + wave-9c/15 additions: omen-reading, sparring,
  tending wounded, sigil graffiti, chanting, dream-sharing, effigy
  mending, whisper campaigns, blood rites, wilds hunts), weighted by
  beliefs, morale, and time of day
- `src/war/` — `WarSystem`: war declarations vs deity factions, kill
  tracking, belief sync for single/multi-deity power scaling
- `src/relics/` — `RelicSystem`: relic power amplifiers, single-use cursed
  artifact traps
- `src/city/` — `CitySystem`: neutral cities with districts, permanent
  building destruction, ruin tracking, population casualties
- `src/chaos/` — `LunaticSystem`: Chaos-active cultists may snap Lunatic
  and misbehave against other active beliefs; aligning them raises risk
- `src/driver/` — the playable REPL (`cultulhu_play` binary)
- `tests/` — `tests_main.cpp` (v0.1) + `tests_wave2.cpp` + `tests_wave3.cpp`
  + `tests_beta.cpp` (81 checks: Dreams, camera, animation, free roam,
  assets, CC, obedience, ambient, night effects), all via ctest

## Creative-liberty tuning decisions (numbers the doc left vague)

- Power starts at **100** / max **1000**.
- Belief adoption takes **120s** of game time; punishing infringers **halves**
  remaining adoption time and adds **+10** insurrection risk.
- Torture: +8 power per civilian/creature tortured, +5 per captured cultist
  tortured, +0 for own cultists, **−12** when an own cultist is one-hit killed.
- Fear: raids add +20 fear level and +12 power × destruction (0–1); fear
  decays **1.5/s** out of combat (power −1/s while decaying); captured
  creatures / bred monstrosities in the army add +2/s passive fear generation
  each while in combat; losing a cultist −6 fear / −6 power; defeat wipes fear
  and −60 power. **Night raids add ×1.25 fear.**
- Breeding: +15 power per successful breeding; −10 per cultist killed by feral
  monstrosities.
- Chaos: **2×** adoption speed; explosions +6 power; imprisoned cultists −4;
  punishing cultists to align them publishes extra insurrection risk.
- Sacrifice: completed ritual +25 power; interrupted −15.
- Conversion: +4 power per soul; instant mass-convert of **3** with **200s**
  cooldown; campaign efficiency ×1.5; cultists ×1.5 easier for enemy deities
  to steal.
- War: +10 power per enemy cultist slain at war with one deity, ×0.4 rate
  against multiple; −5 / −20 when base buildings are damaged / destroyed.
- Reconstruction: healing ×1.5, all other spells ×0.75; damaged buildings
  auto-rebuild over **60s**.
- Trickery: mimic kills +8 power, slain mimics −8, sprung traps +6 (+5 fear
  if Fear active).
- Magic: sorcerer damage ×1.3, incoming CC duration ×0.6, necromancy ritual
  +10 power, cultist magic learning time ×1.5.
- Onslaught: +3 power per civilian slain; melee damage ×1.25; if no civilian
  slain for **1 in-game hour**, power decays −2/s.
- **Dreams (12th belief)**: +0.5 power/s per resting cultist (**×1.5 at
  night**); dream-whisper conversion chance **0.002/s** per resting cultist
  (+2 power per whisper when Dreams active, +4 more if Conversion active);
  nightmare chance **0.001/s** per resting cultist (**×3 with Chaos**),
  waking the cultist Lunatic.
- Insurrection: revolt at risk ≥ 80; revolt burns risk back to 30.
- **Crowd control**: Stun (no action/move), Slow (move ×slowFactor, stacks
  toward 0.2 floor), Root (no move), Fear (flees; AI interprets). Durations
  scale with belief modifiers (Magic ×0.6).
- **Obedience**: 0.5 + 0.4×avg loyalty − 0.5×risk − Chaos lunatic penalty −
  distance penalty + belief bonuses (War→GoToWar +0.15,
  Conversion→ConvertCampaign +0.15, Sacrifice→MassSacrifice +0.1), clamped
  [0.05, 0.95]. Roll < chance → Obeyed; < chance+0.15 → PartiallyObeyed;
  else risk > 0.7 → SparksInsurrection else Refused. Refused +5 risk,
  SparksInsurrection +15 risk.
- **Ambient behavior**: every 30s each eligible cultist acts; weights shift
  with beliefs (Preach +Conversion, Brawl +Chaos/low morale, Desecrate
  +Torture/Chaos, Pray +Sacrifice/Magic/night). Pray +1 power; Preach 25% ×
  efficiency to convert; Brawl 30% injury + +2 risk; Desecrate +fear/+power
  per beliefs.
- **Camera**: FP eye at avatar + 1.7m with yaw/pitch look; TP boom 6m behind
  with 1.2m over-shoulder offset; crosshair in FP only.
- **Animation**: one-shot states (Attack 0.8s, Cast 1.2s, Channel 3s,
  Death 2.0s) auto-return to Idle when unbound; locomotion/idle loop.
- **Free roam**: 10-minute day (22:00–06:00 night); spawns every 20s up to
  30 civilians / 12 creatures; map bounds ±500m.
- Onslaught (5v5 capture): one central point; the holding team banks 1 pt/s
  while uncontested (any enemy on the point pauses ticking); player kills
  score 5 pts; first to 400 wins, 600s limit, tied clock -> sudden-death
  overtime (first score wins, 120s cap).
- MOBA (5v5): waves every 30s (3 melee + 1 ranged; every 3rd wave +
  siege), 2 towers per team per lane (250 HP, 18 dps). Great Old One:
  1500 HP, shielded by backdoor protection until one full lane of towers
  falls; cracking a lane empowers that lane's waves (x1.75 super minions).
  Waves scale +2% per 30s. Players clear waves fast (2x damage vs minions).
  20-minute limit, then higher GOO-HP fraction wins (then most towers).
  Join with `match join [team]`; `move` walks your champion, combat is
  automatic; `camera fp|tp|switch` works mid-match.
- Bots in every mode: `match capture|moba` starts a 5v5 bot-vs-bot game
  (just watch, or `match join [team]` for players-vs-bots); free-roam has
  `spawn bot [n]` (rival hunters: wander, engage avatar/creatures, flee
  when hurt — player-vs-bot and bot-vs-bot). Civilians wander and flee
  threats; cultists do ambient actions (pray, patrol, preach, brawls...).
- Conversion campaigns: ~300s game time per soul at 1.0 efficiency.
- Breeding: feral-birth chance **35%**.
- City destruction: ruin = destroyed-HP fraction per district (permanent);
  each destroyed building kills **50%** of its per-building population share.
- Sacrifice deny-death: a Converted individual may take a dying cultist's
  place — volunteer dies, cultist revives at **25% HP**, **+20 power**.
- Chaos lunatics: **0.0005** new lunatics per cultist per game-second;
  misbehave every **60s**; aligning one adds **+10** risk.

## Wave 4: belief exertion & interactions

Every belief now carries an **exertion (fervor) meter 0–100** (baseline 10,
decays toward baseline at 0.05/s when neglected — wave 10: was 0.5/s, which
made the synergy/tension gates unreachable). Active beliefs accumulate
at full rate; inactive beliefs still track at half rate, so swapping creeds
mid-game has momentum. Exertion is fed by a data table
(`src/exertion/ActionExertionTable.h`) — every game action feeds it:

| Action | Exertion feed |
|---|---|
| Torture civilians/creatures | Torture +8, Fear +3 |
| Torture captured enemies | Torture +5, Fear +2 |
| Raid / raze district | Fear +10 (raid), +10 (razed), +6 (building destroyed) |
| Successful breeding | Breeding +8 |
| Feral rampage | Breeding +4, Fear +5 |
| Explosion | Chaos +6 |
| Punish infringers / brawl / desecrate / lunatic acts | Chaos +4/+3/+2/+3 |
| Completed sacrifice ritual | Sacrifice +20 |
| Interrupted sacrifice ritual | Sacrifice −5 |
| Conversion (soul) | Conversion +6 |
| Mass conversion (200s cd) | Conversion +10 |
| Sermon / dream-whisper | Conversion +2/+1 |
| Enemy cultist slain in war | War +6 |
| Building auto-rebuilt / heal | Reconstruction +8/+2 |
| Mimic kill / sprung trap / cursed artifact | Trickery +8/+6/+4 |
| Sorcerer spell cast / necromancy | Magic +3/+10 |
| Civilian slain / melee attack | Onslaught +3/+1 |
| Cultist rests / prays / dream-whisper / nightmare | Dreams +2/+1/+8/+3 (nightmare also Chaos +2) |
| Directive obeyed (Raid→Fear, Convert→Conversion, Sacrifice→Sacrifice, War→War, Defend→Reconstruction, Relic→Magic) | aligned belief +8 (partial +4) |
| Directive refused / sparks insurrection | Chaos +4 / +8 |

### Interaction matrix (12×12)

When two beliefs both exceed **25 exertion** they interact (wave 10: was 50,
unreachable):

**Synergies** (amplify each other):
| Pair | Name | Effect |
|---|---|---|
| Fear × Torture | Terror | power from torture/raid actions ×1.25 |
| Dreams × Chaos | Nightmare Surge | lunatic-wake chance ×2 |
| War × Onslaught | Blood Frenzy | combat power +0.15 |
| Sacrifice × Magic | Dark Rites | necromancy power ×1.5 |
| Conversion × Trickery | Infiltration | conversion chance +0.10 |
| Breeding × Fear | Dread Broods | +0.8 fear/s passive |
| War × Fear | Shock and Awe | raid power ×1.25 (stacks additively with Terror) |
| Sacrifice × Dreams | Martyrs' Visions | dream-whisper conversion ×1.5 |
| Magic × Dreams | Oneiromancy | ritual-caster conversion +0.05 |
| Torture × Chaos | Cruelty Unbound | torture power ×1.25 (stacks additively) |

**Conflicts** (each suppresses the other's exertion gains by half while both
> 25; both > 35 fires a `BeliefTension` event + **+1.5** insurrection risk,
60s cooldown per pair):
| Pair | Name | Extra effect |
|---|---|---|
| Reconstruction × Onslaught | Ashes and Scaffolds | — |
| Chaos × Conversion | Sabotage | conversion chance −0.10 |
| Chaos × Sacrifice | Desecrated Rites | sacrifice power ×0.75 |
| Reconstruction × War | Builders vs Warriors | — |
| Torture × Sacrifice | Waste Not | — |

### Derived stats (recomputed every tick)

- **Combat power** = `1.0 + 0.004×(War+Onslaught) + 0.002×Magic + 0.15 (Blood Frenzy)` — multiplies all damage your faction deals (wired through `strikeMelee`/`castSpell` via the driver's `combatPowerMult`).
- **Loyalty drift**/s = `0.01×(Sacrifice+Dreams+Conversion)/100 − 0.02×Chaos/100 − 0.015×risk` — applied to every living non-Converted cultist's devotion (clamped 0–100).
- **Conversion chance** = `0.35 + 0.004×Conversion + 0.002×(Trickery+Magic) + 0.10 (Infiltration) − 0.10 (Sabotage)`, clamped **[0.05, 0.95]** — used by player-directed conversion actions/commands and by the new **ambient sorcerer conversion rituals** (`src/ai/RitualCaster`: sorcerers with ≥25 mana attempt to convert civilians within 200m every 45s).

The `ExertionSystem` owns the unified power pipeline (event → existing
belief power rules → synergy multipliers → `PowerSystem`), so the driver no
longer duplicates it. `status` in the REPL now prints exertion levels and
derived stats.

## Wave 5: animation pipeline

The core's animation system (`src/animation/`) is engine-agnostic: it produces
**poses** (bone name → position + euler-degree rotation) that the Unreal
binding later retargets onto real skeletons. Everything below works headless,
so the game is fully playable with zero art assets.

### Data model
- `BoneTrack`: one bone's keyframes (`Keyframe{time, pos, rotEuler}` in
  degrees), sampled with linear interpolation and end clamping.
- `AnimationClip`: name, duration, loop flag, source path, plus
  `tracks` (bone name → `BoneTrack`). `sampleAt(t)` returns a full `Pose`,
  wrapping `t` for looping clips and clamping for one-shots.
- `AnimationStateMachine`: binds one clip per `AnimationState`
  (Idle/Walk/Run/Attack/Cast/Stunned/Death/Channel). `requestState(s,
  blendSeconds=0.25)` cross-fades between clips; `sampleBlendedPose()` lerps
  the old and new poses with a smoothstepped blend factor while `update(dt)`
  keeps both clips advancing. Death is terminal, Stunned interrupts anything
  but Death, and Attack/Cast/Channel auto-return to Idle when their clip
  finishes. States with no clip bound use the runtime procedural fallback, so
  unbound states never crash the game.

### Procedural generators
`ProceduralClips.h` synthesizes sinusoidal clips for the generic humanoid rig
(`humanoidBones()`: hips, spine, head, upperArmL/R, lowerArmL/R, upperLegL/R,
lowerLegL/R):
- `makeWalk()` — 1.0 s loop: legs swing opposite phase, arms counter-swing,
  hips bob twice per cycle.
- `makeRun()` — 0.6 s loop: wider swing, bent elbows, forward lean.
- `makeIdle()` — 2.0 s loop: breathing, arm sway, slow head look.
- `makeAttackSwing()` — 0.8 s one-shot: overhead wind-up → strike → recover.
- `makeDeath()` — 1.2 s one-shot: stagger, topple sideways, crumple.
`bindProceduralFallbacks(machine)` fills any unbound state (Idle/Walk/Run/
Attack/Death) without overwriting hand-bound clips.

### FBX import hook
`FbxClipImporter` is an abstract interface — the core never links an FBX SDK.
A future engine-side/tools implementation should: parse bone curves from the
FBX, map node names through `RigDefinition` (exact match, then the
retarget-profile rename table), resample to keyframes in euler degrees, and
persist each clip via `ClipSerializer` (see the 6-step TODO block in
`src/animation/FbxClipImporter.h`).

### `assets/animations/` layout
Imported or hand-tuned clips live here as `.canim` text files, one clip per
file, in a readable line format:
```
CLIP "Walking" 1.000000000 1
TRACK hips 13
KEY 0.000000000 0.000000000 1.000000000 0.000000000 0.000000000 0.000000000 0.000000000
...
```
`ClipSerializer::save/load` round-trips them exactly; the game loads `.canim`
files at runtime with no SDK dependency. Procedural clips are generated in
code and never need files, but can be dumped here for inspection.

### Mixamo naming
`AnimationStateMachine::mixamoClipName(state)` maps each state to its standard
Mixamo pack name (Walk → `"Walking"`, Run → `"Running"`, Attack →
`"SwordAndShieldSlash"`, Cast → `"Spellcast"`, …), so Mixamo packs downloaded
from mixamo.com bind with zero renaming: import each FBX through the hook
above, name the clip with `mixamoClipName(state)`, save to
`assets/animations/`, and `bindClip` it.

## Directive executor

Issuing a directive (`command <raid|war|convert|sacrifice|defend|relic>`) only
decides obedience. When the cult obeys, the **DirectiveExecutor**
(`src/commands/DirectiveExecutor.h`) spawns a live operation that plays the
directive out over game time. It subscribes to `DirectiveResolved` and parses
the `"DirectiveName/OutcomeName"` tag; `Refused` and `SparksInsurrection`
spawn nothing. At most 4 operations run concurrently (extras are refused with
a log line).

Operations and their event flow (all through the EventBus, so the
exertion/power pipeline reacts automatically — there is no parallel power
path; `PartiallyObeyed` halves magnitudes and durations):

| Directive | Operation | Ticks | Events per tick | Completion |
|---|---|---|---|---|
| GoToWar | WarOperation | 8 × 5s | `EnemyCultistSlain` (1–3, enemy faction) | `DirectiveCompleted` |
| RaidCity | RaidOperation | 10 × 5s | `RaidPerformed` (destruction 0.1–0.3) | `DirectiveCompleted` |
| ConvertCampaign | ConvertOperation | 6 × 5s | `ConversionPerformed` (1–2 souls) | `DirectiveCompleted` |
| MassSacrifice | SacrificeOperation | 6 × 5s (30s ritual) | 5%/tick interruption | `SacrificeCompleted`, or `SacrificeInterrupted` |
| Defend | DefendOperation | 6 × 10s (60s) | progress only | `DirectiveCompleted` |
| GatherRelic | RelicOperation | 6 × 5s (travel) | progress only | 60% → `ArtifactTriggered` (tag `"relic"`), else nothing |

Every tick publishes `DirectiveProgress` (tag = directive name, amount =
0..1); natural completion publishes `DirectiveCompleted`. The Defend
operation emits no gameplay buff itself — `DirectiveExecutor::defenseActive()`
returns true while any Defend op is live, and the game layer applies whatever
buff it wants (e.g. halve incoming damage) for that window.

Note: `CommandSystem` still applies its instant effects on Obeyed (e.g. an
immediate `RaidPerformed(0.4)` for raids, `startConversionCampaign` for
converts) alongside the new follow-through operation — initial strike plus
sustained operation.

### Driver save/load

- `save <file>` — snapshots clock time, power, active beliefs, insurrection
  risk, and all entities (avatar + cultists + world entities) via SaveSystem.
- `load <file>` — restores them: clock/power/beliefs/risk are set directly
  (beliefs restore wholesale, no adoption timer) and the entity list is
  respawned.

Beta limitations: entity AI state is not restored — rest/anim state, cultist
devotion, sorcerer mana, relic amplifiers, monstrosity species/feral flags,
mimic disguise, building rebuild progress, and ambient/ritual timers all
reset to defaults; respawned entities get fresh ids.

## Wave 6: Radmin VPN multiplayer (player-hosted, no dedicated server)

No AWS, no recurring server cost: one player hosts the match on their own
PC, everyone else joins over the Radmin VPN virtual LAN (26.x.x.x).

### Playing over Radmin VPN — step by step

1. Everyone installs **Radmin VPN** (free) and joins the same Radmin
   network (one player creates it and shares the network name).
2. The host starts the game and types:
   `myip` — confirms the Radmin adapter is detected (its 26.x.x.x address
   is highlighted; friends will join this IP).
3. The host types `host 47778` (any port works). A lobby opens and a
   presence beacon starts broadcasting on the Radmin subnet.
4. Friends type `discover` — the host's game appears — or connect
   directly: `join 26.x.x.x 47778 <name>`.
5. Everyone types `ready`. The host watches with `players`, then types
   `startgame` (or `startgame force`).
6. Play. Clients steer with `move`/`attack` (sent at 30 Hz); the host's PC
   simulates the world and broadcasts snapshots at 20 Hz. `netent` shows
   the entities your client is tracking. `chat <msg>` talks to the lobby.

Driver command reference: `myip`, `discover [secs]`, `host <port> [name]`,
`join <ip> <port> <name>`, `ready`, `players`, `startgame [force]`,
`chat <msg>`, `netent`, `leave`.

### How it works

- `src/net/Socket.h` — thin POSIX `UdpSocket`/`TcpSocket`/`TcpListener`
  wrappers (non-blocking). Windows port is a marked TODO (Winsock2).
- `src/net/RadminNet.h` — enumerates IPv4 adapters, detects the Radmin
  adapter by its 26.0.0.0/8 address, computes the subnet broadcast
  address. Falls back to LAN, then loopback, with a clear log message.
- `src/net/Discovery.h` — host UDP beacons every 2 s
  (`CULTHULU|1|<host>|<mode>|<players>|<max>|<tcpPort>` on UDP 47777);
  clients listen and list found hosts (6 s expiry).
- `src/net/Protocol.h` — framing: `[type:u8][len:u32 BE][payload]`;
  payloads are readable `key=value;` text. Types: Hello, Welcome,
  PlayerList, ChatMsg, Ready, StartGame, ClientInput, HostSnapshot,
  Disconnect.
- `src/net/Lobby.h` — host lobby: up to 10 players, ready tracking,
  auto 5v5 team assignment, all-ready or forced start.
- `src/net/Netcode.h` — host-authoritative: clients send
  `{seq, moveX, moveZ, yaw, buttons}` at 30 Hz; host broadcasts
  `{tick, id,x,y,z,hp,state}` snapshots at 20 Hz; clients apply directly.

### Limitations (v1, honest)

- No client-side prediction/reconciliation — fine on virtual-LAN
  latencies (<50 ms typical), visible lag on worse links.
- No delta compression or interest management — fine at our entity
  counts; revisit past ~200 tracked entities.
- No encryption at the game layer — Radmin VPN already encrypts the
  tunnel, so traffic between players is protected.
- Discovery uses UDP broadcast; heavily restricted sandboxes may block
  UDP outright (the test suite skips the live discovery test in that
  case and still validates the beacon protocol).
- The host's PC does all the simulating — a weak host means a laggy
  game for everyone. No dedicated server also means no 24/7 lobbies.

## Wave 7: world, controls & characters

Player-facing input, a radial command menu, melee combos, a plug-and-play
character framework (Cthulhu Avatar as the template), his Wave of
Domination RMB kit, world zones, procedural dungeons/caves, altars,
buildings, construction AI, and per-player KDA for the Tab stats overlay.

### Full controls (owner's bindings)

`src/input/InputManager.h` is the abstract input model: the engine fills
an `InputState` per frame (held/pressed/released edges per key) and calls
`update(state, dt)`. No OS calls, no wall clock — timestamps use
accumulated game time.

| Binding      | Behaviour                                                       |
|--------------|-----------------------------------------------------------------|
| WASD         | `moveVector()`: XZ-plane movement, diagonals normalized to 1    |
| Space        | jump — `jumpPressed()` with 0.15 s jump buffer + 0.12 s coyote time; `setGrounded()` is engine-filled |
| Shift        | sprint — drains the stamina gauge 18/s; exhaustion blocks sprint until regen passes 30/100 |
| Q / F / R    | ability slots 0/1/2: `bindAbility(id, cooldown)`; `abilityFired(i)` is true on the frame a ready slot triggers |
| E            | interact — `findInteractable(pos, candidates)`: nearest `Interactable` (Altar / Captive / Relic / Door) within 3.0 units |
| Tab          | edge-toggles `statsOverlayVisible()` (see the stats overlay below) |
| Alt + LMB    | `altLeftClick()` edge — the engine opens the radial command menu at `menuRequestPos()` |
| LMB          | melee attack / combo-chain input: `lmbHeld()`, `lmbLastClickTime()` feeds the combo tracker |
| RMB          | heavy / ranged attack — a per-character state machine (see Wave of Domination) |

Driver demos: `jump` (buffer + coyote), `sprint <on|off>`
(drain/regen/exhausted), `interact`, `menu`, `combo`, `rmb`.

### Command menu (Alt + left-click)

`src/ui/CommandMenu.h` is UI-agnostic: it builds the button data model
(which buttons, enabled or not, and why), dispatches the chosen action,
and offers `renderText()` — an ASCII radial listing for driver testing.
The Alt+left-click trigger itself is an engine concern
(`InputManager::altLeftClick()`).

What the click landed on (`MenuContext::Kind`) selects the button set:

| Context      | Buttons                                                        |
|--------------|----------------------------------------------------------------|
| Own cultist  | Follow, AttackTarget, SacrificeOrder, ConvertOrder             |
| Location     | MoveTo, RaidAt, BuildAltarAt, ScoutAt                           |
| Enemy        | AttackTarget, CaptureOrder                                     |
| Altar        | RitualSacrifice, RitualConvert, RitualNecromancy               |
| Captive      | SacrificeOrder, ConvertOrder                                   |

Every menu ends with Cancel. Buttons are disabled with a human-readable
reason, driven by a `GameStateSummary` the engine fills in before calling
`generateMenu()` (the menu does no world queries itself):

- no commandable cultists → every order button disabled;
- an altar already nearby → BuildAltarAt disabled;
- no captives held → RitualSacrifice disabled;
- rituals disrupted → all three ritual buttons disabled;
- too many live directive operations → RaidAt disabled.

Dispatch mapping: RaidAt → whole-cult `RaidCity` directive; AttackTarget
on an enemy → `GoToWar` plus a per-unit attack order; ConvertOrder /
RitualConvert → `ConvertCampaign`; RitualSacrifice → `MassSacrifice`
(obedience roll + `DirectiveExecutor` follow-through happen
automatically). The per-unit orders (Follow, MoveTo, CaptureOrder,
SacrificeOrder, BuildAltarAt, BuildAt, ScoutAt, RitualNecromancy) are
queued in `issuedOrders()` for the engine binding to consume and clear.

### Melee combos

`src/combat/Combos.h`: `ComboTracker` advances on LMB press edges fed
with explicit game-time timestamps (`onLmbClick(ts)`); `update(ts)`
resets on timeout. `currentStage()` is 0 when inactive, else the 1-based
stage. Each `ComboStage` carries the `animState` name to play (matches
`animationStateName()`), a `damageMult` (the caller multiplies it into
the existing damage pipeline), and an optional `ccType`/`ccSeconds`
(names match `ccTypeName()`; empty = none).

Default `basic_flurry` (1.2 s chain window):

1. jab — ×1.00, no CC
2. sweeping strike — ×1.15, Slow 1.0 s
3. heavy slam — ×1.50, Stun 0.75 s

Clicks arriving past the window reset to stage 1; a click on the final
stage starts a fresh chain.

### Character authoring: add a character in 5 minutes

No code changes needed. Drop a folder in `assets/characters/` and the
game picks it up at startup (`CharacterPackageLoader::scanAndLoad`;
driver `chars` lists them, `addchar <folder>` hot-loads one at runtime,
`validate <id>` prints the check report):

```
assets/characters/<name>/
  character.def   # required: stats, ability kit, RMB, passive
  model.fbx       # optional: rigged model (logic-only without it)
  rig.map         # optional: "engine_bone = fbx_bone" lines
  bones.list      # optional: one FBX bone name per line (lets the
                  #           auto-mapper run before the FBX importer lands)
  animations/     # optional: .canim clips
```

`assets/characters/cthulhu_avatar/` is the worked template — copy the
folder, rename it, edit the values.

Wave 25 ships all 20 playable kits (`assets/characters/<id>/character.def`,
generated by `generate_kits.py`; roles, power budget, and the effect
vocabulary are documented in `assets/characters/KITS.md`). Folders ship
without models — the engine uses the procedural placeholder until a
`model.fbx`/`model.glb` + `rig.map` is dropped in.

#### character.def field reference

Top-level: `id` (required, usually matches the folder), `display_name`,
`flavor`, `max_hp`, `move_speed`, `max_stamina`, `melee_combo`,
`rmb_ability` (optional state-machine kit id, e.g. `wave_of_domination`).

`[q]` / `[f]` / `[r]` ability sections: `id`, `name`, `flavor`,
`cooldown`, `stamina_cost`, `mana_cost`, `effect` (kind string the engine
binds: `aoe_damage`, `fear_aura`, `summon`, `buff`, ...), `power`,
`range`.

`[rightclick]`: `kind` (`MeleeHeavy` | `MindControl` | `AcidSpit` |
`EldritchGrasp`), `name`, `damage_mult`, `range`, `cc` (optional CC type
name), `cc_seconds`. If `rmb_ability` names a registered kit (see
`createRmbAbility()`), the kit's state machine drives RMB and these
numbers are the fallback.

`[passive]`: `id`, `desc` (behavior hook; the engine binds it).

Lines starting with `#` are comments; unknown keys are ignored so future
fields don't break old files.

The C++-side data model is `CharacterDef` (`src/characters/`):
`SpellDef` (Q/F/R kit), `HeavyAttackDef` (right-click numbers),
`rmbAbilityId` (optional state-machine kit), `meleeComboId` (forward
reference into the combo system), `passiveId`/`passiveDesc` (hooks).
`CharacterRegistry` stores them by string id; duplicate or empty ids are
rejected so two definitions can never fight over one key. Baseline tuning
to measure against: cultist 100 HP, civilian 50 HP, stock melee hit 10
(see `combat/Attacks.h` tuning notes).

The Cthulhu Avatar (`"Cthulhu, the Dreaming God"`, id `cthulhu_avatar`):
500 HP / 6.0 m/s / 100 stamina; Q "Tentacle Slam" (aoe_damage, 120 power,
8 m, 8 s cd, 25 stam); F "Nightmare Veil" (fear_aura, 4 s fear, 12 m,
15 s cd, 35 stam); R "Call of the Deep" (summon, power 4, 20 m, 45 s cd,
50 stam); combo `eldritch_flurry`; RMB `wave_of_domination`; passive
`dreamers_presence`.

#### Rig mapping

The engine expects an 11-bone humanoid rig: `hips, spine, head`,
`upperArmL/R`, `lowerArmL/R`, `upperLegL/R`, `lowerLegL/R`. The
auto-mapper matches FBX bone names case-insensitively with alias tables
for Mixamo (`mixamorig:LeftArm` → `upperArmL`), Blender Rigify
(`upper_arm.L` → `upperArmL`, `thigh.L` → `upperLegL`) and generic DCC
names (`pelvis` → `hips`). Every bone gets a confidence score (1.0 exact,
0.9 alias); unmatched bones are reported, never silently dropped. An
explicit `rig.map` overrides auto-mapping.

#### What happens when pieces are missing

The validator (`CharacterValidator`) reports loudly but the game always
runs:

| Missing              | Behavior                                             |
|----------------------|------------------------------------------------------|
| `model.fbx`          | "model slot empty": logic-only, no visuals           |
| `animations/`        | procedural fallback for walk, run, idle, attack...   |
| `rig.map`            | auto-map with reported confidence (needs a bone list)|
| unknown `rmb_ability`| falls back to the `[rightclick]` numbers             |
| bad `character.def`  | package skipped with the reason logged               |

Note: register the Cthulhu Avatar from exactly one source — the package
scan is the recommended one. If the code also registers
`makeCthulhuAvatar()`, the scan reports a duplicate id.

### Wave of Domination (Cthulhu Avatar RMB)

Per-character right-click kits are state machines implementing
`RmbAbility` (`src/characters/abilities/RmbAbility.h`): the game feeds
input edges (RMB press/release, mouse move, LMB) plus a world snapshot
(`RmbContext`), and the ability drives positions, CC, damage and events.
New characters register kits in `createRmbAbility()` and name the kit in
`CharacterDef.rmbAbilityId`. "Wave of Domination" is the reference
implementation and the template for how RMB kits should feel.

State machine: Ready →(RMB)→ Wave →(victims caught)→ Hold →(LMB)→
Flying, or →(RMB released)→ dropped. Cooldown 7 s starts on cast.

- **Wave**: mind-control wavefront travels forward 25 m at 12 m/s, 4 m
  wide. Catches civilians, adventurers and cultists of any faith except
  Chaos. Chaos-aligned lunatics are immune (madness shields the mind);
  feral monstrosities are immune (mindless). Max 5 victims.
- **Hold** (RMB held): victims levitate at an anchor 3 m in front of
  Cthulhu, 2 m up, + mouse swing offset (smoothed, max 6 m). Victims are
  immobilized while held.
- **Slam**: a swung victim intersecting a building (2.5 m) or another
  entity (1.2 m) deals 150 damage and the victim dies.
- **Launch** (LMB while held): all victims hurled forward at 30 m/s; on
  impact they deal 300 damage (2× slam) and die; victims that fly 40 m
  without hitting anything drop and survive.
- **Drop** (RMB released): victims land gently and live with a 1 s stun
  stagger — unless dropped from above 8 m, in which case the fall kills.
- **Direct stun** (RMB with an explicit cursor target): that entity is
  Stunned 2.5 s instead, on the same 7 s cooldown.
- **Kills** from slam/launch/falls emit `CivilianSlain`, feeding Onslaught
  exertion through the existing pipeline.

Tuning table (all from the `k*` constants in `WaveOfDomination.h`):

| Parameter | Value |
|---|---|
| Cooldown | 7.0 s (from cast) |
| Wave range / speed | 25 m / 12 m/s |
| Wave half-width | 2.0 m (4 m wide wavefront) |
| Max victims | 5 |
| Anchor | 3 m forward, 2 m up; swing max 6 m |
| Slam damage | 150 |
| Launch damage / speed / max distance | 300 (2×) / 30 m/s / 40 m |
| Building / entity hit radius | 2.5 m / 1.2 m |
| Lethal drop height | 8.0 m |
| Gentle-drop stagger | 1.0 s Stun |
| Direct stun | 2.5 s Stun |

Driver: `rmb press` (casts; drops a demo civilian in the wave path when
empty), `rmb move <dx> <dy>`, `rmb launch`, `rmb release`,
`rmb stun [entity-id]`.

### World: zones, dungeons, altars, buildings, construction AI

- **`Zone`** (`src/world/Zone.h`): named rectangular region built from a
  plain `ZoneDef` (name, bounds, spawn points, ambient params, relic
  spots). Data-driven — a future loader can fill `ZoneDef` without
  touching the class. Helpers: `contains()`, `center()`,
  `randomPoint(rng)`, `randomRelicSpot(rng)`. **`WorldMap`**: named
  collection of zones + `zoneAt(pos)`; zones may overlap and the first in
  insertion order wins, so list specific zones before general ones.
- **`DungeonInstance`** (`src/world/Dungeon.h`): seeded procedural layout
  on a 48×48 tile grid (`#` wall, `.` floor, `E` entrance), 4.0 world
  units per cell. Algorithm: rejection-sample up to 12 non-overlapping
  rooms (4–10 cells), then join consecutive rooms with L-shaped corridors
  (random elbow order). Same seed → identical layout. **`CaveInstance`**:
  organic variant — drunkard's-walk tunnels from the center plus stamped
  circular chambers. The caller creates the entrance entity and links it
  with `setEntranceEntity()`; entities move between the surface roster
  and the inside roster with `registerSurfaceEntity()` / `enter(id)` /
  `exit(id)` (each emits `DungeonEntered` / `DungeonExited`).
  `relicSpots()` gives world-space relic positions at room centers past
  the entrance; `setBossId(id)` + `onEntityDied(id)` complete the dungeon
  (`DungeonCompleted`) when the boss dies.
- **`Altar : public Building`** (`src/entities/Structures.h`) with the
  entity type fixed up to `EntityType::Altar`, so altars inherit
  damage/destruction/Reconstruction auto-rebuild while remaining a
  distinct entity kind. Tiers 1–3 (`upgrade()` clamps);
  `ritualPowerMult()` = 1.0x / 1.5x / 2.0x. Captive bookkeeping for the
  later escort layer: `assignCaptive(id)` / `unassignCaptive(id)` /
  `assignedCaptives()`.
- **`BuildingType`** (`Altar, Barracks, Wall, Watchtower, Trap, Portal`)
  with per-type default HP: Altar 800, Barracks 1500, Wall 2500,
  Watchtower 600, Trap 300, Portal 1200. `Building` gained a typed ctor
  `(faction, pos, type, maxHp = 0)`; the legacy ctor is untouched.
- **`ConstructionSite`**: progress 0..1 advancing linearly with builder
  count (n builders = n× speed; one builder finishes in 120 s; zero
  builders stall). `complete()` hands back the finished `Building` (a
  real `Altar` for altar targets), `nullptr` for repair sites and on
  repeat calls. Driver: `build <type>` starts a site at the avatar; `tick`
  advances it; finished sites spawn into the world.
- **`BuilderAI`** (`src/ai/BuilderAI.h`): idle, loyal-enough cultists
  (alive, `Loyal` state, devotion ≥ 40) autonomously start and assist
  builds near their settlement. What gets built is weighted by belief
  exertion (≥ 50 counts as high): War → Walls/Barracks, Magic → Altars,
  Breeding → Portals, Trickery → Traps, Reconstruction → repairs. New
  orders appear ~every 20 s (±25% jitter) while idle labor exists (max 3
  active sites per settlement, 4 builders per site). Reconstruction-high
  + damaged friendly buildings → repair orders jump the queue. New
  construction halts when insurrection risk ≥ 70. Emits `BuildStarted` /
  `BuildProgress` (throttled to 10% steps) / `BuildCompleted`.

### KDA & the Tab stats overlay

`PlayerStatsTracker` (`src/net/PlayerStats.h`): the host feeds it damage
and kill events (`setPlayerName`, `recordDamage(dealerIdx, entityId,
amount, now)`, `recordKill(killerIdx, victimIdx[, entityId, now])`) and
`rows()` returns `net::KdaRow` {name, kills, deaths, assists} snapshots in
player-index order. Assist rule: every *other* player who damaged the
victim's entity within the last 10 s gets exactly one assist per kill;
the killer never self-assists; the 2-arg `recordKill` records
kill/death only (e.g. environmental deaths). Driver: `kda` runs a scripted
demo.

Net integration: new `MsgType::PlayerKda`, broadcast at 1 Hz alongside the
20 Hz snapshots (`NetHost::setKdaProvider()`; no provider = no KDA
traffic); clients read the latest standings via `NetClient::kda()`; the
lobby roster carries kills/deaths/assists (`HostLobby::updateKda`).

`StatsPanel::gather()` (`src/ui/StatsPanel.h`) fills a pure-data
`StatsData` struct for the Tab overlay (the engine renders it): roster
headcount by cultist state, total kills, Cthulhu's power (0..1000),
insurrection risk (0..100), the 12 belief-exertion gauges (0..100), and
the per-player KDA table.

### Tests

Wave 7 adds the `cultulhu_tests_wave7*` suites (zones, dungeons/caves,
altars, buildings, construction sites, builder AI, command menu, input
model, combos, stats panel, character packages, validation, RMB
abilities, KDA): all passing alongside the earlier waves (21/21 ctest
suites green as of wave 13 + overnight hardening).

## Wave 10: balance-sim recommendations applied (R1–R5)

The five structural fixes the wave-9 sim flagged but left unapplied:

- **R1 — diminishing returns on stacked belief income.** Power deltas now
  scale by active-belief count: 1× / 0.75× / 2-beliefs, 0.5× / 3-beliefs
  (`ExertionSystem::stackedIncomeMultiplier`). Conversion×Trickery×Magic
  dropped from 2.28× to 0.79× the scenario median.
- **R2 — exertion can reach the synergy gates.** `DECAY_PER_SEC` 0.5→0.05,
  `SYNERGY_THRESHOLD` 50→25, `TENSION_THRESHOLD` 70→35; sparse feeds bumped
  so every belief warms under steady play (SacrificeCompleted +12→+20,
  BuildingRebuilt +2→+8, HealPerformed +1→+2, DreamWhisper +1→+8). The full
  interaction matrix is live in the sim: Terror, Nightmare Surge, Blood
  Frenzy, Dark Rites, Infiltration, Martyrs' Visions, Oneiromancy all fire;
  conflict suppression holds Reconstruction×War below the tension gate.
- **R3 — organic insurrection fuel.** Cultists under 20 devotion stoke
  +0.2/s risk (`CultManager::update`). Chaos scenarios now revolt ~3–5× per
  hour organically instead of only via punishment (kept at 0.2/s, not 0.5/s,
  so it stays a slow burn rather than a chained alarm).
- **R4 — Onslaught idle decay reachable.** `ONSLAUGHT_IDLE_LIMIT` 3600s→
  1200s: 20 idle minutes now bite (not measurable in the sim — the bot
  never idles).
- **R5 — Reconstruction & Dreams power paths.** Rebuilding +2 power,
  heals +0.5; dream-whispers 2.0→13.0. Dreams now reaches 500 power;
  Reconstruction earns 66/run from its own actions (190→256 final).

Re-ran the full sim (18 scenarios × 8 seeds × 3600 ticks; report in
`sim_reports/wave10_report.md`, wave-9 CSVs archived under
`sim_reports/wave9_baseline/`). Watch item: Fear×Torture is the fastest
loadout at 1.99× the scenario median (Terror synergy now fires, as
designed) — inside the 2× flag, but the flag may want recalibration if
the target is ≤1.5× for all loadouts. No sim scenario pairs a hot
conflict, so tension events are covered by unit test
(`testTensionEvents`) rather than the sim matrix.

## Wave 13: Vale of Pnath deep dungeon

Signature Dreamlands dungeon (public-domain Lovecraft concept; see
`assets/creatures/LEGAL_NAMES.md`): a 72×72 `ValeOfPnath` dungeon with
per-room depth and a dread value (0 at the mouth → 1.0 at the vault) that
scales everything. Hazards: abyss pits (20 + 12×depth fall damage),
maddening whispers (fear grind), and dhole tunnels — first traversal
telegraphs tremors, the next triggers an ambush. The Dhole (1500 HP,
burrowing, 18m fear aura) is a mini-boss; the deepest room holds a cursed
relic vault with a guardian. `dungeon pnath [seed]` in the driver.
The dhole model slot is an intentional placeholder (no CC0 dhole model
exists); the engine falls back to a procedural serpent/worm shape.
New exertion feeds: pit falls / whispers / ambushes feed Fear, the relic
feeds Magic.

## Wave 12: NPC + creature bodies

Five NPC character packages (`cultist_hooded`, `cultist_magus`,
`civilian_villager`, `civilian_guard`, `civilian_laborer`) and seven
creatures (`pale_wight`, `ossified_brute`, `charnel_imp`,
`skittering_ghoul`, `wraith`, `risen_dead`, `dagon_spawn`) — all
CC0 KayKit/Kenney models, all names legal-name audited. Driver:
`bestiary` lists the catalog, `spawn monstrosity [species] [n]` spawns
them. The 12 large `.glb` models ship as Base64 `.glb.b64` text
(restored by `python3 assets/decode_assets.py` after cloning).
Honest gap: no genuinely Mythos-specific CC0 models exist anywhere
surveyed — no tentacled horrors, true deep ones, shoggoths, or winged
terrors; the owner's own sculpts remain the real fix.

## Wave 11: lightweight world art pass

38 CC0 Kenney models (0.72MB total, none over 626 triangles) plus
`assets/maps/ruined_city.map`: 90 placements across cult base, graveyard,
old city, and outskirts. `ModelCatalog` + `MapLoader` + the `mapinfo`
driver command validate the art; UE5 import/performance guidance lives in
`unreal/Docs/ArtImportAndPerf.md`. Quaternius was evaluated and rejected
(its license is no longer CC0). Binaries travel as `.b64` text because
GitHub's file API mangles raw binary uploads.

## Wave 9: balance sim, new content, robustness

### Balance simulator (`src/sim/`, binary `cultulhu_sim`)
Headless batch runner: 18 scenarios (12 single-belief loadouts + triples
Fear×Torture, War×Onslaught, Dreams×Chaos, Conversion×Trickery×Magic,
Sacrifice×Magic×Dreams, Chaos×Reconstruction×War) × 8 seeds × 3600 ticks,
with a scripted ScenarioBot playing each belief at a fixed schedule.
Metrics: power-over-time curve, ticks-to-500/800, insurrection events,
conversions, war kills, deaths by cause. Reports land in `sim_reports/`
(git-ignored CSVs + committed `analysis.md`).

Findings (full detail in `sim_reports/analysis.md`):
- Conversion×Trickery×Magic is a 2.28× outlier (fastest to power 500) —
  linear stacking with no diminishing returns (logged as recommendation R1).
- The synergy/conflict matrix is currently unreachable: peak exertion 31.5
  observed vs the 50/70 synergy/tension gates (R2).
- Insurrection never fires organically outside Chaos punishment paths (R3);
  Onslaught idle-decay never triggers at the 3600s limit (R4);
  Reconstruction generates ~zero power (R5).

Conservative tuning applied (each within ±20%, documented):
TORTURE_PER_VICTIM 8.0→6.4, SACRIFICE_COMPLETED 25.0→30.0,
FEAR_RAID_POWER 12.0→14.4, ONSLAUGHT_PER_CIVILIAN 3.0→3.6.

### New directives
- **Assassinate Prophet**: 60s infiltration; per-tick strike chance
  `0.08 + 0.30×Trickery − dist/2000`; exposure risks an insurrection nudge
  and a War event; success assassinates the enemy leader (Fear/War spike).
- **Blight Land**: 120 corruption ticks on a zone; Fear rises, civilian
  output falls; zone stays flagged Blighted.
- **Grand Summoning**: 90s ritual costing 300 power up front (no refund on
  interruption); completes into a 1200 HP dread champion.
Driver: `directive assassinate|blight|summon`, `directive zones`.

### New ambient events & dungeon hazards
Ambient: omen-reading (Dreams synergy), sparring, tending wounded
(Reconstruction synergy), sigil graffiti (+zone Fear), chanting circle
(Magic exertion, may lure a creature). Hazards in the procedural
generator (seeded): spike pits (25 dmg, once per entity), trapped rooms
(15 dmg, one-shot, feeds Trickery), cave-ins (20 dmg, 35% trigger, 40%
seal the passage). Driver: `ambient [name]`, `dungeon [seed]`.

### The Vale of Pnath (wave 13) — signature deep dungeon
Lovecraft's Dreamlands abyss (public domain; only Lovecraft-original
terms are used — see `assets/creatures/LEGAL_NAMES.md`). A special
`ValeOfPnath` dungeon type, deeper and larger than normal dungeons:
72×72 grid, up to 20 rooms, each room carrying a **depth** value (0 at
the mouth, growing along the corridor chain) that reads as vertical
descent. Entered through a surface fissure or from the deepest cave
tier. Driver: `dungeon pnath [seed]`.

- **Escalating dread**: `dreadAt(room) = depth/maxDepth` in [0,1].
  Deeper rooms hit harder and feed more Fear exertion.
- **Vale hazards** (seeded, via the virtual `fireHazard()` hook):
  abyss pits (`O`: 20 + 12×depth fall damage, once per entity),
  maddening whispers (`w`: 4 + 16×dread fear, every traversal),
  dhole tunnels (`D`: two-stage — first traversal publishes
  `DholeTremors` as a telegraph, the next traversal triggers
  `DholeAmbush` for 55 + 45×dread damage, attributed to the linked
  dhole entity when one is set).
- **Dholes**: `Dhole : Monstrosity` (`src/entities/Units.h`) — 1500 HP
  burrowing ambusher, feral, with an 18m fear aura (12 fear/sec).
  Starts burrowed; `surface()`/`burrow()` toggle it. No CC0 dhole
  model exists anywhere surveyed, so the `ModelCatalog` "dhole" slot
  is an intentional empty placeholder and the procedural
  serpent/worm-like fallback applies.
- **Relic vault**: the deepest room holds the cursed artifact
  (`valeRelicSpot()`); spawn the guardian at `guardianSpawnPos()`
  and register it with `setBossId()` — its death completes the Vale
  (`DungeonCompleted`), and seizing the relic publishes
  `ValeRelicClaimed` (Magic exertion).

### Robustness: fuzz harness (`src/fuzz/`, binary `cultulhu_fuzz`)
150k deterministic iterations (+300k extra seeds, all clean; ASan/UBSan
clean) across three targets: event bus, command parser, driver REPL
dispatch — malformed events, mutated/garbage/10KB inputs, NaN/Inf values.
Fixed 4 real bugs: `SaveSystem::load` uncaught exceptions on malformed
saves (now returns false); net decode CPU-hang on attacker-controlled
count (`n > 4096` rejected); `ClipSerializer` allocation bomb (1M key cap,
NaN rejected); heap-use-after-free in WaveOfDomination when victims were
freed by a load mid-levitation (stale victims pruned, ability cancelled on
load). Regression tests in `cultulhu_tests_fuzz`.

## Wave 15: content expansion — new ambient events + two directives

Conservative new content reusing the wave 9b/9c systems (no new models,
no new systems; all per-tick work O(1)).

### New directives
- **OneiricHarvest** (Dreams): 60s mass dream-rite; each 10s tick the
  cult's loyal dreamers channel a vision (`DreamShared` tag
  `directive_dream`, so the exertion pipeline picks it up unchanged).
  Completion publishes `OneiricHarvestCompleted` and distills the dreams
  into 3 power per dreamer (skipped when no `PowerSystem` is attached).
- **RebuildSanctum** (Reconstruction): 60s rebuilding; per-tick
  `SanctumRebuiltTick` (Reconstruction 1.0), then on completion a
  `BuildingRebuilt` (reads as restored infrastructure to every listener)
  + `SanctumRebuilt` + a +5 devotion bump for the whole cult. Partial
  obedience halves ticks and the devotion bump.
Both feed their creed's exertion on Obeyed (8.0) and Chaos on Refused
(4.0), and gain +0.10 obedience when their creed is active.
Driver: `directive dream|rebuild`; also `command dream|rebuild` (the
`directive` path wires the executor context; `command` degrades
gracefully).

### New ambient events
Five new autonomous cultist activities in `AmbientDirector` (now 16),
each with belief weighting, an event, and exertion feeds:
- **dream-sharing** (Dreams; night bonus): recounts a vision
  (`DreamShared`, Dreams 3.0 / Conversion 1.0); when Dreams exertion
  runs hot (>= 50) a distant civilian may convert (`DreamWhisper` +
  `ConversionPerformed`, tag `dreamshared`).
- **mend-effigy** (Reconstruction; low-morale bonus): repairs a shrine
  (`EffigyMended`, Reconstruction 2.0), +2 devotion.
- **whisper-campaign** (Trickery/Fear): plants false rumors
  (`RumorSpread`, Trickery 3.0 / Fear 1.0), +zone fear when a world map
  is attached, 20% chance a civilian converts (tag `rumor`).
- **blood-rite** (Torture/Sacrifice): ritual laceration (`RiteOfFlesh`,
  Torture 3.0 / Fear 1.0), +2 devotion, 10% self-injury.
- **wilds-hunt** (Onslaught/War): hunts a wild beast (`WildsHunted`,
  Onslaught 2.0) bringing back 1–2 supplies, 15% chance the quarry
  fights back.
`BeliefSystem` reacts like the wave-9c events (power on shared dreams,
mended effigies, hunted meat; dread on the blood rite when Fear is
active). Driver: `ambient dream|mend|rumor|rite|hunt` (or bare
`ambient` for a weighted-random action).

## Discrepancy notes (faithful to the doc)

- The doc said "12 beliefs" but listed 11. The 12th — **Dreams** — was chosen
  by the project owner and is now implemented (rest, dream-visions,
  dream-whisper conversions, nightmares).
- The doc's belief list mentions Cthulhu specifically, but the game lets the
  player choose any cosmic horror entity — the systems are entity-agnostic
  (faction ids), with Cthulhu as the default flavor.

## Roadmap

- **Playable beta** (done): free roam mode, Dreams belief, switchable
  FP/TP camera, EldritchAvatar, model plug-in points + `assets/models/`
  convention, animation state machine with Mixamo naming + procedural
  fallback, combat CC (Stun/Slow/Root/Fear), eldritch directives with
  obedience model, autonomous cultist ambient behavior, day/night effects,
  headless REPL driver (`cultulhu_play`), 81 beta tests.
- **Unreal binding**: map `Vec3`→`FVector`, entities→`AActor` subclasses,
  `EventBus`→Unreal delegates, `GameClock`→world time, `CameraSystem`→
  `APlayerCameraManager`, `AnimationStateMachine`→Anim Blueprints; keep core
  engine-agnostic and bind at the edges.
- **Networking** (done, wave 6): Radmin VPN player-hosted multiplayer —
  UDP discovery beacons, TCP lobby (10 players, 5v5 teams, ready-up),
  host-authoritative 30 Hz inputs / 20 Hz snapshots, driver commands
  (`myip`, `discover`, `host`, `join`, `ready`, `players`, `startgame`).
  No dedicated server; host's PC simulates.
- **Wave 5** (done): animation clip data model (bone tracks/keyframes/poses),
  `.canim` text serialization, procedural Walk/Run/Idle/Attack/Death
  generators, crossfade blending in the state machine, FBX importer hook
  interface, directive follow-through executor (live operations per directive
  with progress/completion events), driver `save`/`load` commands.
- **Animation pipeline**: Mixamo auto-rig + animation library / Blender
  Python (`bpy`) scripted animation generation on player-supplied rigged
  FBX models; retarget into engine Animation Blueprints. (Core-side data
  model, procedural clips, `.canim` format, and FBX hook are done —
  `assets/animations/` is the drop point.)
- **Content**: per-entity tuning, full 5v5 matchmaking flow, more ambient
  variety and directive types. (Driver save/load done.)

## Performance: keeping it light (owner constraint)

The host's PC simulates the world for every player, so the game is
budgeted for modest machines from day one. Current discipline:

- **Asset budget**: every model < 5k triangles, total art < 30 MB, no
  texture above 512 px (most packs are vertex-colored, no textures at all).
- **Map budget**: `assets/maps/ruined_city.map` holds ~60–120 placed
  props — streets, ruins, and the cultist base stay readable without
  drowning the host.
- **Sim budget**: the core is plain C++17 with no engine overhead in the
  hot loop; snapshots broadcast at 20 Hz, client inputs at 30 Hz.

### Optimization levers available later (not yet needed)

If profiling ever shows the host struggling, these are the planned,
in-order levers — cheapest wins first:

1. **Instanced rendering** — repeated props (tombstones, wall segments,
   braziers, trees) become UE5 Instanced Static Meshes: one draw call per
   prop type instead of one per prop. (See
   `unreal/Docs/ArtImportAndPerf.md`.)
2. **LODs** — auto-generated LOD chains on buildings/props (UE5 can
   generate these on FBX import); Nanite is an option for hero pieces only,
   never for instanced small props.
3. **Occlusion culling** — UE5's built-in culling + precomputed visibility
   volumes for the dungeon interiors so unseen rooms cost nothing.
4. **Texture streaming** — already tiny (≤512 px), but UE5 texture
   streaming keeps only visible mips resident if we ever add larger art.
5. **Spatial partitioning for entity queries** — the sim's radius/nearest
   queries (wave catch checks, AI perception, slam collision) move from
   linear scans to a uniform grid / quadtree when entity counts grow.
6. **Dynamic AI tick scaling** — ambient cultist AI already runs on
   staggered intervals; under load, distant/off-screen cultists tick less
   often (0.5 Hz) while near-camera ones stay at full rate.
7. **Snapshot delta compression** — netcode currently sends full 20 Hz
   snapshots; deltas + quantized positions halve host upload bandwidth.
8. **Dungeon interior streaming** — dungeon/cave interiors load only while
   a player is inside (or near an entrance); surface props stream by zone.

None of these change game design — they are pure engineering levers to
pull if and when the profiler says so.

## Art: the ruined city (wave 11)

The game now ships with real art — a dark Lovecraftian ruined city and
cultist base built from free models, kept deliberately light for the
player-hosted model.

### Sources & licenses (all CC0 1.0, verified on the source sites)

| Pack | URL | Used for |
|---|---|---|
| Kenney Graveyard Kit | https://kenney.nl/assets/graveyard-kit | tombstones, crypts, dead trees, fences, debris |
| Kenney Castle Kit | https://kenney.nl/assets/castle-kit | ruined walls, towers, gate, ground |
| Kenney Mini Dungeon | https://kenney.nl/assets/mini-dungeon | dirt, floors |

Full per-file table in `assets/world/MANIFEST.md`, license statements in
`assets/world/LICENSES.md`. Quaternius was evaluated and **rejected** —
its license changed to a proprietary one in 2026, failing the CC0-only
rule. One consistent low-poly style (Kenney) keeps the look coherent.

### Budget (hard, enforced by tests)

- 38 models downloaded, **0.72 MB total** (cap 30 MB)
- Max single model **626 triangles** (cap 5,000); 7,414 tris total
- 3 shared palette textures, exactly **512×512 px** (cap 512)
- Map uses 11 models across **90 placements** in 4 zones
  (`assets/maps/ruined_city.map`): `cult_base` (altar plaza, braziers,
  banners, shelters), `graveyard`, `old_city` (ruined streets, collapsed
  towers), `outskirts`

### Art direction

Desaturated greys and moss greens for stone, ember-orange braziers as the
only warm light source, violet-black fog. Documented in the header of
`assets/maps/ruined_city.map`.

### Swapping / extending the map

1. Drop new CC0 `.glb` files into `assets/world/<category>/` (keep the
   budgets — `tests_wave11` enforces them).
2. Add a row to `assets/world/MANIFEST.md`.
3. Add `place <path> <x> <y> <z> <rotY> <scale> <zone>` lines to
   `assets/maps/ruined_city.map` (format documented in its header).
4. Run `mapinfo` in the driver — it lists placements and fails loudly on
   missing files. `ctest` re-validates everything.

## Art: the eldritch battlefield (wave 17)

A second art pass for the dark look the owner asked for — gothic ruins, dead
nature, cave rock, ember/fire accents. Still brutally light.

### Sources & licenses (all CC0 1.0, verified per pack)

| Pack | URL | Used for |
|---|---|---|
| Kenney Graveyard Kit (more) | https://kenney.nl/assets/graveyard-kit | obelisk, bone pile, dead trunks, crypts |
| Kenney Nature Kit | https://kenney.nl/assets/nature-kit | cave boulders, rock spires, dark trees |
| KayKit Dungeon Remastered | https://kaylousberg.itch.io/kaykit-dungeon-remastered | pillars, torches, chest, banner, spike trap |
| KayKit Halloween Bits | https://kaylousberg.itch.io/halloween-bits | tombstones, dead tree, lantern, candles |

Per-file table in `assets/world/MANIFEST.md`, license statements in
`assets/world/LICENSES.md`.

### Budget

- 27 models, **~0.71 MB total** (all < 1000 tris; max 728)
- 2 shared 512×512 atlas textures (KayKit atlases downsampled from 1024);
  Kenney graveyard models reuse the wave-11 palette, nature models untextured
- New map `assets/maps/eldritch_battlefield.map`: **72 placements** in 4
  zones — `shattered_court` (obelisk, crypts, banners, spike traps),
  `cavern_mouth` (boulders, rock spires), `ashen_grove` (dead forest),
  `ember_shrine` (torches, lanterns, candles, chest)
- `ruined_city.map` untouched (90 placements, still asserted by tests)

### Known art gaps (honest)

No CC0 source found for stalactites/stalagmites, dark crystals, or hanging
chains — surveyed Kenney, KayKit, itch.io, and OpenGameArt. These remain
original-art tasks.

## Art: second dark-art pass (wave 19)

More atmosphere for the eldritch battlefield — ruined arches, broken stairs,
bone decorations (ribcages, skulls, scattered bones), stone coffins, dead
bushes, collapsed beams, plus in-house mist banks, scorched-earth patches,
and ember clusters.

### Sources

14 more models from the already-verified CC0 packs above: KayKit
Halloween Bits (arches, gate, bones, ribcage, skulls, coffin, dead tree,
broken fence) and KayKit Dungeon Remastered (wide stairs, rubble, broken
floor tile, arched wall, broken wall). 5 props + 5 textures generated
in-house (original work, no license to verify) — see
`assets/world/LICENSES.md` §8.

### Budget

- 19 models, **~0.54 MB decoded GLB** (all ≤ 1000 tris except ribcage at
  1240 — under the 2000 hard cap), + ~90 KB new textures (all ≤ 256px)
- `assets/maps/eldritch_battlefield.map` extended 72 → **120 placements**;
  `ruined_city.map` untouched

### ModelCatalog names

`stone_arch`, `dark_gate`, `scattered_bones`, `ribcage`, `skull`,
`skull_candle`, `stone_coffin`, `dead_tree_large`, `broken_fence`,
`ruined_stairs`, `large_rubble`, `broken_step`, `dungeon_arch`,
`broken_wall`, `mist_bank`, `scorched_earth`, `ember_cluster`,
`collapsed_beams`, `dead_bush`.

## Bestiary: NPCs & creatures (wave 12)

Civilians, cultists and horrors finally have bodies. All CC0, all light.

> **Binaries are base64-packed.** The GitHub push path corrupts raw binary
> files, so every `.glb`/`.png` ships as a `*.b64` text sidecar. After
> cloning, run **`python3 assets/decode_assets.py`** once to restore the
> binaries. Tests and the driver reference the decoded `.glb` paths.

### NPCs (character packages — auto-discovered, validated)

| package | model | source / license | tris | role |
|---|---|---|---|---|
| `cultist_hooded` | Rogue_Hooded | KayKit Adventurers / CC0 | 6,035 | rank-and-file cultist |
| `cultist_magus` | Mage | KayKit Adventurers / CC0 | 5,683 | cult leader / high priest |
| `civilian_villager` | Rogue | KayKit Adventurers / CC0 | 6,377 | townsfolk |
| `civilian_guard` | Knight | KayKit Adventurers / CC0 | 6,952 | city watch / militia |
| `civilian_laborer` | Barbarian | KayKit Adventurers / CC0 | 6,543 | dockworker / laborer |

All five are **rigged** (skeleton intact) with animation clips stripped for
size — our procedural animation fallback drives them. Catalog role keys
(`ModelCatalog::creatureModel("cultist")` etc.) resolve humanoid NPC
entity types to these packages. Source:
https://github.com/KayKit-Game-Assets/KayKit-Character-Pack-Adventures-1.0 —
"Free for personal and commercial use, no attribution required. (CC0 Licensed)".

### Creatures (bestiary — `assets/creatures/`)

| species key | file | source | tris | rigged | role |
|---|---|---|---|---|---|
| `pale_wight` | pale_wight.glb | KayKit Skeletons / CC0 | 4,588 | yes (clips stripped) | robed skeletal oracle — cult horror |
| `ossified_brute` | ossified_brute.glb | KayKit Skeletons / CC0 | 5,934 | yes (clips stripped) | heavy bone horror |
| `charnel_imp` | charnel_imp.glb | KayKit Skeletons / CC0 | 5,288 | yes (clips stripped) | swarm chaff |
| `skittering_ghoul` | skittering_ghoul.glb | KayKit Skeletons / CC0 | 5,278 | yes (clips stripped) | fast skirmisher |
| `wraith` | wraith.glb | Kenney Graveyard Kit / CC0 | 413 | no (static) | lesser servitor, drifts between graves |
| `risen_dead` | risen_dead.glb | Kenney Graveyard Kit / CC0 | 1,078 | no (static) | reanimated corpse |
| `dagon_spawn` | dagon_spawn.glb | Kenney Mini Dungeon / CC0 | 374 | skin data present | deep-one hybrid brute |

Spawned monstrosities resolve their `species` string through
`ModelCatalog::creatureModel()` — `spawn monstrosity pale_wight 3` just
works. Naming is honest: the KayKit models are undead horrors re-themed
for the bestiary, the Kenney pair are graveyard spooks, and the orc is a
brute re-themed as a deep-one hybrid — none are true Mythos entities.
Genuinely Lovecraftian CC0 models (tentacled things, true deep ones,
shoggoths, winged terrors) were not found in any CC0 source surveyed
(Kenney, KayKit, OpenGameArt, itch.io). Per-file sources
in `assets/creatures/MANIFEST.md`. (Naming follows
`assets/creatures/LEGAL_NAMES.md`: in-game creature names use only
Lovecraft-original public-domain names or plain generic English.)

### Budget note

Rigged characters get their own tier: **<10k tris** per character model
(static world props stay <5k). KayKit humanoids land at 5.7k–7k indexed
tris — still mobile-light and far below anything that troubles a host PC.
Total wave-12 model bytes stay under the 10 MB cap. Enforced by
`tests_wave12`.

### Driver

- `bestiary` — lists every registered creature/NPC model with tri counts
  and file-presence checks.
- `spawn monstrosity [species] [n]` — spawn a bestiary species by name
  (e.g. `spawn monstrosity deep_one 2`); unknown species spawn logic-only.

### UE5 import

See `unreal/Docs/ArtImportAndPerf.md`: Content Browser folder layout,
import settings, and the key rule — repeated props become **Instanced
Static Meshes** (one draw call per prop type), LOD chains on import,
shared darkening material for the grade.
