#include "sim/SimWorld.h"

namespace cultulhu {
namespace sim {

SimWorld::SimWorld(uint64_t seed, const std::vector<Belief>& loadout)
    : rng_(seed), beliefs_(bus_, clock_), cult_(bus_, clock_, rng_),
      exertion_(bus_, beliefs_, power_, cult_, rng_) {
    // Steady-state creed: active from tick 0, no adoption delay.
    beliefs_.restoreActive(loadout);
    beliefs_.setActiveWars(1);
    for (int i = 0; i < START_CULTISTS; ++i) cult_.recruit();
}

void SimWorld::tick(double dt) {
    beliefs_.update(dt);   // adoption timers (none pending in sim)
    beliefs_.tick(dt);     // fear decay, Onslaught idle decay
    cult_.update(dt);      // conversion campaigns, revolt check
    exertion_.update(dt);  // exertion decay, derived stats, tensions
    clock_.advance(dt);
}

bool SimWorld::killOneCultist() {
    for (size_t i = 0; i < cult_.size(); ++i) {
        Cultist& c = cult_.at(i);
        if (c.alive()) {
            c.takeDamage(c.maxHp(), true);
            cult_.dismissDead();
            return true;
        }
    }
    return false;
}

} // namespace sim
} // namespace cultulhu
