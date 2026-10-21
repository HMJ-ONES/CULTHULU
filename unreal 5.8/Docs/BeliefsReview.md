#Latest update: [Initial beliefs, entity preaching, and ALT armies](ArmyCommands.md). Player combat orders bypass loyalty and preserve separate fronts; building still checks loyalty and doctrine.

 Beliefs: implementation and review guide

Latest command/menu update: [Initial beliefs, entity preaching, and ALT armies](ArmyCommands.md). Player combat orders now bypass loyalty and preserve separate fronts; building still checks loyalty and doctrine.

Latest combat, corpse, movement, PvE health and HUD changes: [SkillCombatReview.md](SkillCombatReview.md).

Population follow-up: see `LivingCultSociety.md` for autonomous work groups, original NPC art/animation, the live contribution display, and the new +2 Reconstruction feed from actual construction completion.

2026-10-16. Active project: `unreal 5.8/CultUlhu.uproject` (UE 5.8.3).

## Open the review

Play → Free Roam → select an entity → choose three initial beliefs → enter the world. **B** opens/closes Beliefs and Cult Life. Hold **ALT** to use the cursor, then click a belief. The mouse wheel scrolls its full description, event feeds, and interactions. Aim at a person, captive, creature, ground, or altar before opening the panel to use its action buttons. Releasing ALT returns camera control. The simulation continues while the panel is open.

The left column shows all twelve fervor meters, active creeds, and remaining adoption times. The top shows power, fear, insurrection risk, combat strength, conversion chance, loyalty drift, sleepers, and sorcerer ritual attempts/successes. The journal records real events and resulting power. Cultists have world labels showing their role, current work, and loyalty; their body poses change while resting.

## Source authority

Before changes, twelve complete local files matched the live **HMJ-ONES/CULTHULU, game-core** source. Exact paths and Git blob hashes are saved in `BeliefsGitHubAudit.json`. Relevant source families also reviewed: `cult`, `commands`, `chaos`, `combat`, `exertion`, `ai`, `dreams`, `rituals`, `breeding`, `traps`, `entities`, and `ui`.

The executable source takes priority over older prose. Current source values include Torture **6.4**, Fear raid power **14.4**, Sacrifice **30**, Onslaught civilian power **3.6** and idle limit **1200s**, and DreamWhisper **13**. Older README values such as 8/12/25/3/3600 were not copied back into the game. No entity abilities were invented or replaced.

All power rewards below are **base** values. The source scales event income and losses by active belief count: one belief ×1, two ×0.75, three ×0.5, then applies relevant synergies/conflicts. Rest and time effects retain their source pathways. A displayed base reward therefore may differ from the actual journal balance change.

## The shared feedback loop

1. A cultist chooses an action using the original sixteen-action weighted table. Current creeds, loyalty/morale, and time of day affect those weights. Orders can interrupt idle work.
2. Native movement brings the cultist to a real audience, patient, sparring partner, or quarry. Pair actions require a partner within 6m; audience work must reach within 5.5m. Ambient work takes 30 seconds at its destination. Hunting requires an actual quarry to die.
3. Completed work emits the original typed event once. Brawls, sparring, blood rites, and tending project their real health and loyalty changes back to the relevant actors after the entire core update. A later snapshot cannot erase the change.
4. `ActionExertionTable` changes one or more fervors. Active beliefs receive full feeds; inactive beliefs receive half. Fervor stays between 0–100, starts at 10, and moves toward 10 at 0.05/s when neglected.
5. Fervor changes combat strength, loyalty drift, conversion success, construction preferences, and interaction effects. Those changed conditions influence later actions and order outcomes.

Each of the three factions owns a separate event bus, clock, random generator, beliefs, cult roster, fervor, dreams, rituals, war, construction, and power. Rival events do not feed the human faction's creed. Wars are symmetric and explicitly between the participating factions.

### Adoption and disobedience

At most three creeds can be active or pending in total. Choosing a replacement removes the outgoing creed immediately and begins a 120-game-second adoption. Chaos doubles progress. Pending slots are reserved, so a fourth pending adoption is rejected instead of silently disappearing.

Punish only works on an actual infringer or lunatic. An infringer's punishment halves remaining adoption times and adds 10 risk; Chaos adds another 10. Source lunatic alignment strikes the lunatic down and adds its specified risk. Brawling and desecration can make a follower an infringer when their action feeds a creed conflicting with a professed belief. Imprisoned, dead, converted-away, captive, and lunatic people do not perform normal ambient work.

