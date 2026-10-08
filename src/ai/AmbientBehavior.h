#pragma once

#include "beliefs/BeliefSystem.h"
#include "core/EventBus.h"
#include "core/Events.h"
#include "core/RNG.h"
#include "cult/CultManager.h"
#include "entities/Units.h"

#include <cstdint>

namespace cultulhu {

class ExertionSystem; // optional: powers Dreams/Reconstruction synergies
class WorldMap;       // optional: lets graffiti raise fear in a real zone

// Autonomous cultist activity: between player commands the followers act on
// their own (pray, patrol, gather, preach, brawl, desecrate, read omens,
// spar, tend the wounded, paint sigils, chant), weighted by their
// devotion/morale, active beliefs, and the time of day. This is what
// makes the cult feel alive during free-roam.
enum class AmbientAction {
    Pray,
    Patrol,
    Gather,
    Preach,
    Brawl,
    Desecrate,
    // Wave 9c: five new autonomous activities.
    OmenReading,    // studies portents; small power gain; Dreams synergy
    Sparring,       // friendly bouts; tiny combat-power/loyalty bump; injury risk
    TendWounded,    // heals injured cultists; Reconstruction synergy; loyalty bump
    Graffiti,       // paints eldritch sigils; raises Fear in the current zone
    ChantingCircle, // group chant; Magic exertion gain; may attract a creature
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

    // Wave 9c: optional hooks. The exertion system feeds the Dreams bonus
    // for omen-reading and the Reconstruction boost for tending the
    // wounded; the world map lets graffiti raise fear in the cultist's
    // current zone. Both are nullable; without them the base effects
    // apply.
    void setExertion(ExertionSystem* e) { exertion_ = e; }
    void setWorldMap(WorldMap* m) { map_ = m; }

    // Wave 9c: force one ambient action on a cultist (driver/testing).
    // Returns false when the index is out of range or the cultist is not
    // eligible. Also picks a random eligible cultist + weighted action.
    bool forceAction(size_t cultistIndex, AmbientAction a);
    bool forceRandom();

    uint64_t actionsPerformed() const { return actions_; }

private:
    static bool eligible(const Cultist& c);

    void applyAction(Cultist& c, AmbientAction a);

    EventBus& bus_;
    RNG& rng_;
    BeliefSystem& beliefs_;
    CultManager& cult_;
    ExertionSystem* exertion_ = nullptr; // optional (wave 9c synergies)
    WorldMap* map_ = nullptr;            // optional (graffiti zones)

    double interval_;
    double timer_ = 0.0;
    double hourOfDay_ = 12.0;
    uint64_t actions_ = 0;
};

} // namespace cultulhu
