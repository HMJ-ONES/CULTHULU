#include "entities/Entity.h"

namespace cultulhu {

uint64_t Entity::nextId_ = 1;

Entity::Entity(EntityType type, FactionId faction, Vec3 pos, float maxHp)
    : id_(nextId_++), type_(type), faction_(faction), pos_(pos),
      hp_(maxHp), maxHp_(maxHp) {}

bool Entity::takeDamage(float amount, bool oneHit) {
    if (!alive() || amount <= 0.0f) return false;
    // A "one-hit kill" only counts if a single blow destroys a healthy target.
    if (oneHit && hp_ >= maxHp_) oneHitKilled_ = true;
    hp_ -= amount;
    if (hp_ <= 0.0f) { hp_ = 0.0f; return true; }
    return false;
}

void Entity::heal(float amount) {
    if (!alive() || amount <= 0.0f) return;
    hp_ += amount;
    if (hp_ > maxHp_) hp_ = maxHp_;
}

} // namespace cultulhu
