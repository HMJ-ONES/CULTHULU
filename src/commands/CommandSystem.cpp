#include "commands/CommandSystem.h"

#include <algorithm>
#include <cstdio>

namespace cultulhu {

const char* directiveName(DirectiveType d) {
    switch (d) {
        case DirectiveType::GoToWar:         return "GoToWar";
        case DirectiveType::RaidCity:        return "RaidCity";
        case DirectiveType::ConvertCampaign: return "ConvertCampaign";
        case DirectiveType::MassSacrifice:   return "MassSacrifice";
        case DirectiveType::Defend:          return "Defend";
        case DirectiveType::GatherRelic:     return "GatherRelic";
        case DirectiveType::AssassinateProphet: return "AssassinateProphet";
        case DirectiveType::BlightLand:      return "BlightLand";
        case DirectiveType::GrandSummoning:  return "GrandSummoning";
        case DirectiveType::OneiricHarvest:  return "OneiricHarvest";
        case DirectiveType::RebuildSanctum:  return "RebuildSanctum";
        case DirectiveType::Count:           return "Count";
    }
    return "Unknown";
}

const char* commandOutcomeName(CommandOutcome o) {
    switch (o) {
        case CommandOutcome::Obeyed:            return "Obeyed";
        case CommandOutcome::PartiallyObeyed:   return "PartiallyObeyed";
        case CommandOutcome::Refused:           return "Refused";
        case CommandOutcome::SparksInsurrection:return "SparksInsurrection";
    }
    return "Unknown";
}

CommandSystem::CommandSystem(EventBus& bus, RNG& rng, BeliefSystem& beliefs,
                             CultManager& cult)
    : bus_(bus), rng_(rng), beliefs_(beliefs), cult_(cult) {}

size_t CommandSystem::commandableCount() const {
    size_t n = 0;
    for (size_t i = 0; i < cult_.size(); ++i) {
        const Cultist& c = cult_.at(i);
        if (c.alive() && c.state() != CultistState::Converted) ++n;
    }
    return n;
}

float CommandSystem::obedienceChance(DirectiveType d, Vec3 target) const {
    return previewObedience(d, target).chance;
}

ObediencePreview CommandSystem::previewObedience(DirectiveType d,
                                                 Vec3 target) const {
    // Creative liberty: obedience is a blend of devotion-driven loyalty,
    // insurrection pressure, chaos-driven lunacy, command distance from the
    // cult's centroid, and belief alignment. Tuned so a fresh loyal cult
    // (~0.74) obeys most things while a crumbling one (~0.2) rarely does.
    ObediencePreview out;
    float loyaltySum = 0.0f;
    size_t n = 0;
    size_t lunatics = 0;
    Vec3 centroid;
    for (size_t i = 0; i < cult_.size(); ++i) {
        const Cultist& c = cult_.at(i);
        if (!c.alive() || c.state() == CultistState::Converted) continue;
        loyaltySum += c.devotion() / 100.0f;
        if (c.state() == CultistState::Lunatic) ++lunatics;
        centroid += c.position();
        ++n;
    }
    if (n == 0) {
        out.reasons.push_back("no cultists left to command");
        return out;
    }

    float loyalty = loyaltySum / static_cast<float>(n);
    float risk = cult_.insurrectionRisk() / 100.0f;
    float lunaticFrac = static_cast<float>(lunatics) / static_cast<float>(n);
    centroid = centroid * (1.0f / static_cast<float>(n));
    float distPenalty = std::min(0.2f, centroid.distance(target) / 2500.0f);

    float chance = 0.5f + 0.4f * loyalty - 0.5f * risk
                   - distPenalty;
    if (beliefs_.isActive(Belief::Chaos))
        chance -= 0.3f * lunaticFrac;

    // The cult carries out orders that match its creed.
    bool creedAligned = false;
    if (beliefs_.isActive(Belief::War) && d == DirectiveType::GoToWar) { chance += 0.15f; creedAligned = true; }
    if (beliefs_.isActive(Belief::Conversion) && d == DirectiveType::ConvertCampaign) { chance += 0.15f; creedAligned = true; }
    if (beliefs_.isActive(Belief::Sacrifice) && d == DirectiveType::MassSacrifice) { chance += 0.10f; creedAligned = true; }
    // Wave 9b: the new directives align with their creeds too.
    if (beliefs_.isActive(Belief::Trickery) && d == DirectiveType::AssassinateProphet) { chance += 0.10f; creedAligned = true; }
    if (beliefs_.isActive(Belief::Fear) && d == DirectiveType::BlightLand) { chance += 0.10f; creedAligned = true; }
    if (beliefs_.isActive(Belief::Magic) && d == DirectiveType::GrandSummoning) { chance += 0.10f; creedAligned = true; }
    // Wave 15: the new directives align with their creeds too.
    if (beliefs_.isActive(Belief::Dreams) && d == DirectiveType::OneiricHarvest) { chance += 0.10f; creedAligned = true; }
    if (beliefs_.isActive(Belief::Reconstruction) &&
        d == DirectiveType::RebuildSanctum) { chance += 0.10f; creedAligned = true; }

    if (chance < 0.05f) chance = 0.05f;
    if (chance > 0.95f) chance = 0.95f;
    out.chance = chance;

    // Wave 28: human-readable reasons, worst problems first.
    if (loyalty < 0.35f) out.reasons.push_back("devotion is low");
    if (risk > 0.5f) out.reasons.push_back("insurrection brews");
    if (distPenalty > 0.1f) out.reasons.push_back("the target is far");
    if (lunaticFrac > 0.25f) out.reasons.push_back("lunatics howl in the ranks");
    if (creedAligned) out.reasons.push_back("it matches the creed");
    else out.reasons.push_back("it strains against the creed");
    return out;
}

void CommandSystem::publishIssuedResolved(DirectiveType d, CommandOutcome o,
                                          float chance,
                                          FactionId targetFaction) {
    GameEvent issued(EventType::DirectiveIssued);
    issued.tag = directiveName(d);
    issued.amount = chance;
    bus_.publish(issued);

    GameEvent resolved(EventType::DirectiveResolved);
    // Wave 4: tag carries "DirectiveName/OutcomeName" so the exertion
    // pipeline can attribute obedience/failure to the right belief.
    resolved.tag = std::string(directiveName(d)) + "/" + commandOutcomeName(o);
    resolved.amount = chance;
    // Wave 9b: carry the enemy faction so follow-through operations know
    // who the directive is aimed at (FACTION_NEUTRAL = unspecified).
    resolved.faction = targetFaction;
    bus_.publish(resolved);
}

CommandResult CommandSystem::issueCommand(DirectiveType d, Vec3 target,
                                          FactionId targetFaction) {
    if (commandableCount() == 0) {
        publishIssuedResolved(d, CommandOutcome::Refused, 0.0f, targetFaction);
        // Nobody to obey, and nobody to revolt: no risk added.
        return {CommandOutcome::Refused, 0.0f, "no cultists to command"};
    }

    float chance = obedienceChance(d, target);
    float risk = cult_.insurrectionRisk() / 100.0f;
    float r = rng_.uniform(0.0f, 1.0f);

    CommandOutcome outcome;
    if (r < chance)
        outcome = CommandOutcome::Obeyed;
    else if (r < chance + 0.15f)
        outcome = CommandOutcome::PartiallyObeyed;
    else if (risk > 0.7f)
        outcome = CommandOutcome::SparksInsurrection;
    else
        outcome = CommandOutcome::Refused;

    publishIssuedResolved(d, outcome, chance, targetFaction);

    switch (outcome) {
        case CommandOutcome::Obeyed:
            if (d == DirectiveType::ConvertCampaign)
                cult_.startConversionCampaign(5);
            else if (d == DirectiveType::RaidCity) {
                GameEvent raid(EventType::RaidPerformed);
                raid.amount = 0.4f;
                raid.pos = target;
                bus_.publish(raid);
            }
            // GoToWar, MassSacrifice, Defend, GatherRelic, and the wave 9b
            // directives (AssassinateProphet, BlightLand, GrandSummoning):
            // no automatic effect; the DirectiveExecutor's follow-through
            // operations interpret the outcome.
            break;

        case CommandOutcome::PartiallyObeyed:
            break; // no automatic effect either

        case CommandOutcome::Refused: {
            GameEvent e(EventType::InsurrectionRiskUp);
            e.amount = 5.0f;
            e.tag = "refused_directive";
            bus_.publish(e);
            break;
        }

        case CommandOutcome::SparksInsurrection: {
            GameEvent e(EventType::InsurrectionRiskUp);
            e.amount = 15.0f;
            e.tag = "insurrection_spark";
            bus_.publish(e);
            break;
        }
    }

    char detail[128];
    // e.g. "GoToWar obeyed (chance 0.72)"
    std::string outName = commandOutcomeName(outcome);
    if (!outName.empty() && outName[0] >= 'A' && outName[0] <= 'Z')
        outName[0] = static_cast<char>(outName[0] - 'A' + 'a');
    std::snprintf(detail, sizeof(detail), "%s %s (chance %.2f)",
                  directiveName(d), outName.c_str(), chance);
    return {outcome, chance, detail};
}

} // namespace cultulhu
