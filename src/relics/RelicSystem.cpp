#include "relics/RelicSystem.h"
#include "core/EventBus.h"

namespace cultulhu {

RelicSystem::RelicSystem(EventBus& bus) : bus_(bus) {}

void RelicSystem::addRelic(float amplifier, const std::string& name) {
    if (amplifier <= 0.0f) return;
    HeldRelic r;
    r.amplifier = amplifier;
    r.name = name.empty() ? "an unnamed relic" : name;
    relics_.push_back(r);
}

void RelicSystem::removeRelic(float amplifier) {
    for (auto it = relics_.begin(); it != relics_.end(); ++it) {
        if (it->amplifier == amplifier) { relics_.erase(it); return; }
    }
}

float RelicSystem::powerMultiplier() const {
    float m = 1.0f;
    for (const auto& r : relics_) m += r.amplifier;
    if (m > MAX_MULTIPLIER) m = MAX_MULTIPLIER;
    return m;
}

void RelicSystem::seizeRelic(float amplifier, uint64_t claimerId,
                             const std::string& relicName) {
    addRelic(amplifier, relicName);
    GameEvent ev(EventType::RelicClaimed);
    ev.sourceId = claimerId;
    ev.amount = amplifier;
    ev.tag = relicName;
    bus_.publish(ev);
}

uint64_t RelicSystem::placeCursedArtifact(Vec3 pos, FactionId owner) {
    CursedArtifactTrap t;
    t.id = nextId_++;
    t.pos = pos;
    t.owner = owner;
    artifacts_.push_back(t);
    return t.id;
}

bool RelicSystem::checkTrigger(uint64_t id,
                              const std::vector<const Entity*>& entities,
                              float radius) {
    for (auto& t : artifacts_) {
        if (t.id != id || !t.armed) return false;
        for (const Entity* e : entities) {
            if (!e || !e->alive()) continue;
            if (e->faction() == t.owner) continue;
            if (t.pos.distance(e->position()) <= radius) {
                t.armed = false; // single-use
                GameEvent ev(EventType::ArtifactTriggered);
                ev.sourceId = t.id;
                ev.targetId = e->id();
                ev.faction = t.owner;
                ev.pos = t.pos;
                ev.amount = 1.0f;
                bus_.publish(ev);
                return true;
            }
        }
        return false;
    }
    return false;
}

} // namespace cultulhu
