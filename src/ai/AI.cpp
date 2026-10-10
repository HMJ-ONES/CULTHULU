#include "ai/AI.h"

#include <limits>

namespace cultulhu {
namespace ai {

void moveToward(Entity& e, const Vec3& target, double dt, float speed) {
    Vec3 to = target - e.position();
    float dist = to.length();
    if (dist < 1e-4f) return;
    float step = speed * static_cast<float>(dt);
    if (step >= dist) e.setPosition(target);
    else e.setPosition(e.position() + to.normalized() * step);
}

void cultistFollow(Cultist& c, const Vec3& leaderPos, double dt, float speed) {
    // Imprisoned cultists cannot move; lunatics wander off (handled by the
    // Chaos belief's lunatic behavior elsewhere).
    if (c.state() == CultistState::Imprisoned) return;
    moveToward(c, leaderPos, dt, speed);
}

void civilianFlee(Civilian& c, const Vec3& threatPos, double dt, float speed) {
    Vec3 away = c.position() - threatPos;
    if (away.length() < 1e-4f) away = Vec3(1, 0, 0);
    moveToward(c, c.position() + away.normalized() * 10.0f, dt, speed);
    // Wave 18: the fleeing civilian runs in panic (loops like Run; the
    // caller/UE5 binding calls civilianCalm() when the threat is gone so
    // they are never stuck in FearRun).
    if (c.alive())
        c.anim().requestState(AnimationState::FearRun);
}

void civilianCalm(Civilian& c) {
    // The threat is gone: drop the panic pose back to Idle. requestState
    // no-ops when already Idle, and Death is never overridden.
    c.anim().requestState(AnimationState::Idle);
}

const Entity* selectRampageTarget(
    const Monstrosity& m, const std::vector<const Entity*>& entities) {
    const Entity* best = nullptr;
    float bestDist = std::numeric_limits<float>::max();
    for (const Entity* e : entities) {
        if (!e || e->id() == m.id() || !e->alive()) continue;
        if (e->faction() != FACTION_CTHULHU) continue; // rampages vs the cult
        float d = m.position().distance(e->position());
        if (d < bestDist) { bestDist = d; best = e; }
    }
    return best;
}

void rampageStep(Monstrosity& m, const std::vector<const Entity*>& entities,
                 double dt, float speed) {
    if (!m.feral()) return;
    const Entity* target = selectRampageTarget(m, entities);
    if (target) moveToward(m, target->position(), dt, speed);
}

} // namespace ai
} // namespace cultulhu
