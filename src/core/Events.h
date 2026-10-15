#pragma once

#include "core/Vec3.h"

#include <cstdint>
#include <string>

namespace cultulhu {

// Every gameplay-relevant occurrence flows through the EventBus as a
// GameEvent. Belief rules react to these events (see BeliefSystem).
enum class EventType {
    // Torture belief
    TorturePerformed,     // amount = number of civilians/creatures tortured
    CapturedTortured,     // amount = captured cultists/creatures tortured
    OwnCultistTortured,   // torturing own cultists: no benefit, no extra risk
    CultistOneHitKilled,  // own cultist one-hit killed: power loss

    // Fear belief
    RaidPerformed,        // amount = destruction 0..1
    CapturedInArmy,       // amount = captured creatures / bred monstrosities in army
    CombatStarted,
    CombatEnded,
    Defeated,             // Cthulhu defeated
    CultistLost,

    // Breeding belief
    MonstrosityBred,
    FeralRampage,
    CultistKilledByFeral,

    // Belief adoption / insurrection
    BeliefAdopted,
    InfringerPunished,
    InsurrectionRiskUp,   // amount = risk added
    Revolt,

    // Chaos belief
    Explosion,
    CultistImprisoned,

    // Sacrifice belief
    SacrificeCompleted,
    SacrificeInterrupted,

    // Conversion belief
    ConversionPerformed,  // amount = number converted
    MassConversionUsed,

    // War belief
    EnemyCultistSlain,    // faction = enemy deity faction
    BaseBuildingDamaged,
    BaseBuildingDestroyed,

    // Reconstruction belief
    HealPerformed,        // amount = HP healed
    BuildingRebuilt,

    // Trickery belief
    MimicKill,            // civilian/adventurer slain by a mimic
    MimicSlain,
    TrapSprung,           // captive trapped

    // Magic belief
    NecromancyPerformed,
    MagicLearned,         // amount = learning time (seconds)
    SpellCast,            // tag = school; amount = base effectiveness

    // Onslaught belief
    CivilianSlain,
    MeleeAttack,          // amount = base damage

    // Relics / artifacts (wave 2)
    ArtifactTriggered,

    // City destruction (wave 3)
    CityBuildingDestroyed, // amount = district ruin 0..1 after the hit
    DistrictRazed,         // a city district reached 100% ruin

    // Sacrifice belief: a converted individual can deny the death of one of
    // Cthulhu's cultists by serving as a sacrifice in their place.
    DeathDenied,           // amount = 1 per denial

    // Chaos belief: lunatics acting against the other active beliefs
    LunaticActed,          // tag = misbehavior id (e.g. "interruptedRitual")
    LunaticAligned,        // a lunatic was killed/punished to align them

    // Dreams belief (12th): rest, dream-visions, nightmares
    RestStarted,           // sourceId = cultist id
    RestEnded,             // sourceId = cultist id
    DreamWhisper,          // a distant civilian stirred by dream-whispers
    Nightmare,             // sourceId = cultist who woke Lunatic

    // Belief exertion (wave 4): two conflicting beliefs both burning hot.
    BeliefTension,         // tag = "A vs B"; amount = tension level 0..1

    // Crowd control
    CCApplied,             // micro-dev (2026-10-13): a stun/root landed on a
                           // match fighter — sourceId = victim id, tag =
                           // victim name, faction = victim team,
                           // amount = CC duration seconds

    // Ambient cultist behavior
    PrayerOffered,         // amount = power from prayer
    SuppliesGathered,      // amount = supplies gathered
    SermonPreached,        // amount = souls converted (0/1)
    BrawlBrokeOut,         // sourceId/targetId = the brawlers
    DesecrationDone,       // amount = 1

    // Eldritch directives
    DirectiveIssued,       // tag = directive name; amount = obedience chance
    DirectiveResolved,     // tag = "DirectiveName/OutcomeName"; amount = obedience chance
    DirectiveProgress,     // tag = directive name; amount = progress 0..1
    DirectiveCompleted,    // tag = directive name; the operation ran its course

