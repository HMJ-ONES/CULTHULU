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
    CCApplied,             // tag = CC type name; amount = duration seconds

    // Ambient cultist behavior
    PrayerOffered,         // amount = power from prayer
    SuppliesGathered,      // amount = supplies gathered
    SermonPreached,        // amount = souls converted (0/1)
    BrawlBrokeOut,         // sourceId/targetId = the brawlers
    DesecrationDone,       // amount = 1

    // Eldritch directives
    DirectiveIssued,       // tag = directive name; amount = obedience chance
    DirectiveResolved,     // tag = "DirectiveName/OutcomeName"; amount = obedience chance

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
