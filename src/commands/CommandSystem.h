#pragma once

#include "beliefs/Belief.h"
#include "beliefs/BeliefSystem.h"
#include "core/EventBus.h"
#include "core/Events.h"
#include "core/RNG.h"
#include "core/Vec3.h"
#include "cult/CultManager.h"
#include "entities/Entity.h"

#include <string>

namespace cultulhu {

// Eldritch directives the player issues to the whole cult. Issuing a
// directive never guarantees obedience: the cult is a living thing and its
// compliance depends on devotion, insurrection risk, chaos, distance and
// which beliefs are currently active.
enum class DirectiveType {
    GoToWar,
    RaidCity,
    ConvertCampaign,
    MassSacrifice,
    Defend,
    GatherRelic,
    // Wave 9b: three new directives. See DirectiveExecutor.h for their
    // follow-through operations.
    AssassinateProphet, // kill an enemy leader via infiltration
    BlightLand,         // corrupt a zone over many ticks
    GrandSummoning,     // long ritual: 300 power to summon a champion
    // Wave 15: two new directives.
    OneiricHarvest,     // mass dream-rite: channel dream-visions into power
    RebuildSanctum,     // repair the sanctum: rebuild + cult devotion bump
    Count
};

const char* directiveName(DirectiveType d);

enum class CommandOutcome {
    Obeyed,
    PartiallyObeyed,
    Refused,
    SparksInsurrection
};

const char* commandOutcomeName(CommandOutcome o);

struct CommandResult {
    CommandOutcome outcome;
    float obedienceChance;
    std::string detail;
};

class CommandSystem {
public:
    CommandSystem(EventBus& bus, RNG& rng, BeliefSystem& beliefs,
                  CultManager& cult);

    // Issue a directive toward 'target' (and optionally an enemy faction).
    // Rolls the obedience model, publishes DirectiveIssued/Resolved plus any
    // consequence events, and applies automatic side effects for Obeyed.
    // Wave 9b: targetFaction is recorded on the DirectiveResolved event so
    // follow-through operations (assassination, war) know the enemy.
    CommandResult issueCommand(DirectiveType d, Vec3 target,
                               FactionId targetFaction = FACTION_NEUTRAL);

private:
    // Number of cultists that can receive commands (alive, non-Converted).
    size_t commandableCount() const;

    // Obedience chance in [0.05, 0.95] for the current cult state.
    float obedienceChance(DirectiveType d, Vec3 target) const;

    void publishIssuedResolved(DirectiveType d, CommandOutcome o, float chance,
                               FactionId targetFaction);

    EventBus& bus_;
    RNG& rng_;
    BeliefSystem& beliefs_;
    CultManager& cult_;
};

} // namespace cultulhu