    // World / construction (wave 7)
    BuildStarted,          // tag = building type name; sourceId = site id
    BuildProgress,         // tag = building type name; amount = progress 0..1
    BuildCompleted,        // tag = building type name; sourceId = site id
    AltarUpgraded,         // sourceId = altar id; amount = new tier
    RitualAtAltar,         // sourceId = altar id; tag = ritual name
    CaptiveDelivered,      // sourceId = captive id; targetId = altar id
    DungeonEntered,        // sourceId = entity id; targetId = dungeon id
    DungeonExited,         // sourceId = entity id; targetId = dungeon id
    DungeonCompleted,      // targetId = dungeon id; boss slain, dungeon cleared

    // Cthulhu Avatar RMB: "Wave of Domination" (wave 7).
    VictimLevitated,      // sourceId = caster; targetId = victim caught by the wave
    VictimSlammed,         // sourceId = caster; targetId = victim; amount = damage; tag = "slam"/"launch"
    VictimsLaunched,       // sourceId = caster; amount = victim count
    VictimDropped,         // sourceId = caster; targetId = victim; amount = 1 fell to death, 0 survived
    DirectStunApplied,     // sourceId = caster; targetId = victim; amount = stun seconds

    // Wave 9c: new ambient cultist activities.
    OmenRead,             // sourceId = cultist; amount = power from reading portents
    SparringHeld,         // sourceId/targetId = the sparring pair; amount = 1
    SigilPainted,         // sourceId = cultist; amount = 1; pos = sigil location
    ChantingHeld,         // sourceId = cultist; amount = 1
    CreatureAttracted,    // sourceId = cultist; tag = species lured by the chant

    // Wave 9c: dungeon hazard triggers.
    SpikePitSprung,       // sourceId = entity id; targetId = dungeon id; amount = damage
    CaveIn,               // sourceId = dungeon id; targetId = entity id;
                          // amount = damage; tag = "sealed" or "rubble"

    // Wave 15: five new ambient activities.
    DreamShared,          // sourceId = cultist; amount = 1 (2 when Dreams
                          // exertion runs hot, >= 50); a shared dream-vision
    EffigyMended,         // sourceId = cultist; amount = 1
    RumorSpread,          // sourceId = cultist; amount = 1; pos = location
                          // the false rumor was planted at
    RiteOfFlesh,          // sourceId = cultist; amount = 1
    WildsHunted,          // sourceId = cultist; amount = meat brought back

    // Wave 13: Vale of Pnath hazards & encounters.
    AbyssPitFall,         // sourceId = entity id; targetId = dungeon id;
                          // amount = fall damage (scales with depth)
    MaddeningWhispers,    // sourceId = dungeon id; targetId = entity id;
                          // amount = fear inflicted (scales with dread)
    DholeTremors,         // telegraph: sourceId = dungeon id;
                          // targetId = room index (as uint64); the next
                          // traversal of that room triggers the ambush
    DholeAmbush,          // sourceId = dhole entity id; targetId = victim id;
                          // amount = damage; tag = "ambush"
    ValeRelicClaimed,     // sourceId = entity id; targetId = dungeon id;
                          // the cursed relic at the Vale's bottom was seized

    // Wave 15: two new directives.
    OneiricHarvestCompleted, // OneiricHarvest done: amount = dreamers that
                          // channeled the rite; tag = directive name;
                          // pos = rite site
    SanctumRebuiltTick,   // RebuildSanctum progress tick: amount = progress
                          // 0..1; tag = directive name; pos = sanctum site
    SanctumRebuilt,       // RebuildSanctum done: tag = directive name;
                          // pos = sanctum site

