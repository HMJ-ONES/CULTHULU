#include "exertion/ExertionSystem.h"

#include "exertion/ActionExertionTable.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace cultulhu {

namespace {
int bi(Belief b) { return static_cast<int>(b); }

float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// DirectiveResolved tags are "DirectiveName/OutcomeName" (see
// CommandSystem::publishIssuedResolved). Split them back apart.
bool parseDirectiveTag(const std::string& tag, DirectiveType& d,
                        std::string& outcome) {
    const size_t slash = tag.find('/');
    if (slash == std::string::npos) return false;
    const std::string dname = tag.substr(0, slash);
    outcome = tag.substr(slash + 1);
    for (int i = 0; i < static_cast<int>(DirectiveType::Count); ++i) {
        DirectiveType cand = static_cast<DirectiveType>(i);
        if (dname == directiveName(cand)) {
            d = cand;
            return true;
        }
    }
    return false;
}
} // namespace

ExertionSystem::ExertionSystem(EventBus& bus, BeliefSystem& beliefs,
                               PowerSystem& power, CultManager& cult,
                               RNG& rng)
    : bus_(bus), beliefs_(beliefs), power_(power), cult_(cult), rng_(rng) {
    for (int i = 0; i < static_cast<int>(Belief::Count); ++i)
        exertion_[i] = BASELINE;
    stats_ = computeDerivedStats(exertion_, 0.0f);
    // Unified power pipeline: every gameplay event flows through here.
    for (int i = 0; i < static_cast<int>(EventType::Count); ++i) {
        bus_.subscribe(static_cast<EventType>(i),
                       [this](const GameEvent& e) { onGameEvent(e); });
    }
}

float ExertionSystem::exertion(Belief b) const {
    return exertion_[bi(b)];
}

void ExertionSystem::addExertion(Belief b, float amount) {
    const int i = bi(b);
    const float rate = beliefs_.isActive(b) ? 1.0f : 0.5f;
    float gain = amount * rate;
    // Conflict suppression: when this belief and a conflicting belief both
    // burn above threshold, each suppresses the other's gains by half.
    if (gain > 0.0f && exertion_[i] > SYNERGY_THRESHOLD) {
        for (int j = 0; j < static_cast<int>(Belief::Count); ++j) {
            if (j != i &&
                beliefRelation(b, static_cast<Belief>(j)) ==
                    BeliefRelation::Conflict &&
                exertion_[j] > SYNERGY_THRESHOLD) {
                gain *= 0.5f;
                break;
            }
        }
    }
    exertion_[i] = clampf(exertion_[i] + gain, MIN_EXERTION, MAX_EXERTION);
}

void ExertionSystem::feedFromEvent(const GameEvent& e) {
    for (std::size_t i = 0; i < ACTION_EXERTION_TABLE_SIZE; ++i) {
        const ExertionFeed& feed = ACTION_EXERTION_TABLE[i];
        if (feed.type == e.type) addExertion(feed.belief, feed.amount);
    }
}

void ExertionSystem::feedFromDirective(const GameEvent& e) {
    if (e.type != EventType::DirectiveResolved) return;
    DirectiveType d;
    std::string outcome;
    if (!parseDirectiveTag(e.tag, d, outcome)) return;
    for (std::size_t i = 0; i < DIRECTIVE_EXERTION_TABLE_SIZE; ++i) {
        const DirectiveFeed& feed = DIRECTIVE_EXERTION_TABLE[i];
        if (feed.directive != d) continue;
        if (outcome == "Obeyed") {
            addExertion(feed.belief, feed.obeyed);
        } else if (outcome == "PartiallyObeyed") {
            addExertion(feed.belief, feed.obeyed * 0.5f);
        } else if (outcome == "Refused") {
            addExertion(Belief::Chaos, feed.refused); // defiance feeds Chaos
        } else if (outcome == "SparksInsurrection") {
            addExertion(Belief::Chaos, feed.refused * 2.0f);
        }
        return;
    }
}

