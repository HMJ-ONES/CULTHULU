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

Tests: 306 checks (87 v0.1 + 68 wave 2 + 70 wave 3 + 81 beta), 0 failures.

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
- **Networking**: authoritative server sim using this core; replicate
  `GameEvent`s to clients.
- **Animation pipeline**: Mixamo auto-rig + animation library / Blender
  Python (`bpy`) scripted animation generation on player-supplied rigged
  FBX models; retarget into engine Animation Blueprints.
- **Content**: per-entity tuning, full 5v5 matchmaking flow, save/load in
  the driver, more ambient variety and directive types.
