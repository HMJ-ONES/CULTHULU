#include "animation/AnimationDirector.h"

#include "core/EventBus.h"

namespace cultulhu {

AnimationDirector::AnimationDirector(EventBus& bus) : bus_(bus) {
    bus_.subscribe(EventType::BrawlBrokeOut,
                   [this](const GameEvent& e) { onBrawlBrokeOut(e); });
    bus_.subscribe(EventType::SacrificeStarted,
                   [this](const GameEvent& e) { onSacrificeStarted(e); });
    bus_.subscribe(EventType::SacrificeCompleted,
                   [this](const GameEvent& e) { onSacrificeCompleted(e); });
    bus_.subscribe(EventType::SacrificeInterrupted,
                   [this](const GameEvent& e) { onSacrificeInterrupted(e); });
    bus_.subscribe(EventType::MaulStruck,
                   [this](const GameEvent& e) { onMaulStruck(e); });
}

void AnimationDirector::tick(double dt) {
    for (auto it = tracked_.begin(); it != tracked_.end();) {
        Entity* e = it->second;
        if (!e || !e->alive()) {
            // Dead (or gone) entities leave the stage: their anim state is
            // terminal anyway, and pruning here keeps the registry from
            // outliving dismissed cultists / cleared worlds.
            it = tracked_.erase(it);
            continue;
        }
        e->update(dt); // base Entity::update advances the anim machine
        ++it;
    }
}

bool AnimationDirector::requestState(uint64_t id, AnimationState s,
                                     double blendSeconds) {
    Entity* e = find(id);
    if (!e) return false;
    e->anim().requestState(s, blendSeconds);
    return true;
}

Entity* AnimationDirector::find(uint64_t id) const {
    if (id == 0) return nullptr;
    auto it = tracked_.find(id);
    return it != tracked_.end() ? it->second : nullptr;
}

void AnimationDirector::setBoth(uint64_t a, uint64_t b, AnimationState s) {
    requestState(a, s);
    requestState(b, s);
}

void AnimationDirector::onBrawlBrokeOut(const GameEvent& e) {
    // Both brawlers throw wild punches. Brawl is a 0.9s one-shot that
    // auto-returns to Idle; no manual reset needed.
    setBoth(e.sourceId, e.targetId, AnimationState::Brawl);
}

void AnimationDirector::onSacrificeStarted(const GameEvent& e) {
    // The rite begins: the priest performs, the victim is bound. Both clips
    // loop for the rite's duration.
    requestState(e.sourceId, AnimationState::SacrificePerformer);
    requestState(e.targetId, AnimationState::SacrificeVictim);
}

void AnimationDirector::onSacrificeCompleted(const GameEvent& e) {
    // Completion re-asserts the rite poses (covers rites started without a
    // SacrificeStarted event, e.g. abstract sim publishers); the performer
    // is released back to Idle by gameplay once the aftermath resolves.
    requestState(e.sourceId, AnimationState::SacrificePerformer);
    requestState(e.targetId, AnimationState::SacrificeVictim);
}

void AnimationDirector::onSacrificeInterrupted(const GameEvent& e) {
    // The rite is broken: everyone stands down.
    setBoth(e.sourceId, e.targetId, AnimationState::Idle);
}

void AnimationDirector::onMaulStruck(const GameEvent& e) {
    // The beast tears into its victim. Maul is a 0.9s one-shot on the
    // ATTACKER that auto-returns to Idle.
    requestState(e.sourceId, AnimationState::Maul);
}

} // namespace cultulhu
