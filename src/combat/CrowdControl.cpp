#include "combat/CrowdControl.h"
#include "beliefs/BeliefSystem.h"
#include "combat/Combat.h"
#include "core/EventBus.h"

#include <algorithm>

namespace cultulhu {

const char* ccTypeName(CCType type) {
    switch (type) {
        case CCType::Stun: return "Stun";
        case CCType::Slow: return "Slow";
        case CCType::Root: return "Root";
        case CCType::Fear: return "Fear";
        case CCType::Count: break;
    }
    return "Unknown";
}

void ActiveEffects::apply(uint64_t entityId, CCType type,
                          float baseDurationSeconds,
                          const BeliefSystem& beliefs, EventBus& bus) {
    float duration = combat::ccDuration(baseDurationSeconds, beliefs);
    if (duration <= 0.0f) return;

    CCEffect& eff = fx_[entityId][static_cast<int>(type)];
    if (type == CCType::Slow) {
        // Re-slowing stacks multiplicatively toward a 0.2 floor; a fresh
        // Slow starts at the default 0.5 movement factor.
        eff.type = CCType::Slow;
        eff.slowFactor = std::max(0.2f, eff.slowFactor * 0.5f);
        eff.totalDuration = std::max(eff.totalDuration, duration);
        eff.remaining = std::max(eff.remaining, duration);
    } else {
        // Stun / Root / Fear refresh to the longest remaining duration.
        eff.type = type;
        eff.totalDuration = std::max(eff.totalDuration, duration);
        eff.remaining = std::max(eff.remaining, duration);
    }

    GameEvent e(EventType::CCApplied);
    e.targetId = entityId;
    e.amount = duration;
    e.tag = ccTypeName(type);
    bus.publish(e);
}

void ActiveEffects::tick(double dt) {
    float step = static_cast<float>(dt);
    for (auto it = fx_.begin(); it != fx_.end();) {
        for (auto jit = it->second.begin(); jit != it->second.end();) {
            jit->second.remaining -= step;
            if (jit->second.remaining <= 0.0f)
                jit = it->second.erase(jit);
            else
                ++jit;
        }
        if (it->second.empty())
            it = fx_.erase(it);
        else
            ++it;
    }
}

const CCEffect* ActiveEffects::find(uint64_t id, CCType type) const {
    auto it = fx_.find(id);
    if (it == fx_.end()) return nullptr;
    auto jit = it->second.find(static_cast<int>(type));
    return jit == it->second.end() ? nullptr : &jit->second;
}

bool ActiveEffects::has(uint64_t id, CCType type) const {
    return find(id, type) != nullptr;
}

bool ActiveEffects::isStunned(uint64_t id) const {
    return has(id, CCType::Stun) || has(id, CCType::Fear);
}

bool ActiveEffects::isRooted(uint64_t id) const {
    return has(id, CCType::Root) || has(id, CCType::Stun);
}

float ActiveEffects::moveMultiplier(uint64_t id) const {
    if (isStunned(id) || isRooted(id)) return 0.0f;
    float mult = 1.0f;
    auto it = fx_.find(id);
    if (it != fx_.end()) {
        for (const auto& kv : it->second) {
            if (kv.second.type == CCType::Slow)
                mult *= kv.second.slowFactor;
        }
    }
    return mult;
}

size_t ActiveEffects::activeCount() const {
    size_t n = 0;
    for (const auto& kv : fx_) n += kv.second.size();
    return n;
}

} // namespace cultulhu
