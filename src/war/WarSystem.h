#pragma once

#include "entities/Entity.h"

#include <unordered_map>
#include <vector>

namespace cultulhu {

class BeliefSystem;
class EventBus;

// Tracks wars declared against rival deities (factions) and feeds the War
// belief its scaling context: full power rate vs a single deity, reduced
// rate when fighting several at once.
class WarSystem {
public:
    explicit WarSystem(EventBus& bus);

    void declareWar(FactionId deity);   // deity > 0 (0 is Cthulhu)
    void makePeace(FactionId deity);
    bool atWar(FactionId deity) const;
    int warCount() const { return static_cast<int>(wars_.size()); }

    // Record the slaying of a cultist of the victim faction. Publishes
    // EnemyCultistSlain when the victim's deity is at war with Cthulhu.
    void recordKill(FactionId victimFaction);
    int killsAgainst(FactionId deity) const;

    // Push the current war count into the belief system so War-belief power
    // scaling (single vs multiple deities) stays correct.
    void syncBeliefs(BeliefSystem& beliefs) const;

private:
    EventBus& bus_;
    std::vector<FactionId> wars_;
    std::unordered_map<FactionId, int> kills_;
};

} // namespace cultulhu
