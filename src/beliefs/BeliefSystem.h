#pragma once

#include "beliefs/Belief.h"
#include "core/Events.h"

#include <vector>

namespace cultulhu {

class EventBus;
class GameClock;

// Implements the belief rules from the design doc:
//  - max 3 active beliefs at a time
//  - changing a belief starts an adoption timer (followers need time)
//  - punishing infringers shortens adoption but raises insurrection risk
//  - onEvent() converts gameplay events into Cthulhu-power deltas
class BeliefSystem {
public:
    static constexpr int MAX_ACTIVE = 3;
    // Base game-time for followers to adopt a belief change (~120s per doc).
    static constexpr double ADOPTION_TIME = 120.0;
    // Mass-conversion (Conversion belief): 3 instant converts, 200s cooldown.
    static constexpr double MASS_CONVERT_COOLDOWN = 200.0;
    static constexpr int MASS_CONVERT_COUNT = 3;
    // Onslaught: power decays if no civilian slain for 1 in-game hour.
    static constexpr double ONSLAUGHT_IDLE_LIMIT = 3600.0;

    BeliefSystem(EventBus& bus, GameClock& clock);

    const std::vector<Belief>& active() const { return active_; }
    bool isActive(Belief b) const;
    int activeWars() const { return activeWars_; }

    // Request swapping belief 'out' for belief 'in'. Starts an adoption
    // timer; the new belief is NOT active until adoption completes.
    // Returns false if 'in' is already active/pending or no room.
    bool requestChange(Belief in, Belief out);

    // Punish infringers: halves remaining adoption time but publishes an
    // insurrection-risk event.
    void punishInfringer();

    void setActiveWars(int n) { activeWars_ = n; }
    void setInCombat(bool c) { inCombat_ = c; }
    void setNight(bool n) { night_ = n; } // fear raids hit harder at night

    // Advance adoption timers. Call once per tick.
    void update(double dt);

    // React to a gameplay event. Returns the power delta for Cthulhu.
    float onEvent(const GameEvent& e);

    // Time-based effects (Fear decay/generation, Onslaught idle decay).
    // Returns the power delta for this tick.
    float tick(double dt);

    // Conversion belief: instant mass conversion availability.
    bool canMassConvert() const;
    void useMassConvert(); // starts the 200s cooldown, publishes event

    // Belief-derived modifiers used by other systems:
    float healMultiplier() const;      // Reconstruction: 1.5x healing
    float spellEffectiveness() const;  // Reconstruction: 0.75x other spells
    float meleeBonus() const;          // Onslaught: 1.25x melee
    float sorcererDamageMult() const;  // Magic: 1.3x sorcerer damage
    float foeCcDurationMult() const;   // Magic: 0.6x incoming CC duration
    float magicLearnTimeMult() const;  // Magic: 1.5x learning time
    float conversionEfficiency() const;// Conversion: 1.5x campaign efficiency
    float conversionVulnerability() const; // Conversion: cultists easier to steal

    float fearLevel() const { return fear_; } // 0..100
    double adoptionRemaining(Belief b) const; // -1 if not pending

private:
    struct Pending {
        Belief belief;
        double remaining;
    };

    EventBus& bus_;
    GameClock& clock_;

    std::vector<Belief> active_;
    std::vector<Pending> pending_;

    int activeWars_ = 0;
    bool inCombat_ = false;
    bool night_ = false; // set from the free-roam day/night clock

    float fear_ = 0.0f;             // Fear belief level 0..100
    float fearPassiveGen_ = 0.0f;   // from captured creatures / bred monstrosities
    double massConvertReadyAt_ = 0.0;
    double onslaughtIdleTime_ = 0.0;

    void adoptPending(const Pending& p);
};

} // namespace cultulhu