    // Wave 9b: three new directives.
    LeaderAssassinated,   // AssassinateProphet success: sourceId = slain
                          // leader's entity id (0 when unknown);
                          // faction = enemy faction; amount = 1
    EnemyMoraleShocked,   // the enemy cult's morale breaks: faction = enemy
                          // faction; amount = shock duration in seconds.
                          // Conversions against them are easier while the
                          // Conversion-exertion it feeds stays high.
    AssassinExposed,      // AssassinateProphet failed: the infiltrator was
                          // uncovered. sourceId = assassin's cultist id;
                          // tag = directive name; pos = assassination site.
                          // The target escapes.
    ZoneBlightTick,       // BlightLand corruption tick: tag = zone name;
                          // amount = blight progress 0..1; pos = zone center.
                          // The city system reads this to scale civilian
                          // output down while the blight spreads.
    ZoneBlighted,          // BlightLand completed: tag = zone name; amount =
                          // 1 when the zone registry flagged it persistent,
                          // 0 when no zone registry was attached.
    SummoningInterrupted,  // GrandSummoning failed: tag = directive name or
                          // the reason ("interrupted", "insufficient_power").
                          // Spent power is NOT refunded.
    ChampionSummoned,      // GrandSummoning completed: a champion answers.
                          // sourceId = 0 (the game layer spawns the entity);
                          // tag = champion kind; amount = champion max HP;
                          // pos = summoning site.

