#include "ai/AmbientBehavior.h"

#include "exertion/ExertionSystem.h"
#include "world/WorldMap.h"

#include <algorithm>
#include <vector>

namespace cultulhu {

const char* ambientActionName(AmbientAction a) {
    switch (a) {
        case AmbientAction::Pray:           return "Pray";
        case AmbientAction::Patrol:         return "Patrol";
        case AmbientAction::Gather:         return "Gather";
        case AmbientAction::Preach:         return "Preach";
        case AmbientAction::Brawl:          return "Brawl";
        case AmbientAction::Desecrate:      return "Desecrate";
        case AmbientAction::OmenReading:    return "OmenReading";
        case AmbientAction::Sparring:       return "Sparring";
        case AmbientAction::TendWounded:    return "TendWounded";
        case AmbientAction::Graffiti:       return "Graffiti";
        case AmbientAction::ChantingCircle: return "ChantingCircle";
        case AmbientAction::DreamSharing:   return "DreamSharing";
        case AmbientAction::MendEffigy:     return "MendEffigy";
        case AmbientAction::WhisperCampaign: return "WhisperCampaign";
        case AmbientAction::BloodRite:      return "BloodRite";
        case AmbientAction::WildsHunt:      return "WildsHunt";
        case AmbientAction::Count:          return "Count";
    }
    return "Unknown";
}

AmbientAction chooseAmbientAction(const Cultist& /*c*/, const BeliefSystem& beliefs,
                                  float morale01, double hourOfDay, RNG& rng) {
    // Weights indexed by AmbientAction (Pray..WildsHunt).
    static constexpr int kActionCount = static_cast<int>(AmbientAction::Count);
    float w[kActionCount] = {
        15.0f, // Pray
        20.0f, // Patrol
        20.0f, // Gather
        10.0f, // Preach
        5.0f,  // Brawl
        5.0f,  // Desecrate
        6.0f,  // OmenReading
        6.0f,  // Sparring
        4.0f,  // TendWounded
        4.0f,  // Graffiti
        5.0f,  // ChantingCircle
        6.0f,  // DreamSharing
        4.0f,  // MendEffigy
        5.0f,  // WhisperCampaign
        3.0f,  // BloodRite
        5.0f,  // WildsHunt
    };
    static_assert(sizeof(w) / sizeof(w[0]) == 16, "weights must match AmbientAction count");

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
    // Wave 9c weights.
    if (beliefs.isActive(Belief::Dreams))
        w[6] += 12.0f;
    if (beliefs.isActive(Belief::War) || beliefs.isActive(Belief::Onslaught))
        w[7] += 10.0f;
    if (beliefs.isActive(Belief::Reconstruction))
        w[8] += 10.0f;
    if (morale01 < 0.5f)
        w[8] += 6.0f; // low spirits: the wounded get tended
    if (beliefs.isActive(Belief::Fear))
        w[9] += 12.0f;
    if (beliefs.isActive(Belief::Trickery))
        w[9] += 8.0f;
    if (beliefs.isActive(Belief::Magic))
        w[10] += 12.0f;
    if (night)
        w[10] += 6.0f;
    // Wave 15 weights.
    if (beliefs.isActive(Belief::Dreams))
        w[11] += 12.0f;
    if (night)
        w[11] += 6.0f;
    if (beliefs.isActive(Belief::Reconstruction))
        w[12] += 10.0f;
    if (morale01 < 0.5f)
        w[12] += 4.0f; // low spirits: mend what is broken
    if (beliefs.isActive(Belief::Trickery))
        w[13] += 10.0f;
    if (beliefs.isActive(Belief::Fear))
        w[13] += 5.0f;
    if (beliefs.isActive(Belief::Torture))
        w[14] += 8.0f;
    if (beliefs.isActive(Belief::Sacrifice))
        w[14] += 6.0f;
    if (beliefs.isActive(Belief::Onslaught))
        w[15] += 10.0f;
    if (beliefs.isActive(Belief::War))
        w[15] += 6.0f;

    float total = 0.0f;
    for (float x : w) total += x;

    float roll = rng.uniform(0.0f, total);
    float acc = 0.0f;
    for (int i = 0; i < kActionCount; ++i) {
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

        case AmbientAction::OmenReading: {
            // Portents read in entrails and starlight: a small power gain.
            // Dreams synergy: when Dreams exertion burns hot (>= 50) the
            // visions sharpen and the reading yields a bonus.
            float amount = 1.0f;
            if (exertion_ && exertion_->exertion(Belief::Dreams) >= 50.0f)
                amount = 2.5f;
            GameEvent e(EventType::OmenRead);
            e.sourceId = c.id();
            e.amount = amount;
            bus_.publish(e);
            break;
        }

        case AmbientAction::Sparring: {
            // Friendly bouts: pick a partner the same way Brawl does.
            std::vector<size_t> others;
            for (size_t i = 0; i < cult_.size(); ++i) {
                Cultist& o = cult_.at(i);
                if (o.id() != c.id() && eligible(o)) others.push_back(i);
            }
            if (others.empty()) break; // nobody to spar with: nothing happens

            Cultist& partner = cult_.at(others[rng_.intRange(
                0, static_cast<int>(others.size()) - 1)]);

            GameEvent e(EventType::SparringHeld);
            e.sourceId = c.id();
            e.targetId = partner.id();
            e.amount = 1.0f;
            bus_.publish(e);

            // Tiny loyalty bump for both fighters; small injury risk.
            c.setDevotion(c.devotion() + 2.0f);
            partner.setDevotion(partner.devotion() + 2.0f);
            if (rng_.chance(0.15f)) {
                Cultist& hurt = rng_.chance(0.5f) ? c : partner;
                hurt.takeDamage(8.0f);
            }
            break;
        }

        case AmbientAction::TendWounded: {
            // Patch up the most hurt cultist. Reconstruction synergy: when
            // Reconstruction exertion burns hot (>= 50) the tending is
            // markedly stronger.
            Cultist* patient = nullptr;
            float worst = 1.0f;
            for (size_t i = 0; i < cult_.size(); ++i) {
                Cultist& o = cult_.at(i);
                if (!o.alive()) continue;
                float frac = o.hp() / o.maxHp();
                if (frac < worst) { worst = frac; patient = &o; }
            }
            if (!patient || worst >= 1.0f) break; // nobody hurt

            float dose = 18.0f;
            if (exertion_ &&
                exertion_->exertion(Belief::Reconstruction) >= 50.0f)
                dose = 30.0f;
            float healed = std::min(dose, patient->maxHp() - patient->hp());
            patient->heal(healed);
            patient->setDevotion(patient->devotion() + 3.0f);

            GameEvent e(EventType::HealPerformed);
            e.sourceId = c.id();
            e.targetId = patient->id();
            e.amount = healed;
            bus_.publish(e);
            break;
        }

        case AmbientAction::Graffiti: {
            // Eldritch sigils on the walls. The zone the cultist stands in
            // grows more afraid (when a world map is attached); the Fear
            // belief's dread rises through SigilPainted regardless.
            if (map_) {
                if (Zone* z = map_->zoneAtMut(c.position()))
                    z->addAmbientFear(8.0f);
            }
            GameEvent e(EventType::SigilPainted);
            e.sourceId = c.id();
            e.amount = 1.0f;
            e.pos = c.position();
            bus_.publish(e);
            break;
        }

        case AmbientAction::ChantingCircle: {
            GameEvent e(EventType::ChantingHeld);
            e.sourceId = c.id();
            e.amount = 1.0f;
            bus_.publish(e);
            // Sometimes the chant carries: a wild thing slinks closer.
            if (rng_.chance(0.10f)) {
                static const char* kSpecies[] = {"ghoul", "deep one",
                                                 "night-gaunt"};
                GameEvent lured(EventType::CreatureAttracted);
                lured.sourceId = c.id();
                lured.tag = kSpecies[rng_.intRange(0, 2)];
                bus_.publish(lured);
            }
            break;
        }

        case AmbientAction::DreamSharing: {
            // The cultist recounts last night's vision at the campfire.
            // When Dreams exertion burns hot (>= 50) the vision is vivid
            // enough to reach a distant sleeper: a civilian converts.
            const bool hot = exertion_ &&
                             exertion_->exertion(Belief::Dreams) >= 50.0f;
            GameEvent e(EventType::DreamShared);
            e.sourceId = c.id();
            e.amount = hot ? 2.0f : 1.0f;
            bus_.publish(e);
            if (hot && rng_.chance(0.30f)) {
                GameEvent whisper(EventType::DreamWhisper);
                whisper.sourceId = c.id();
                whisper.amount = 1.0f;
                whisper.tag = "dreamshared";
                bus_.publish(whisper);

                GameEvent conv(EventType::ConversionPerformed);
                conv.sourceId = c.id();
                conv.amount = 1.0f;
                conv.tag = "dreamshared";
                bus_.publish(conv);
            }
            break;
        }

        case AmbientAction::MendEffigy: {
            // Small shrine repairs: unglamorous, steadying work.
            GameEvent e(EventType::EffigyMended);
            e.sourceId = c.id();
            e.amount = 1.0f;
            bus_.publish(e);
            c.setDevotion(c.devotion() + 2.0f); // pride in the work
            break;
        }

        case AmbientAction::WhisperCampaign: {
            // The cultist slips into the settlement planting false rumors.
            // The zone grows more afraid when a world map is attached; the
            // Trickery belief's craft rises through RumorSpread regardless.
            if (map_) {
                if (Zone* z = map_->zoneAtMut(c.position()))
                    z->addAmbientFear(4.0f);
            }
            GameEvent e(EventType::RumorSpread);
            e.sourceId = c.id();
            e.amount = 1.0f;
            e.pos = c.position();
            bus_.publish(e);
            // A rumor that lands converts like a sermon.
            if (rng_.chance(0.20f)) {
                GameEvent conv(EventType::ConversionPerformed);
                conv.sourceId = c.id();
                conv.amount = 1.0f;
                conv.tag = "rumor";
                bus_.publish(conv);
            }
            break;
        }

        case AmbientAction::BloodRite: {
            // Ritual laceration: the flesh is an altar too. Zeal rises;
            // the rite sometimes draws more blood than intended.
            GameEvent e(EventType::RiteOfFlesh);
            e.sourceId = c.id();
            e.amount = 1.0f;
            bus_.publish(e);
            c.setDevotion(c.devotion() + 2.0f);
            if (rng_.chance(0.10f)) c.takeDamage(6.0f);
            break;
        }

        case AmbientAction::WildsHunt: {
            // A hunting party of one: the meat feeds the cult (supplies),
            // the quarry sometimes feeds on the hunter.
            const int meat = rng_.intRange(1, 2);
            GameEvent hunt(EventType::WildsHunted);
            hunt.sourceId = c.id();
            hunt.amount = static_cast<float>(meat);
            bus_.publish(hunt);

            GameEvent food(EventType::SuppliesGathered);
            food.sourceId = c.id();
            food.amount = static_cast<float>(meat);
            bus_.publish(food);

            if (rng_.chance(0.15f)) c.takeDamage(10.0f);
            break;
        }

        case AmbientAction::Count:
            break;
    }
    ++actions_;
}

bool AmbientDirector::forceAction(size_t cultistIndex, AmbientAction a) {
    if (cultistIndex >= cult_.size()) return false;
    Cultist& c = cult_.at(cultistIndex);
    if (!eligible(c)) return false;
    applyAction(c, a);
    return true;
}

bool AmbientDirector::forceRandom() {
    std::vector<size_t> elig;
    for (size_t i = 0; i < cult_.size(); ++i)
        if (eligible(cult_.at(i))) elig.push_back(i);
    if (elig.empty()) return false;
    Cultist& c = cult_.at(
        elig[static_cast<size_t>(
            rng_.intRange(0, static_cast<int>(elig.size()) - 1))]);
    applyAction(c, chooseAmbientAction(c, beliefs_, c.devotion() / 100.0f,
                                       hourOfDay_, rng_));
    return true;
}

} // namespace cultulhu
