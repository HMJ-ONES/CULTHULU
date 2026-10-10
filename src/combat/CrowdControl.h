#pragma once

#include "core/Events.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace cultulhu {

class BeliefSystem;
class EventBus;

// Crowd-control effect kinds.
enum class CCType {
    Stun,   // no action, no movement
    Slow,   // movement reduced by slowFactor
    Root,   // no movement, can still act
    Fear,   // unit flees; the AI layer interprets this as a timed effect
    Count
};

// Human-readable name for a CC type (also used as the CCApplied event tag).
const char* ccTypeName(CCType type);

// A single active crowd-control effect on one entity.
struct CCEffect {
    CCType type = CCType::Stun;
    float totalDuration = 0.0f; // seconds at application time
    float remaining = 0.0f;     // seconds left before expiry
    float slowFactor = 0.5f;    // Slow only: movement multiplier (0.2..1.0)
};

// Tracks timed crowd-control effects per entity id. Stun/Root/Fear
// refresh to the longest remaining duration; stacking Slow multiplies
// the movement factor down to a 0.2 floor.
class ActiveEffects {
public:
    // Apply a CC effect. The base duration is scaled by belief modifiers
    // (combat::ccDuration: Magic shortens foe CC). Publishes CCApplied with
    // tag=ccTypeName, amount=final duration, targetId=entityId.
    void apply(uint64_t entityId, CCType type, float baseDurationSeconds,
               const BeliefSystem& beliefs, EventBus& bus);

    // Advance all timers by dt seconds, erasing expired effects.
    void tick(double dt);

    bool has(uint64_t id, CCType type) const;
    bool isStunned(uint64_t id) const; // Stun or Fear
    bool isRooted(uint64_t id) const;  // Root or Stun

    // Movement multiplier: 0.0 if stunned/rooted, otherwise the product
    // of active slow factors (1.0 when nothing is slowing the entity).
    float moveMultiplier(uint64_t id) const;

    // Total number of active effects across all entities.
    size_t activeCount() const;

private:
    // entity id -> (cc type index -> effect)
    std::unordered_map<uint64_t, std::unordered_map<int, CCEffect>> fx_;

    const CCEffect* find(uint64_t id, CCType type) const;
};

} // namespace cultulhu
