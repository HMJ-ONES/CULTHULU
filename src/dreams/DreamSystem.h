#pragma once

#include <cstdint>
#include <functional>
#include <set>

namespace cultulhu {

class EventBus;
class RNG;
class BeliefSystem;
class CultManager;

// Dreams belief (the 12th belief, chosen by the project owner): cultists need
// rest. While resting they receive dream-visions from their entity.
//  - each resting cultist generates a trickle of power per second
//  - dream-whispers can convert distant civilians (small chance per tick);
//    the conversion flows through ConversionPerformed so the Conversion
//    belief synergizes naturally
//  - nightmares: a resting cultist may wake up Lunatic (the Chaos belief
//    amplifies this, mirroring its lunatic theme)
// Rest state is tracked here; the game loop / driver / AI layer calls
// startRest/endRest. Effects only apply while the Dreams belief is active.
class DreamSystem {
public:
    // powerPerSec: power per resting cultist per game-second.
    // convertChance: dream-whisper conversion chance per resting cultist per
    //   game-second. nightmareChance: base nightmare chance per resting
    //   cultist per game-second (x3 while Chaos is active).
    DreamSystem(EventBus& bus, RNG& rng, BeliefSystem& beliefs,
                CultManager& cult, float powerPerSec = 0.5f,
                float convertChance = 0.002f, float nightmareChance = 0.001f);

    void startRest(uint64_t cultistId);
    void endRest(uint64_t cultistId);
    bool isResting(uint64_t cultistId) const;
    size_t restingCount() const { return resting_.size(); }

    // Optional hook: pick a distant civilian id for a dream-whisper
    // conversion (0 = abstract, no simulated civilian).
    void setCivilianPicker(std::function<uint64_t()> picker) {
        picker_ = std::move(picker);
    }

    // Advance dream logic. Returns the power delta for this tick (only while
    // the Dreams belief is active).
    float update(double dt);

private:
    EventBus& bus_;
    RNG& rng_;
    BeliefSystem& beliefs_;
    CultManager& cult_;

    float powerPerSec_;
    float convertChance_;
    float nightmareChance_;
    bool night_ = false; // dreams run deeper at night (x1.5 power)

    std::set<uint64_t> resting_;
    std::function<uint64_t()> picker_;

public:
    // Night amplifies dream-visions (set from the free-roam day/night clock).
    void setNight(bool n) { night_ = n; }
};

} // namespace cultulhu
