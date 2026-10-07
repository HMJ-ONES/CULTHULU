#include "ai/AmbientBehavior.h"

#include <algorithm>
#include <vector>

namespace cultulhu {

const char* ambientActionName(AmbientAction a) {
    switch (a) {
        case AmbientAction::Pray:      return "Pray";
        case AmbientAction::Patrol:    return "Patrol";
        case AmbientAction::Gather:    return "Gather";
        case AmbientAction::Preach:    return "Preach";
        case AmbientAction::Brawl:     return "Brawl";
        case AmbientAction::Desecrate: return "Desecrate";
        case AmbientAction::Count:     return "Count";
    }
    return "Unknown";
}

AmbientAction chooseAmbientAction(const Cultist& /*c*/, const BeliefSystem& beliefs,
                                  float morale01, double hourOfDay, RNG& rng) {
    // Weights indexed by AmbientAction (Pray..Desecrate).
    float w[6] = {
        15.0f, // Pray
        20.0f, // Patrol
        20.0f, // Gather
        10.0f, // Preach
        5.0f,  // Brawl
        5.0f,  // Desecrate
    };

    bool night = (hourOfDay >= 22.0 || hourOfDay < 6.0);
    if (beliefs.isActive(Belief::Sacrifice) || beliefs.isActive(Belief::Magic))
        w[0] += 15.0f;
    if (night)
        w[0] += 10.0f;
    if (beliefs.isActive(Belief::Conversion))
        w[3] += 20.0f;
    if (beliefs.isActive(Belief::Chaos))
        w[4] += 15.0f;
    if (morale01 < 0.3f)
        w[4] += 10.0f;
    if (beliefs.isActive(Belief::Torture) || beliefs.isActive(Belief::Chaos))
        w[5] += 15.0f;

    float total = 0.0f;
    for (float x : w) total += x;

    float roll = rng.uniform(0.0f, total);
    float acc = 0.0f;
    for (int i = 0; i < 6; ++i) {
        acc += w[i];
        if (roll <= acc) return static_cast<AmbientAction>(i);
    }
    return AmbientAction::Patrol; // unreachable (float rounding guard)
}

AmbientDirector::AmbientDirector(EventBus& bus, RNG& rng, BeliefSystem& beliefs,
                                 CultManager& cult, double intervalSeconds)
    : bus_(bus), rng_(rng), beliefs_(beliefs), cult_(cult),
      interval_(intervalSeconds) {}

bool AmbientDirector::eligible(const Cultist& c) {
    if (!c.alive()) return false;
    CultistState s = c.state();
    return s == CultistState::Loyal || s == CultistState::Infringer;
}

void AmbientDirector::update(double dt) {
    timer_ += dt;
    if (timer_ < interval_) return;
    while (timer_ >= interval_) timer_ -= interval_;

    for (size_t i = 0; i < cult_.size(); ++i) {
        Cultist& c = cult_.at(i);
        if (!eligible(c)) continue;
        AmbientAction a = chooseAmbientAction(c, beliefs_, c.devotion() / 100.0f,
                                              hourOfDay_, rng_);
        applyAction(c, a);
    }
}

void AmbientDirector::applyAction(Cultist& c, AmbientAction a) {
    switch (a) {
        case AmbientAction::Patrol:
            // Flavor only: the cultist walks its rounds. No event.
            break;

        case AmbientAction::Pray: {
            GameEvent e(EventType::PrayerOffered);
            e.sourceId = c.id();
            e.amount = 1.0f;
            bus_.publish(e);
            break;
        }

        case AmbientAction::Gather: {
            GameEvent e(EventType::SuppliesGathered);
            e.sourceId = c.id();
            e.amount = static_cast<float>(rng_.intRange(1, 3));
            bus_.publish(e);
            break;
        }

        case AmbientAction::Preach: {
            float eff = beliefs_.conversionEfficiency();
            if (rng_.chance(0.25f * eff)) {
                GameEvent sermon(EventType::SermonPreached);
                sermon.sourceId = c.id();
                sermon.amount = 1.0f;
                bus_.publish(sermon);

                GameEvent conv(EventType::ConversionPerformed);
                conv.sourceId = c.id();
                conv.amount = 1.0f;
                conv.tag = "sermon";
                bus_.publish(conv);
            }
            break;
        }

        case AmbientAction::Brawl: {
            // Find another eligible cultist to fight with.
            std::vector<size_t> others;
            for (size_t i = 0; i < cult_.size(); ++i) {
                Cultist& o = cult_.at(i);
                if (o.id() != c.id() && eligible(o)) others.push_back(i);
            }
            if (others.empty()) break; // nobody to brawl with: nothing happens

            Cultist& target = cult_.at(others[rng_.intRange(
                0, static_cast<int>(others.size()) - 1)]);

            GameEvent brawl(EventType::BrawlBrokeOut);
            brawl.sourceId = c.id();
            brawl.targetId = target.id();
            bus_.publish(brawl);

            if (rng_.chance(0.30f)) target.takeDamage(15.0f);

            GameEvent risk(EventType::InsurrectionRiskUp);
            risk.sourceId = c.id();
            risk.amount = 2.0f;
            risk.tag = "brawl";
            bus_.publish(risk);
            break;
        }

        case AmbientAction::Desecrate: {
            GameEvent e(EventType::DesecrationDone);
            e.sourceId = c.id();
            e.amount = 1.0f;
            bus_.publish(e);
            break;
        }

        case AmbientAction::Count:
            break;
    }
    ++actions_;
}

} // namespace cultulhu
