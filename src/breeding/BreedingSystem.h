#pragma once

#include "core/Vec3.h"
#include "entities/Units.h"

#include <memory>

namespace cultulhu {

class EventBus;
class RNG;

enum class Species {
    Human,
    DeepOne,
    Ghoul,
    Beast,
    Horror,
    Count
};

inline const char* speciesName(Species s) {
    switch (s) {
        case Species::Human:   return "Human";
        case Species::DeepOne:  return "DeepOne";
        case Species::Ghoul:    return "Ghoul";
        case Species::Beast:    return "Beast";
        case Species::Horror:   return "Horror";
        case Species::Count:    return "Count";
    }
    return "Unknown";
}

// Breeding belief mechanics: cross-species breeding to create monstrosities.
// Some are born feral and uncontrollable — they rampage against the cult.
class BreedingSystem {
public:
    // Wave 29: 35% feral was self-sabotage you couldn't mitigate; the base
    // is now 15%, and Breeding exertion (skill) halves it further.
    static constexpr float DEFAULT_FERAL_CHANCE = 0.15f;

    BreedingSystem(EventBus& bus, RNG& rng);

    // Mirrors BeliefSystem::isActive(Belief::Breeding); breeding is only
    // possible while the belief is active.
    void setBreedingActive(bool b) { active_ = b; }
    bool breedingActive() const { return active_; }

    void setFeralChance(float c) { feralChance_ = c; }
    float feralChance() const { return feralChance_; }
    // Wave 29: skilled breeders breed truer — skill 0..1 (from Breeding
    // exertion) scales the effective feral chance down to half.
    void setBreedingSkill(float s) { skill_ = s < 0 ? 0 : (s > 1 ? 1 : s); }
    float effectiveFeralChance() const {
        return feralChance_ * (1.0f - 0.5f * skill_);
    }

    static bool compatible(Species a, Species b);

    // Returns nullptr when the belief is inactive or the species are
    // incompatible. Publishes MonstrosityBred (tag = name), and FeralRampage
    // when feral. Pass a name to christen the birth (NMS-style naming).
    std::unique_ptr<Monstrosity> breed(Species a, Species b,
                                      FactionId faction, Vec3 pos,
                                      const std::string& name = "");

private:
    EventBus& bus_;
    RNG& rng_;
    bool active_ = false;
    float feralChance_ = DEFAULT_FERAL_CHANCE;
    float skill_ = 0.0f;
};

} // namespace cultulhu
