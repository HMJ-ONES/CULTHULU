#include "war/WarSystem.h"
#include "beliefs/BeliefSystem.h"
#include "core/EventBus.h"

#include <algorithm>

namespace cultulhu {

WarSystem::WarSystem(EventBus& bus) : bus_(bus) {}

void WarSystem::declareWar(FactionId deity) {
    if (deity <= 0 || deity == FACTION_CTHULHU) return;
    if (!atWar(deity)) wars_.push_back(deity);
}

void WarSystem::makePeace(FactionId deity) {
    wars_.erase(std::remove(wars_.begin(), wars_.end(), deity), wars_.end());
}

bool WarSystem::atWar(FactionId deity) const {
    return std::find(wars_.begin(), wars_.end(), deity) != wars_.end();
}

void WarSystem::recordKill(FactionId victimFaction) {
    if (!atWar(victimFaction)) return;
    ++kills_[victimFaction];
    GameEvent e(EventType::EnemyCultistSlain);
    e.faction = victimFaction;
    e.amount = 1.0f;
    bus_.publish(e);
}

int WarSystem::killsAgainst(FactionId deity) const {
    auto it = kills_.find(deity);
    return it == kills_.end() ? 0 : it->second;
}

void WarSystem::syncBeliefs(BeliefSystem& beliefs) const {
    beliefs.setActiveWars(warCount());
}

} // namespace cultulhu