The original command outcome calculation still uses the cult's loyalty, doctrines, exertion, and risk; orders may be obeyed, partially obeyed, refused, or spark insurrection. Low devotion below 20 raises risk at the source's 0.2/s rate. Source revolts remove the least devoted quarter. Native actors then desert, and stable IDs prevent shifted roster indices from affecting the wrong person.

## Review each belief

| Belief | What cultists/players actually do | Reward or cost and behavioral consequence | First review exercise |
|---|---|---|---|
| Torture | Capture a civilian, creature, or enemy cultist; select Torture. Blood rites and desecration also occur autonomously. | Civilian/creature +6.4; captured enemy cultist +5; own cultist torture grants nothing. An own cultist killed in one full-health hit costs 12. Torture events also feed Fear. Each captive yields its per-victim reward once. | Capture a creature using ALT orders, escort it home, aim at it, use Torture, and compare the journal and fervors. A second click must not produce a second per-victim reward. |
| Fear | Raid real city buildings, paint sigils, keep captured beasts or loyal bred hybrids, lose followers, suffer defeat. | Raid +14.4 × destroyed fraction and +20 fear × fraction; night fear ×1.25. A living army source adds 1 fear/s in combat under the current source tick. Outside combat fear fades 1.5/s with its source power drain. Lost cultist −6 power/fear; defeat −60 and fear resets. | Destroy one city building and compare a partial raid with a whole district. Compare in/out of combat and day/night. Remove an owned creature and check that its army fear stops. |
| Breeding | Bring two captured, owned beasts within 100m of an intact owned altar; use Breed. | Source Beast×Beast compatibility, +15 per birth. The 300-HP child becomes a real loyal ally or feral enemy. Base feral chance 15%, reduced by up to half by Breeding skill/fervor. Feral offspring can kill cultists; each such loss costs 10. | Capture two beasts, bring them to the temple, adopt Breeding, and create a hybrid. Review the source probability and observe its actual allegiance. |
| Chaos | Brawl, desecrate, destroy structures, imprison followers, punish infringers; lunatics undermine other creeds. | Adoption ×2; destructive explosion +6; imprisonment −4; extra punishment risk +10. Brawls risk +2 and may injure an actual nearby follower. Lunatics can steal a real follower for another faction, release a prisoner/beast, damage a real shrine, or interrupt an active sacrifice. | Adopt Chaos with a conflicting creed, observe brawls and loyalty/risk, then punish an infringer during adoption. Confirm the reduced timer and increased risk. |
| Sacrifice | Place a free performer and real offered cultist/captive within 12m of a living owned altar; start a 30s rite. | Completion +30 and the offered person really dies; interruption −15. Movement away, damage, stun, lost participants, incompatible new orders, or altar destruction cancels it. A lunatic interruption applies the source event once. | Complete one rite, then start another and move or injure its performer. The second must stop, preserve a living victim, and emit one interruption. |
| Conversion | Preach to a real civilian, whisper rumors, run original sorcerer rituals, issue conversion orders, use MassConvert. | +4 per actual converted soul; campaign efficiency ×1.5; believers are ×1.5 more vulnerable to rival conversion. MassConvert requires three living civilians, converts exactly three, and waits 200s before another use. | Use MassConvert and count three actual faction changes. Then let a sermon or directed ritual recruit one real audience member. Empty worlds cannot generate phantom converts. |
| War | Fight enemy cultists in a declared war, spar, defend actual owned structures. | Enemy cultist killed: +10 with one war or +4 with multiple; own base damage/destruction −5/−20. War fervor raises derived combat strength. Sparring gives both real partners +2 loyalty, with the source injury chance. | Order a rival attack, compare human/rival allegiance and separate war states, then test a base hit and a kill. |
| Reconstruction | Tend a real nearby wounded follower, mend effigies, heal with an existing entity kit, rebuild surviving owned structures. | Kit healing ×1.5; other source damage/spell effectiveness ×0.75. Actual heal +0.5; an owned surviving damaged building automatically repairs after 60s and emits BuildingRebuilt (+2). Tending gives its original 18 HP or 30 with fervor ≥50, plus 3 loyalty. | Wound a cultist, observe tending until completion and persistent healing. Damage a shrine and wait 60 game seconds. Compare Onslaught/War conflicts. |
| Trickery | Spread rumors near real civilians; place a single-use physical trap. | A real trap crossing captures a living civilian and grants +6; Fear adds +5 fear. Rumors feed Trickery/Fear and may convert their actual audience. A destroyed or already-sprung trap cannot repeatedly generate outcomes. | Place a trap in a civilian route, wait for a crossing, verify actual capture and a single event; then test a rumor campaign. |
| Magic | Chant, use the selected character's original spells, let real cult sorcerers conduct conversion rituals, perform 45s necromancy. | Sorcerer damage ×1.3; incoming CC duration ×0.6 on the defender. Necromancy +10. Sorcerer rituals cost 25 mana, occur at 45s intervals with a real nearby audience, and use the original finite 100 mana. SpellCast is emitted once only for successful casts. | Observe chanting/ritual counts, then finish necromancy at an altar. Compare identical crowd-control effects on Magic and non-Magic defenders. |
| Onslaught | Kill real civilians, make melee attacks, spar, hunt actual creatures. | Civilian death +3.6; melee ×1.25. After 1200s without a civilian kill, power drains 2/s. Killing buildings or monsters cannot earn civilian credit. Fervor boosts combat and interacts with War. | Compare a civilian kill with a beast/building kill. Only the civilian should create CivilianSlain; melee and hunting have their own feeds. |
| Dreams | Rest autonomously or by order, share dreams with nearby followers, read omens; sleepers may convert distant actual civilians. | Each resting follower 0.5 power/s, ×1.5 at night. Whisper chance 0.002/s per sleeper, +13 on success plus real conversion. Nightmare chance 0.001/s, Chaos ×3 and Nightmare Surge ×2, wakes the actual sleeper as a lunatic. | Order Rest, watch the sleeper count and power, then give another order: the sleeper must leave the roster. Observe dream sharing, whispers, and nightmares over time. |

