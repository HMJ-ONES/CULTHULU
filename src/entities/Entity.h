#pragma once

#include "core/Vec3.h"

#include <cstdint>

namespace cultulhu {

enum class EntityType {
    GreatOldOne,
    EldritchAvatar, // the player's cosmic horror entity in free roam
    Cultist,
    Civilian,
    Adventurer,
    Creature,
    Monstrosity,
    Mimic,
    Sorcerer,
    Building,
    Relic,
    Artifact
};

using FactionId = int;
constexpr FactionId FACTION_NEUTRAL = -1;
constexpr FactionId FACTION_CTHULHU = 0;
// Rival deities use faction ids 1..N.

// Base class for everything that exists in the world.
class Entity {
public:
    Entity(EntityType type, FactionId faction, Vec3 pos, float maxHp);
    virtual ~Entity() = default;

    uint64_t id() const { return id_; }
    EntityType type() const { return type_; }
    FactionId faction() const { return faction_; }

    Vec3 position() const { return pos_; }
    void setPosition(Vec3 p) { pos_ = p; }

    float hp() const { return hp_; }
    float maxHp() const { return maxHp_; }
    bool alive() const { return hp_ > 0.0f; }
    bool wasOneHitKilled() const { return oneHitKilled_; }

    // Returns true if this damage killed the entity.
    // oneHit=true marks it as a one-hit kill (matters for Torture belief).
    virtual bool takeDamage(float amount, bool oneHit = false);
    virtual void heal(float amount);

    // Bring a dead entity back at 'hp' (clamped to maxHp). Used by the
    // Sacrifice belief's deny-death mechanic; healing cannot revive.
    void revive(float hp) {
        hp_ = hp < 0.0f ? 0.0f : (hp > maxHp_ ? maxHp_ : hp);
    }

    virtual void update(double /*dt*/) {}

protected:
    uint64_t id_;
    EntityType type_;
    FactionId faction_;
    Vec3 pos_;
    float hp_;
    float maxHp_;
    bool oneHitKilled_ = false;

private:
    static uint64_t nextId_;
};

} // namespace cultulhu
