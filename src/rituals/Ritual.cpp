#include "rituals/Ritual.h"
#include "core/EventBus.h"

namespace cultulhu {

void Ritual::interrupt() {
    if (!active_) return;
    active_ = false;
    interrupted_ = true;
    onInterrupted();
}

bool Ritual::update(double dt) {
    if (!active_) return false;
    elapsed_ += dt;
    if (elapsed_ >= duration_) {
        active_ = false;
        onComplete();
        return true;
    }
    return false;
}

void SacrificeRitual::onComplete() {
    GameEvent e(EventType::SacrificeCompleted);
    bus_.publish(e);
}

void SacrificeRitual::onInterrupted() {
    GameEvent e(EventType::SacrificeInterrupted);
    bus_.publish(e);
}

void NecromancyRitual::onComplete() {
    GameEvent e(EventType::NecromancyPerformed);
    bus_.publish(e);
}

void NecromancyRitual::onInterrupted() {
    // A failed necromancy simply fizzles (no power swing either way).
}

} // namespace cultulhu