## Interaction matrix

Both relevant fervors must be **above 25**, including residual inactive fervor. Selecting two beliefs alone does not automatically activate their pair.

| Synergy | Pair | Source effect |
|---|---|---|
| Terror | Fear + Torture | +25% torture and raid power |
| Nightmare Surge | Dreams + Chaos | Nightmare chance ×2 |
| Blood Frenzy | War + Onslaught | Derived combat strength +0.15 |
| Dark Rites | Sacrifice + Magic | Necromancy reward +50% |
| Infiltration | Conversion + Trickery | Conversion chance +0.10 |
| Dread Broods | Breeding + Fear | Passive fear +0.8/s |
| Shock and Awe | War + Fear | Raid power +25% |
| Martyrs' Visions | Sacrifice + Dreams | Dream conversion chance ×1.5 |
| Oneiromancy | Magic + Dreams | Sorcerer ritual conversion chance +0.05 |
| Cruelty Unbound | Torture + Chaos | Torture power +25% |

Conflicts: Reconstruction/Onslaught (**Ashes and Scaffolds**), Chaos/Conversion (**Sabotage**), Chaos/Sacrifice (**Desecrated Rites**), Reconstruction/War (**Builders vs Warriors**), Torture/Sacrifice (**Waste Not**). Hot conflicting beliefs halve positive gains into the affected fervor. Sabotage subtracts 0.10 conversion chance; Desecrated Rites reduces sacrifice power by 25%. A pair above **35** generates +1.5 insurrection risk with a 60s per-pair cooldown. The panel derives the displayed pairs and active state directly from `InteractionMatrix`.

## Integration choices and remaining source hooks

The source defines balance and probabilities; the following are explicit Unreal binding policies: physical ambient work/ordered rest lasts 30s; idle Dreams followers consider rest every 90s; combat is held for eight seconds after a combat action; the visible day/night cycle is ten real/game minutes; actual city-building destruction contributes `1 / city building count` raid destruction. Idle construction is considered every 15–25s, and four real workers must reach within 6m of a site. Workers require source loyalty ≥40 and halt at source unrest ≥70. Existing native construction caps remain. These are documented policies connecting source systems to this world, not new entity powers.

All twelve beliefs have live actions, modifiers, fervor, and review information. These advanced workflows remain explicit gaps rather than pretend-complete mechanics:

