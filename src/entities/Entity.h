#pragma once

#include "animation/AnimationStateMachine.h"
#include "core/Vec3.h"

#include <cstdint>
#include <string>

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
    Artifact,
    Altar // wave 7: ritual site entity (appended last; keep it last)
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

    // Species tag for creatures/monstrosities ("" for everything else).
    // Used by combat kill instrumentation (MonstrositySlain).
    virtual std::string species() const { return ""; }

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

    // Wave 18: every entity owns a headless-safe animation state machine.
    // Behavior code drives it directly, e.g.
    //     entity.anim().requestState(AnimationState::FearRun);
    // The machine works with no clips bound (procedural fallback) so the
    // simulation runs with zero art assets; the engine binding later binds
    // real FBX/glTF clips per state.
    AnimationStateMachine& anim() { return anim_; }
    const AnimationStateMachine& anim() const { return anim_; }

    // Wave 18: directory holding this entity's custom .canim clips
    // (e.g. "assets/characters/cultist_hooded/animations"), or "" for none.
    // bindEntityClips() loads and binds them (by CLIP name) before the
    // embedded-gltf / procedural steps. Set at spawn by the game layer.
    void setAnimPackDir(std::string dir) { animPackDir_ = std::move(dir); }
    const std::string& animPackDir() const { return animPackDir_; }

    // Default update advances the animation machine (one-shot states
    // auto-return to Idle here). Subclasses with their own update() that
    // skip the base call (e.g. buildings) simply don't tick animation.
    virtual void update(double dt) { anim_.update(dt); }

protected:
    uint64_t id_;
    EntityType type_;
    FactionId faction_;
    Vec3 pos_;
    float hp_;
    float maxHp_;
    bool oneHitKilled_ = false;

    AnimationStateMachine anim_; // wave 18: per-entity animation state
    std::string animPackDir_;     // wave 18: custom .canim clips dir ("")

private:
    static uint64_t nextId_;
};

} // namespace cultulhu
