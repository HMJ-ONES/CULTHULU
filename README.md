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
`attack`, `cast <fireball|fear>`, `tick <n>`, `status`, `help`, `quit`.

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
- `src/modes/` — `GameMode` base; `CapturePointMode` (5v5, capture progress,
  5s scoring ticks, first to 100); `MobaDefense` (lanes, minion waves,
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
  `RaidCity`, `ConvertCampaign`, `MassSacrifice`, `Defend`, `GatherRelic`)
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
  timer picks an ambient action (Pray, Patrol, Gather, Preach, Brawl,
  Desecrate), weighted by beliefs, morale, and time of day
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
- Capture-the-point: capture rate 0.25/s per net occupant; +1 score per owned
  point every 5s; first to 100 wins.
- MOBA: waves every 30s (3 melee + 1 ranged; every 3rd wave + siege), 2
  towers per team per lane (600 HP).
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
decays toward baseline at 0.5/s when neglected). Active beliefs accumulate
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
| Completed sacrifice ritual | Sacrifice +12 |
| Interrupted sacrifice ritual | Sacrifice −5 |
| Conversion (soul) | Conversion +6 |
| Mass conversion (200s cd) | Conversion +10 |
| Sermon / dream-whisper | Conversion +2/+1 |
| Enemy cultist slain in war | War +6 |
| Building auto-rebuilt / heal | Reconstruction +2/+1 |
| Mimic kill / sprung trap / cursed artifact | Trickery +8/+6/+4 |
| Sorcerer spell cast / necromancy | Magic +3/+10 |
| Civilian slain / melee attack | Onslaught +3/+1 |
| Cultist rests / prays / dream-whisper / nightmare | Dreams +2/+1/+1/+3 (nightmare also Chaos +2) |
| Directive obeyed (Raid→Fear, Convert→Conversion, Sacrifice→Sacrifice, War→War, Defend→Reconstruction, Relic→Magic) | aligned belief +8 (partial +4) |
| Directive refused / sparks insurrection | Chaos +4 / +8 |

### Interaction matrix (12×12)

When two beliefs both exceed **50 exertion** they interact:

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
> 50; both > 70 fires a `BeliefTension` event + **+1.5** insurrection risk,
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