float ExertionSystem::powerMultiplierFor(const GameEvent& e) const {
    float mult = 1.0f;
    switch (e.type) {
        case EventType::TorturePerformed:
        case EventType::CapturedTortured:
            // Terror (Fear x Torture) and Cruelty Unbound (Torture x Chaos)
            // stack additively on the signed power delta.
            if (synergyActive(Belief::Fear, Belief::Torture, exertion_))
                mult += 0.25f;
            if (synergyActive(Belief::Torture, Belief::Chaos, exertion_))
                mult += 0.25f;
            break;
        case EventType::RaidPerformed:
            // Terror and Shock and Awe (War x Fear) both love a good raid.
            if (synergyActive(Belief::Fear, Belief::Torture, exertion_))
                mult += 0.25f;
            if (synergyActive(Belief::War, Belief::Fear, exertion_))
                mult += 0.25f;
            break;
        case EventType::NecromancyPerformed:
            // Dark Rites (Sacrifice x Magic) empower necromancy.
            if (synergyActive(Belief::Sacrifice, Belief::Magic, exertion_))
                mult += 0.50f;
            break;
        case EventType::SacrificeCompleted:
            // Desecrated Rites (Chaos x Sacrifice): lunatics disrupt rituals.
            if (conflictActive(Belief::Chaos, Belief::Sacrifice, exertion_))
                mult -= 0.25f;
            break;
        default:
            break;
    }
    return mult;
}

void ExertionSystem::onGameEvent(const GameEvent& e) {
    feedFromEvent(e);
    feedFromDirective(e);
    // Existing belief power rules, scaled by the stacked-income multiplier
    // (R1), then exertion-scaled synergy bonuses.
    const float delta = beliefs_.onEvent(e);
    power_.add(delta * stackedIncomeMultiplier() * powerMultiplierFor(e));
}

// R1 (wave 10): diminishing returns on stacked belief income. Three full
// schedules stacking linearly made Conversion x Trickery x Magic 2.28x the
// scenario median. Each additional active belief now dilutes the whole
// income stream: second belief's events at 0.75x, third at 0.5x. Applies to
// losses too (symmetric dilution).
float ExertionSystem::stackedIncomeMultiplier() const {
    const size_t n = beliefs_.active().size();
    if (n >= 3) return 0.5f;
    if (n == 2) return 0.75f;
    return 1.0f;
}

void ExertionSystem::fireTensions(double dt) {
    const BeliefPair* pairs = conflictPairs();
    const int n = conflictPairCount();
    for (int i = 0; i < n && i < 16; ++i) {
        tensionCooldown_[i] -= dt;
        const float a = exertion_[bi(pairs[i].a)];
        const float b = exertion_[bi(pairs[i].b)];
        if (a > TENSION_THRESHOLD && b > TENSION_THRESHOLD &&
            tensionCooldown_[i] <= 0.0) {
            tensionCooldown_[i] = TENSION_COOLDOWN;
            GameEvent t(EventType::BeliefTension);
            t.tag = std::string(beliefName(pairs[i].a)) + " vs " +
                    beliefName(pairs[i].b);
            t.amount = 1.0f;
            bus_.publish(t);
            // Doctrinal tension frays the cult: a small insurrection nudge.
            GameEvent risk(EventType::InsurrectionRiskUp);
            risk.amount = 1.5f;
            risk.tag = "belief_tension";
            bus_.publish(risk);
        }
    }
}

void ExertionSystem::update(double dt) {
    // Decay toward baseline when neglected.
    const float step = DECAY_PER_SEC * static_cast<float>(dt);
    for (int i = 0; i < static_cast<int>(Belief::Count); ++i) {
        const float diff = BASELINE - exertion_[i];
        if (std::fabs(diff) <= step)
            exertion_[i] = BASELINE;
        else
            exertion_[i] += (diff > 0.0f ? step : -step);
    }

    // Recompute derived stats from the fresh exertion levels.
    stats_ = computeDerivedStats(exertion_,
                                 cult_.insurrectionRisk() / 100.0f);

    // Loyalty drift: devotion follows the creed (clamped 0..100 by
    // Cultist::setDevotion).
    const float drift = stats_.loyaltyDriftPerSec;
    if (drift != 0.0f) {
        for (size_t i = 0; i < cult_.size(); ++i) {
            Cultist& c = cult_.at(i);
            if (!c.alive() || c.state() == CultistState::Converted) continue;
            c.setDevotion(c.devotion() + drift * static_cast<float>(dt));
        }
    }

    // Dread Broods (Breeding x Fear): monstrosities in the ranks make the
    // dark feel darker — passive fear generation.
    if (synergyActive(Belief::Breeding, Belief::Fear, exertion_))
        beliefs_.addFear(0.8f * static_cast<float>(dt));

    fireTensions(dt);
}

} // namespace cultulhu