- Native disguised mimic combat is not yet bound; source MimicKill/MimicSlain hooks stay +8/−8. Trickery is currently exercised through real traps and rumors.
- Source Magic learning-time ×1.5 is retained, but the source has no complete native training curriculum to bind. No training schedule was invented.
- Sacrifice death denial is connected to the original `CultManager::denyDeath` path when a Converted volunteer exists. A player-facing volunteer selection workflow is still missing; Converted also means turned-away elsewhere in the source, so it needs a deliberate design review before exposing that choice.
- Native breeding currently exposes Beast×Beast only. Other source compatibility pairs and a parent-selection UI need a further pass. Summoned/hybrid bodies remain art proxies. The subsequent population pass imports the repository's original cultist and civilian art.
- Advanced abstract directive families (blighting, assassination, grand summoning, relic channels, oneiric harvesting, sanctum projects), zonal ambient fear, and attracted-creature choreography need dedicated native implementations. Their original source events remain visible in the catalog; listing an event feed does not claim that its world feature is implemented.
- The live faith state is session state. Belief selection, pending timers, fervors, ritual participants, and the journal are not yet persisted as a complete Unreal save game.

## Code and verification

Native integration lives in `Source/CultUlhu/Private/CultFaithGameplay.cpp`, `CultFaithActions.cpp`, `CultWorldState.h`, `CultBeliefCatalog.cpp`, and `CultBeliefHUD.cpp`, with hooks in the world director, population, beasts, structures, kits, and player pawn. `B` is an inspection/action panel; ALT keeps its original cursor behavior.

Core command automatic simulation rewards are disabled only in the physical binding: issuing an obeyed raid no longer awards imaginary 40% destruction, and an abstract campaign cannot spawn five roster-only converts later. Actual native work produces those events. Original abstract command defaults remain unchanged.

Core additions preserve abstract simulation defaults: optional real-target filters/pickers, defender-side CC lookup, target conversion vulnerability, living army fear count, and consumption of an already-published ritual interruption. Bug fixes reserve pending adoption slots, emit successful spell/heal events, and type spell kills by actual victim. The canonical action table, interaction matrix, character kits, base reward values, and source probabilities are unchanged.

`Tools/BuildCanonicalCore.ps1 -Test` runs eighteen suites, including `tests_belief_binding.cpp`. `CultUlhu.Beliefs` exercises the Unreal actor binding; existing ground/traversal, creature, canonical kit/targeting, and living-world tests cover regressions. `CultUlhu.Menu.Beliefs` opens the real PIE panel and captures `Saved/Screenshots/CultBeliefsReview.png`. Final run results are recorded below after verification.

### Verified results

- Development Editor build succeeded against the rebuilt canonical library.
- All **18 core suites** passed. The dedicated belief integration executable reported **157 checks, 0 failures**.
- All **5 game-world suites** passed together on the final build in `Saved/Logs/BeliefsFinalGameplay.log`: Beliefs, CanonicalGameplay, GroundSpawn, LivingWorld, MinorCreatures. No automation errors or warnings were reported. Tests included real actor healing, rest, adoption slots, exactly three mass converts, trap capture, per-victim torture, imprisonment, hybrid birth, completed/interrupted sacrifice, 45s necromancy, owned building repair, typed civilian death, conversion/desertion mirrors, and an actual source revolt.
- Ground traversal still passed, including jump/landing and recovery from outside the map. Canonical kit/camera impact and cavern summoning checks passed. The final living-world run retained built temples and clustered cities.
- The command-line Unreal log also contains pre-existing experimental engine Python-toolset startup errors (`AgentSkill`/`PythonTestRunner` unavailable in game mode). These are separate from the passing game automation tests; the normal editor MCP and UI tests work.
- A later full five-suite release run also passed in `Saved/Logs/BeliefsReleaseGameplay.log`, including real-volunteer death denial. The final targeted `CultUlhu.Beliefs` rerun passed in `BeliefsLifecycle.log` after adding an exactly-once converted-follower loss assertion.
- The editor's standalone `CultUlhu.Menu.WorldAutonomy` test genuinely observed **100 game seconds** and passed in `Saved/Logs/BeliefsFinalEditor.log` (103s test duration). Both rivals completed structures, as did the human faction; recruitment work and non-player goals were observed. Automation reported zero errors and warnings.
- An earlier combined menu run exposed a false-positive timeout in the pre-existing autonomy test harness. That result is not counted as a world observation. The harness now reports a world-load timeout as an error and waits for PIE startup before selecting a mode; the standalone run above exercised the actual world.
- The verified autonomy result was **2 completed rival structures, 8 active builders, and 10 converters**. The final standalone belief-panel test passed with zero automation errors/warnings; its screenshot was visually inspected after enlarging text and measuring wrapping. Current image: `Saved/Screenshots/CultBeliefsReview.png`. The editor remains on `L_MainMenu`, ready for normal Play → mode → character selection.
