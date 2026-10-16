#include "traps/TrapSystem.h"
#include "core/EventBus.h"

namespace cultulhu {

TrapSystem::TrapSystem(EventBus& bus) : bus_(bus) {}

uint64_t TrapSystem::placeTrap(Vec3 pos) {
    Trap t;
    t.id = nextId_++;
    t.pos = pos;
    traps_.push_back(t);
    return t.id;
}

const Trap* TrapSystem::get(uint64_t id) const {
    for (const auto& t : traps_)
        if (t.id == id) return &t;
    return nullptr;
}

bool TrapSystem::springTrap(uint64_t trapId, Entity& victim) {
    for (auto& t : traps_) {
        if (t.id != trapId || !t.armed) return false;
        EntityType ty = victim.type();
        if (!victim.alive() ||
            (ty != EntityType::Civilian && ty != EntityType::Adventurer))
            return false;
        t.armed = false; // single-use
        ++t.captured;

        GameEvent e(EventType::TrapSprung);
        e.sourceId = t.id;
        e.targetId = victim.id();
        e.pos = t.pos;
        e.amount = 1.0f;
        bus_.publish(e);
        return true;
    }
    return false;
}

} // namespace cultulhu
