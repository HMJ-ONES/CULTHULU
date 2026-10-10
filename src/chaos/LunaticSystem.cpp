#include "chaos/LunaticSystem.h"
#include "beliefs/BeliefSystem.h"
#include "core/EventBus.h"
#include "core/RNG.h"
#include "cult/CultManager.h"
#include "entities/Units.h"

#include <vector>

namespace cultulhu {

namespace {
// Insurrection risk added when a lunatic is killed/punished to align them.
constexpr float ALIGN_RISK_UP = 10.0f;
} // namespace

LunaticSystem::LunaticSystem(EventBus& bus, RNG& rng, BeliefSystem& beliefs,
                             CultManager& cult, double lunaticRate,
                             double actInterval)
    : bus_(bus), rng_(rng), beliefs_(beliefs), cult_(cult),
      lunaticRate_(lunaticRate), actInterval_(actInterval) {}

void LunaticSystem::update(double dt) {
    if (!beliefs_.isActive(Belief::Chaos)) return;

    // Loyal and infringing cultists may snap.
    for (size_t i = 0; i < cult_.size(); ++i) {
        Cultist& c = cult_.at(i);
        if ((c.state() == CultistState::Loyal ||
             c.state() == CultistState::Infringer) &&
            rng_.chance(static_cast<float>(lunaticRate_ * dt)))
            c.setState(CultistState::Lunatic);
    }

    // Each act wave: one lunatic misbehaves.
    actTimer_ += dt;
    if (actTimer_ >= actInterval_) {
        actTimer_ = 0.0;
        if (lunaticCount() > 0) misbehave();
    }
}

void LunaticSystem::misbehave() {
    // Pick a lunatic and an active belief (other than Chaos) to act against.
    std::vector<size_t> lunatics;
    for (size_t i = 0; i < cult_.size(); ++i)
        if (cult_.at(i).state() == CultistState::Lunatic)
            lunatics.push_back(i);
    if (lunatics.empty()) return;
    const size_t who = lunatics[static_cast<size_t>(
        rng_.intRange(0, static_cast<int>(lunatics.size()) - 1))];

    std::vector<Belief> targets;
    for (Belief b : beliefs_.active())
        if (b != Belief::Chaos) targets.push_back(b);

    GameEvent acted(EventType::LunaticActed);
    acted.sourceId = cult_.at(who).id();

    if (targets.empty()) {
        acted.tag = "raving"; // no other beliefs to undermine: harmless rant
        bus_.publish(acted);
        return;
    }
    misbehaveAgainst(targets[static_cast<size_t>(
        rng_.intRange(0, static_cast<int>(targets.size()) - 1))]);
}

void LunaticSystem::misbehaveAgainst(Belief target) {
    GameEvent acted(EventType::LunaticActed);

    switch (target) {
        case Belief::Sacrifice:
            // Interrupts a sacrifice ritual: the -15 power rule fires.
            acted.tag = "interruptedRitual";
            bus_.publish(acted);
            {
                GameEvent e(EventType::SacrificeInterrupted);
                e.tag = "lunatic";
                bus_.publish(e);
            }
            return;
        case Belief::Conversion:
            // Steals a loyal cultist away to another deity's devotion.
            acted.tag = "stoleDevotion";
            for (size_t i = 0; i < cult_.size(); ++i) {
                Cultist& c = cult_.at(i);
                if (c.state() == CultistState::Loyal) {
                    c.setState(CultistState::Converted);
                    acted.targetId = c.id();
                    break;
                }
            }
            bus_.publish(acted);
            return;
        case Belief::War:
            // Attacks the cult's own shrine: base-building damage lowers
            // War-belief power, per the doc's war rules.
            acted.tag = "attackedOwnShrine";
            bus_.publish(acted);
            {
                GameEvent e(EventType::BaseBuildingDamaged);
                e.tag = "lunatic";
                bus_.publish(e);
            }
            return;
        case Belief::Torture:
            // Frees an imprisoned cultist, undermining the torture apparatus.
            acted.tag = "freedPrisoners";
            for (size_t i = 0; i < cult_.size(); ++i) {
                Cultist& c = cult_.at(i);
                if (c.state() == CultistState::Imprisoned) {
                    c.setState(CultistState::Loyal);
                    acted.targetId = c.id();
                    break;
                }
            }
            bus_.publish(acted);
            return;
        case Belief::Fear:           acted.tag = "preachedCalm"; break;
        case Belief::Breeding:       acted.tag = "freedCaptives"; break;
        case Belief::Reconstruction: acted.tag = "sabotagedRebuild"; break;
        case Belief::Trickery:       acted.tag = "sprungOwnTrap"; break;
        case Belief::Magic:          acted.tag = "burnedRitualComponents"; break;
        case Belief::Onslaught:      acted.tag = "hidCivilians"; break;
        case Belief::Chaos:
        case Belief::Count:          acted.tag = "raving"; break;
    }
    bus_.publish(acted);
}

void LunaticSystem::alignLunatic(size_t index) {
    if (index >= cult_.size()) return;
    Cultist& c = cult_.at(index);
    if (c.state() != CultistState::Lunatic) return;

    c.takeDamage(c.maxHp(), true); // struck down to align them
    GameEvent aligned(EventType::LunaticAligned);
    aligned.sourceId = c.id();
    bus_.publish(aligned);

    // Aligning cultists to other beliefs breeds insurrection.
    GameEvent risk(EventType::InsurrectionRiskUp);
    risk.amount = ALIGN_RISK_UP;
    risk.tag = "alignedLunatic";
    bus_.publish(risk);
}

size_t LunaticSystem::lunaticCount() const {
    size_t n = 0;
    for (size_t i = 0; i < cult_.size(); ++i)
        if (cult_.at(i).state() == CultistState::Lunatic) ++n;
    return n;
}

} // namespace cultulhu
