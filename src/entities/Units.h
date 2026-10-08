#pragma once

#include "entities/Entity.h"
#include "power/PowerSystem.h"

#include <cmath>
#include <string>

namespace cultulhu {

enum class CultistState {
    Loyal,      // follows all active beliefs
    Infringer,  // broke a belief rule; may be punished
    Lunatic,    // Chaos belief: acts against other active beliefs
    Imprisoned, // locked up (Chaos belief: lowers power)
    Converted   // turned to another deity's devotion
};

class GreatOldOne : public Entity {
public:
    GreatOldOne(FactionId faction, Vec3 pos, float maxHp = 5000.0f)
        : Entity(EntityType::GreatOldOne, faction, pos, maxHp) {}
};

// The player's own cosmic horror entity roaming the world in free roam mode.
// Linked to the PowerSystem: the avatar is the in-world vessel of Cthulhu's
// power, so avatar actions feed the belief/power pipeline.
class EldritchAvatar : public Entity {
public:
    EldritchAvatar(FactionId faction, Vec3 pos, PowerSystem& power,
                   float maxHp = 2000.0f)
        : Entity(EntityType::EldritchAvatar, faction, pos, maxHp),
          power_(power) {
        type_ = EntityType::EldritchAvatar;
    }

    // Move in 'dir' (need not be normalized) for dt seconds at speed.
    void move(Vec3 dir, double dt, float speed = 8.0f) {
        const float len =
            dir.x * dir.x + dir.y * dir.y + dir.z * dir.z;
        if (len <= 0.0f || !alive()) return;
        const float inv = 1.0f / std::sqrt(len);
        pos_.x += dir.x * inv * speed * static_cast<float>(dt);
        pos_.y += dir.y * inv * speed * static_cast<float>(dt);
        pos_.z += dir.z * inv * speed * static_cast<float>(dt);
    }

    float facingYaw() const { return facingYaw_; }
    void setFacingYaw(float y) { facingYaw_ = y; }

    PowerSystem& power() { return power_; }

private:
    PowerSystem& power_;
    float facingYaw_ = 0.0f;
};

class Cultist : public Entity {
public:
    Cultist(FactionId faction, Vec3 pos, float maxHp = 100.0f)
        : Entity(EntityType::Cultist, faction, pos, maxHp) {}

    CultistState state() const { return state_; }
    void setState(CultistState s) { state_ = s; }

    float devotion() const { return devotion_; } // 0..100
    void setDevotion(float d) { devotion_ = d < 0 ? 0 : (d > 100 ? 100 : d); }

private:
    CultistState state_ = CultistState::Loyal;
    float devotion_ = 60.0f;
};

class Civilian : public Entity {
public:
    Civilian(Vec3 pos, float maxHp = 50.0f)
        : Entity(EntityType::Civilian, FACTION_NEUTRAL, pos, maxHp) {}
};

class Adventurer : public Entity {
public:
    Adventurer(Vec3 pos, float maxHp = 150.0f)
        : Entity(EntityType::Adventurer, FACTION_NEUTRAL, pos, maxHp) {}
};

class Creature : public Entity {
public:
    Creature(FactionId faction, Vec3 pos, std::string species, float maxHp = 80.0f)
        : Entity(EntityType::Creature, faction, pos, maxHp),
          species_(std::move(species)) {}

    std::string species() const override { return species_; }
    bool captured() const { return captured_; }
    void setCaptured(bool c) { captured_ = c; }

private:
    std::string species_;
    bool captured_ = false;
};

class Monstrosity : public Creature {
public:
    Monstrosity(FactionId faction, Vec3 pos, std::string species, bool feral,
                float maxHp = 300.0f)
        : Creature(faction, pos, std::move(species), maxHp), feral_(feral) {
        // NOTE: type stays Creature for the base ctor; fix it up here.
        type_ = EntityType::Monstrosity;
    }

    bool feral() const { return feral_; }

private:
    bool feral_;
};

class Dhole : public Monstrosity {
public:
    // Wave 13: the burrower of the Vale of Pnath ("Dholes" is on the legal
    // allowlist). High-HP ambusher with a dread (fear) aura. Starts
    // burrowed — untargetable by convention until it surfaces to strike,
    // then it may re-burrow. No CC0 dhole model exists; ModelCatalog maps
    // "dhole" to "" and the procedural serpent/worm-like fallback applies.
    static constexpr float DHOLE_MAX_HP = 1500.0f;
    static constexpr float FEAR_AURA_RADIUS = 18.0f;
    static constexpr float FEAR_AURA_STRENGTH = 12.0f; // fear per second

    Dhole(FactionId faction, Vec3 pos)
        : Monstrosity(faction, pos, "dhole", /*feral=*/true, DHOLE_MAX_HP) {}

    bool burrowed() const { return burrowed_; }
    void surface() { burrowed_ = false; }
    void burrow() { burrowed_ = true; }
    float fearAuraRadius() const { return FEAR_AURA_RADIUS; }
    float fearAuraStrength() const { return FEAR_AURA_STRENGTH; }

private:
    bool burrowed_ = true;
};

class Mimic : public Entity {
public:
    Mimic(FactionId faction, Vec3 pos, float maxHp = 120.0f)
        : Entity(EntityType::Mimic, faction, pos, maxHp) {}

    bool disguised() const { return disguised_; }
    void setDisguised(bool d) { disguised_ = d; }

private:
    bool disguised_ = true;
};

class Sorcerer : public Entity {
public:
    Sorcerer(FactionId faction, Vec3 pos, float maxHp = 120.0f)
        : Entity(EntityType::Sorcerer, faction, pos, maxHp) {}

    float mana() const { return mana_; }
    void setMana(float m) { mana_ = m < 0 ? 0 : (m > maxMana_ ? maxMana_ : m); }
    float maxMana() const { return maxMana_; }

private:
    float mana_ = 100.0f;
    float maxMana_ = 100.0f;
};

} // namespace cultulhu
