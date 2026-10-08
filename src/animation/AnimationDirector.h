#pragma once

#include "animation/AnimationStateMachine.h"
#include "core/Events.h"
#include "entities/Entity.h"

#include <cstdint>
#include <unordered_map>

namespace cultulhu {

class EventBus;

// Wave 18: bridges gameplay EVENTS to per-entity animation state. Behavior
// code (AI, combat, rituals) usually holds an entity reference and can call
// entity.anim().requestState(...) directly, but events only carry entity
// IDS (e.g. BrawlBrokeOut names two brawlers). The director tracks live
// entities by id and applies the animation side of those events, so no
// gameplay system needs a global entity registry of its own.
//
// Headless-safe: requesting a state on an unknown id is a no-op, and
// update() advances every tracked machine (one-shot states auto-return to
// Idle through AnimationStateMachine::update).
//
// Wiring (see driver/main.cpp): the game layer calls track() as entities
// spawn and tick(dt) once per tick. Dead entities are pruned automatically
// (their anim state is terminal — Death refuses all transitions), and
// clear() drops everything (world reload).
class AnimationDirector {
public:
    explicit AnimationDirector(EventBus& bus);

    // Track an entity by its id. Re-tracking the same id replaces it.
    void track(Entity& e) { tracked_[e.id()] = &e; }
    void untrack(uint64_t id) { tracked_.erase(id); }
    void untrack(const Entity& e) { untrack(e.id()); }
    void clear() { tracked_.clear(); }
    size_t tracked() const { return tracked_.size(); }

    // Advance all tracked animation machines; prunes dead entities.
    void tick(double dt);

    // Request a state on a tracked entity; false when the id is unknown.
    bool requestState(uint64_t id, AnimationState s,
                      double blendSeconds = 0.25);

private:
    EventBus& bus_;
    std::unordered_map<uint64_t, Entity*> tracked_;

    Entity* find(uint64_t id) const;
    void setBoth(uint64_t a, uint64_t b, AnimationState s);

    // Event handlers (subscribed in the constructor).
    void onBrawlBrokeOut(const GameEvent& e);
    void onSacrificeStarted(const GameEvent& e);
    void onSacrificeCompleted(const GameEvent& e);
    void onSacrificeInterrupted(const GameEvent& e);
    void onMaulStruck(const GameEvent& e);
};

} // namespace cultulhu
