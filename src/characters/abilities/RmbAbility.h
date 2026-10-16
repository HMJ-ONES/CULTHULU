#pragma once

// CULT-ULHU per-character right-click ability framework (wave 7).
//
// Every playable character defines its own RMB kit as a state machine
// implementing RmbAbility. The framework is deliberately engine-agnostic:
// the game layer feeds abstract input edges (press/release/mouse-move/
// left-click) plus a world snapshot, and the ability drives gameplay
// (positions, CC, damage, events) directly. The engine binding maps
// suggestedAnimState() onto real animation states and wires the input
// edges to the mouse.
//
// "Wave of Domination" (Cthulhu Avatar) is the reference implementation
// and the template for how per-character RMB kits should feel.

#include "animation/AnimationStateMachine.h"
#include "combat/CrowdControl.h"
#include "core/EventBus.h"
#include "core/RNG.h"
#include "core/Vec3.h"
#include "entities/Entity.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace cultulhu {

// Everything an RMB ability needs for one update/input edge. Filled by
// the game layer each tick; the ability never touches the OS.
struct RmbContext {
    Entity* caster = nullptr;   // the character using the ability
    float casterYaw = 0.0f;      // radians, caster facing

    // Explicit single entity under the cursor (0 = none). Some abilities
    // (e.g. Wave of Domination's direct stun) behave differently when the
    // player aimed at a specific target.
    uint64_t targetedEntityId = 0;

    // World candidates the ability may affect (units, buildings, ...).
    std::vector<Entity*> entities;

    EventBus& bus;
    ActiveEffects& effects;
    const BeliefSystem& beliefs;
    RNG& rng;

    // Ground height at a world position (default: flat y = 0). Used for
    // lethal-fall checks and gentle drops.
    std::function<float(Vec3)> groundHeight;

    RmbContext(EventBus& b, ActiveEffects& e, const BeliefSystem& bl, RNG& r)
        : bus(b), effects(e), beliefs(bl), rng(r) {
        groundHeight = [](Vec3) { return 0.0f; };
    }
};

// Abstract right-click ability: a small input-driven state machine owned
// by one character. Implementations must be re-entrant across ticks via
// update(); all timing uses the dt passed in (no wall clock).
class RmbAbility {
public:
    virtual ~RmbAbility() = default;

    virtual const char* abilityId() const = 0;    // registry key
    virtual const char* displayName() const = 0;  // UI name
    virtual float cooldownSec() const = 0;       // full cooldown
    virtual bool ready() const = 0;              // may be cast now

    // Input edges from the game layer.
    virtual void onPress(RmbContext& ctx) = 0;    // RMB pressed
    virtual void onRelease(RmbContext& ctx) = 0; // RMB released
    virtual void onMouseMove(RmbContext& ctx, float dx, float dy) = 0;
    virtual void onLeftClick(RmbContext& ctx) = 0; // LMB while RMB held

    // Per-tick advance. Drives wavefronts, anchors, projectiles, cooldown.
    virtual void update(RmbContext& ctx, double dt) = 0;

    // Animation hook for the caster this tick (engine maps it).
    virtual AnimationState suggestedCasterState() const = 0;
};

// Factory: ability id -> fresh instance. New characters register their
// RMB kits here; CharacterDef.rmbAbilityId names which one to build.
std::unique_ptr<RmbAbility> createRmbAbility(const std::string& id);

} // namespace cultulhu
