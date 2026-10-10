#include "entities/Structures.h"

namespace cultulhu {

BuildingState Building::state() const {
    if (!alive()) return BuildingState::Destroyed;
    return (hp_ < maxHp_) ? BuildingState::Damaged : BuildingState::Intact;
}

bool Building::takeDamage(float amount, bool oneHit) {
    bool wasAlive = alive();
    bool died = Entity::takeDamage(amount, oneHit);
    if (wasAlive) rebuildProgress_ = 0.0f;
    return died;
}

void Building::update(double dt) {
    if (!autoRebuild_) return;
    if (state() != BuildingState::Damaged) return;
    // Full rebuild over ~60s of game time (tunable, see README).
    rebuildProgress_ += static_cast<float>(dt) / 60.0f;
    if (rebuildProgress_ >= 1.0f) {
        rebuildProgress_ = 0.0f;
        heal(maxHp_);
    }
}

void Altar::unassignCaptive(uint64_t captiveId) {
    for (auto it = captives_.begin(); it != captives_.end(); ++it) {
        if (*it == captiveId) {
            captives_.erase(it);
            return;
        }
    }
}

} // namespace cultulhu
