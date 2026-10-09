#include "ai/RivalBot.h"
#include "ai/AI.h"

namespace cultulhu {

Entity* RivalBot::nearestEnemy(const Context& ctx) const {
    // Wave 32: squared distances — no sqrt per check.
    Entity* best = nullptr;
    float bestDSq = SIGHT_RANGE * SIGHT_RANGE;
    if (ctx.avatar && ctx.avatar->alive()) {
        float dSq = position().distanceSq(ctx.avatar->position());
        if (dSq < bestDSq) { bestDSq = dSq; best = ctx.avatar; }
    }
    for (Entity* c : ctx.creatures) {
        if (!c || !c->alive()) continue;
        float dSq = position().distanceSq(c->position());
        if (dSq < bestDSq) { bestDSq = dSq; best = c; }
    }
    return best;
}

void RivalBot::updateWander(double dt) {
    if (idleTimer_ > 0.0) {
        idleTimer_ -= dt;
        return;
    }
    if (!hasWanderTarget_) {
        wanderTarget_ = position() + Vec3(rng_.uniform(-30.0f, 30.0f), 0.0f,
                                          rng_.uniform(-30.0f, 30.0f));
        hasWanderTarget_ = true;
    }
    if ((wanderTarget_ - position()).length() < 2.0f) {
        hasWanderTarget_ = false;
        idleTimer_ = rng_.uniform(2.0, 6.0);
        return;
    }
    ai::moveToward(*this, wanderTarget_, dt, SPEED * 0.55f);
}

void RivalBot::updateEngage(double dt, Entity& enemy) {
    float d = position().distance(enemy.position());
    if (d <= MELEE_RANGE) {
        if (attackCd_ <= 0.0) {
            enemy.takeDamage(MELEE_DAMAGE);
            attackCd_ = 1.0;
        }
        return; // hold ground and swing
    }
    ai::moveToward(*this, enemy.position(), dt, SPEED);
}

void RivalBot::updateFlee(double dt, const Entity& threat) {
    Vec3 away = position() - threat.position();
    if (away.length() < 1e-4f) away = Vec3(1, 0, 0);
    ai::moveToward(*this, position() + away.normalized() * 10.0f, dt, SPEED);
}

void RivalBot::update(double dt, const Context& ctx) {
    if (!alive()) return;
    if (attackCd_ > 0.0) attackCd_ -= dt;
    Entity* enemy = nearestEnemy(ctx);
    if (enemy && hp() / MAX_HP < FLEE_HP_FRACTION) {
        state_ = State::Flee;
        updateFlee(dt, *enemy);
    } else if (enemy) {
        state_ = State::Engage;
        updateEngage(dt, *enemy);
    } else {
        state_ = State::Wander;
        updateWander(dt);
    }
}

} // namespace cultulhu