    // Wave 16: achievements — new instrumentation events.
    RelicClaimed,        // sourceId = claimer entity id; amount = amplifier;
                         // tag = relic name/kind ("" when unnamed)
    // Wave 34: a tier-3 MOBA relic manifests — tag = name, pos = site,
    // amount = tier. Global announcement: fight for it.
    RelicManifested,
    RelicChannelStarted, // micro-dev (2026-10-12): a tier-2/3 MOBA jungle
                         // relic channel just began — the 4s/6s channel
                         // was silent, so contesters never knew someone
                         // was already claiming it. sourceId = channeler
                         // player id; faction = channeler's team;
                         // tag = relic name; amount = tier; pos = site.
    RelicChannelInterrupted, // micro-dev (2026-10-13): a tier-2/3 channel
                             // was broken mid-cast (channeler left the
                             // radius or died) — the channeler got no
                             // feedback that their progress died.
                             // sourceId = interrupted channeler player id;
                             // faction = channeler's team; tag = relic name;
                             // amount = seconds channeled; pos = site.
    MonstrositySlain,    // targetId = monstrosity id; faction = its faction;
                         // tag = species (e.g. "dhole")
    CityDestroyed,       // tag = city name; amount = 1; all districts razed
    PlayerKilled,        // sourceId = killer entity id; targetId = victim
                         // entity id; both are EldritchAvatars (PvP)
    FirstBlood,          // micro-dev (2026-10-12): the match's first player
                         // death — once per match, whatever the cause.
                         // sourceId = killer entity id (a player id, or a
                         // minion/tower id when the killer wasn't a player);
                         // targetId = victim player id
    StreakEnded,         // micro-dev (2026-10-12): a 3+ kill streak was cut
                         // short — the classic shutdown moment. Published
                         // once per qualifying death, before the victim's
                         // streak resets. sourceId = killer entity id
                         // (player id, or minion/tower id); targetId =
                         // victim player id; amount = the ended streak
                         // count; tag = killer player name (empty when the
                         // killer wasn't a player)
    PlayerRespawned,     // micro-dev (2026-10-12): a dead player returns —
                         // published when the respawn timer expires and the
                         // player revives (initial spawns at match start or
                         // `match join` are silent). sourceId = player id;
                         // tag = player name; faction = team
    PlayerLowHp,         // micro-dev (2026-10-12): a fighter dropped below
                         // 35% HP — one-shot per crossing (re-armed by
                         // healing above 50% or by respawning). sourceId =
                         // player id; faction = team; amount = hp fraction;
                         // tag = player name; pos = position
    AbilityCast,         // wave 40: sourceId = caster id; tag = ability name;
                         // amount = slot (0=Q 1=F 2=R 3=RMB)
                         // micro-dev (2026-10-15): pos = caster's position,
                         // stamped at every publish site so the engine's
                         // cast FX lands at the fighter, not the origin
    AbilityUnlocked,     // wave 41: sourceId = player id; tag = unlocked
                         // ability in DOTGOO draft — "Q (name)"/"F (name)"/
                         // "ULT (name)" (micro: names the ability)
    KillStreak,          // wave 42: sourceId = player id; amount = streak
                         // count (2+=double kill); tag = player name
    PlayerLeveledUp,     // 2 PM build (2026-10-12): sourceId = player id;
                         // amount = new level; tag = player name; published
                         // once per addXp call even on multi-level jumps
    PointCaptured,       // targetId = point index; faction = capturing team
    GreatOldOneSlain,    // sourceId = killing-blow entity id (0 unknown);
                         // faction = fallen GOO's team
    GreatOldOneUnderAttack, // micro-dev (2026-10-12): a GOO is taking
                         // damage — published on the first scratch and
                         // each 75/50/25% HP crossing; faction = defending
                         // team; sourceId = attacker id (0 unknown/minion);
                         // amount = hp fraction 0..1 after the hit;
                         // pos = base pos
    TowerDestroyed,      // micro-dev (2026-10-12): a DOTGOO tower crumbled —
                         // sourceId = killing-blow entity id (0 when minions
                         // or siege did it); faction = fallen tower's team;
                         // amount = lane index; tag = lane name
    TowerUnderAttack,    // micro-dev (2026-10-13): a DOTGOO tower is taking
                         // damage — published on the first scratch only
                         // (towers never heal, so the first hit is once-only
                         // by construction); faction = defending team;
                         // sourceId = attacker id (0 unknown/minion);
                         // amount = hp fraction 0..1 after the hit;
                         // tag = lane name; pos = tower pos
    MatchStarted,        // tag = mode name ("capture", "moba")
    MatchPoint,          // micro-dev (2026-10-12): Domination — a team
                         // first crossed MATCH_POINT_SCORE; faction = team;
                         // amount = threshold crossed
    OvertimeStarted,     // micro-dev (2026-10-12): Domination — the 600s
                         // clock expired on a tie, sudden death begins.
                         // Published once; first score ends the match
                         // (120s cap, then a draw).
    UltCharged,          // micro-dev (2026-10-12): Domination — a fighter's
                         // ult finished its opening full-cooldown charge and
                         // is ready to cast. sourceId = player id;
                         // tag = ult ability name; faction = team.
                         // Published once per match per player (the flag is
                         // consumed — casting the ult and re-charging it
                         // never re-fires).
    MatchEnded,          // tag = mode name; faction = winner team (-1 draw)
    AchievementUnlocked, // tag = achievement id; the dark takes note

    // Wave 26: discovery codex — first-time finds in the world.
    // Wave 31: pure journal, no power granted.
    DiscoveryMade,   // first discovery logged; tag = discovery id
                     // ("kind:slug"); faction = 1 when found at night,
                     // else 0; pos = site

    // Wave 18: behavior-hook animation events.
    SacrificeStarted,    // a sacrifice rite begins: sourceId = performer
                         // (priest) entity id; targetId = victim entity id
    MaulStruck,          // a monstrosity/feral beast mauled a human:
                         // sourceId = attacker id; targetId = victim id
    WarEngagement,       // disciplined war fighting (not a brawl, not a
                         // beast maul): sourceId = attacker entity id;
                         // targetId = victim entity id;
                         // faction = attacker's faction

    Count
};

struct GameEvent {
    EventType type = EventType::Count;
    uint64_t sourceId = 0;
    uint64_t targetId = 0;
    int faction = -1;          // faction id context (e.g. slain enemy's deity)
    float amount = 0.0f;       // generic numeric payload
    Vec3 pos;                  // world position context
    std::string tag;           // free-form string payload

    GameEvent() = default;
    explicit GameEvent(EventType t) : type(t) {}
};

} // namespace cultulhu
