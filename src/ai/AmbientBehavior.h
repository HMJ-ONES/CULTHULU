#pragma once

#include "beliefs/BeliefSystem.h"
#include "core/EventBus.h"
#include "core/Events.h"
#include "core/RNG.h"
#include "cult/CultManager.h"
#include "entities/Units.h"

#include <cstdint>

namespace cultulhu {

// Autonomous cultist activity: between player commands the followers act on
// their own (pray, patrol, gather, preach, brawl, desecrate), weighted by
// their devotion/morale, active beliefs, and the time of day. This is what
// makes the cult feel alive during free-roam.
enum class AmbientAction {
    Pray,
    Patrol,
    Gather,
    Preach,
    Brawl,
    Desecrate,
    Count
};

const char* ambientActionName(AmbientAction a);

// Weighted random pick of an ambient action for one cultist.
// morale01 = devotion/100. hourOfDay in [0,24).
AmbientAction chooseAmbientAction(const Cultist& c, const BeliefSystem& beliefs,
                                  float morale01, double hourOfDay, RNG& rng);

// Periodically lets every eligible cultist perform one ambient action.
// Eligible: alive, and state Loyal or Infringer (Lunatic, Imprisoned and
// Converted cultists sit out).
class AmbientDirector {
public:
    AmbientDirector(EventBus& bus, RNG& rng, BeliefSystem& beliefs,
                    CultManager& cult, double intervalSeconds = 30.0);

    void update(double dt);

    void setHourOfDay(double h) { hourOfDay_ = h; }

    uint64_t actionsPerformed() const { return actions_; }

private:
    static bool eligible(const Cultist& c);

    void applyAction(Cultist& c, AmbientAction a);

    EventBus& bus_;
    RNG& rng_;
    BeliefSystem& beliefs_;
    CultManager& cult_;

    double interval_;
    double timer_ = 0.0;
    double hourOfDay_ = 12.0;
    uint64_t actions_ = 0;
};

} // namespace cultulhu
