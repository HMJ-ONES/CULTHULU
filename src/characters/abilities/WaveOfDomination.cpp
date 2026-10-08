#include "characters/abilities/WaveOfDomination.h"

#include "entities/Units.h"

#include <cmath>

namespace cultulhu {

bool WaveOfDomination::isSusceptible(const Entity* e) {
    if (e == nullptr || !e->alive()) return false;
    switch (e->type()) {
        case EntityType::Civilian:
        case EntityType::Adventurer:
            return true;
        case EntityType::Cultist: {
            // Chaos-aligned madness shields the mind: lunatics (driven
            // lunatic by the Chaos belief) are immune to domination.
            const auto* c = static_cast<const Cultist*>(e);
            return c->state() != CultistState::Lunatic;
        }
        default:
            // Monstrosities (feral or not), mimics, sorcerers, creatures,
            // buildings and relics are unaffected by the wave.
            return false;
    }
}

void WaveOfDomination::onPress(RmbContext& ctx) {
    if (!ready() || ctx.caster == nullptr) return;

    // Alternate cast: explicit single target under the cursor -> direct
    // stun, no wave, no levitation. Same cooldown.
    if (ctx.targetedEntityId != 0) {
        Entity* t = findEntity(ctx, ctx.targetedEntityId);
        if (t != nullptr && t->alive()) {
            ctx.effects.apply(t->id(), CCType::Stun, kDirectStunSec,
                              ctx.beliefs, ctx.bus);
            GameEvent e(EventType::DirectStunApplied);
            e.sourceId = ctx.caster->id();
            e.targetId = t->id();
            e.amount = kDirectStunSec;
            e.pos = t->position();
            ctx.bus.publish(e);
            cooldown_ = kCooldownSec;
            return;
        }
        // Target id did not resolve: fall through to wave mode.
    }

    phase_ = Phase::Wave;
    waveDist_ = 0.0f;
    waveOrigin_ = ctx.caster->position();
    waveYaw_ = ctx.casterYaw;
    victims_.clear();
    swingOffset_ = Vec3{};
    swingTarget_ = Vec3{};
    cooldown_ = kCooldownSec;
}

void WaveOfDomination::onRelease(RmbContext& ctx) {
    pruneStaleVictims(ctx);
    if (phase_ == Phase::Hold) {
        dropVictims(ctx);
    } else if (phase_ == Phase::Wave) {
        // Released before anything was caught: the wave dissipates.
        victims_.clear();
        phase_ = Phase::Idle;
    }
}

void WaveOfDomination::onMouseMove(RmbContext& ctx, float dx, float dy) {
    (void)ctx;
    if (phase_ != Phase::Hold) return;
    swingTarget_.x += dx * kMouseGain;
    swingTarget_.y += dy * kMouseGain;
    const float len = std::sqrt(swingTarget_.x * swingTarget_.x +
                                swingTarget_.y * swingTarget_.y);
    if (len > kMaxSwing && len > 0.0f) {
        const float s = kMaxSwing / len;
        swingTarget_.x *= s;
        swingTarget_.y *= s;
    }
}

void WaveOfDomination::onLeftClick(RmbContext& ctx) {
    pruneStaleVictims(ctx);
    if (phase_ == Phase::Hold && !victims_.empty()) launchVictims(ctx);
}

void WaveOfDomination::cancel() {
    victims_.clear();
    phase_ = Phase::Idle;
    waveDist_ = 0.0f;
    swingOffset_ = Vec3{};
    swingTarget_ = Vec3{};
}

void WaveOfDomination::pruneStaleVictims(const RmbContext& ctx) {
    for (size_t i = victims_.size(); i-- > 0;) {
        Entity* e = victims_[i].entity;
        bool live = false;
        for (Entity* c : ctx.entities) {
            if (c != nullptr && c == e) {
                live = true;
                break;
            }
        }
        if (!live) victims_.erase(victims_.begin() + i);
    }
    if (victims_.empty() &&
        (phase_ == Phase::Hold || phase_ == Phase::Flying))
        phase_ = Phase::Idle;
}

void WaveOfDomination::update(RmbContext& ctx, double dt) {
    if (cooldown_ > 0.0f) {
        cooldown_ -= static_cast<float>(dt);
        if (cooldown_ < 0.0f) cooldown_ = 0.0f;
    }
    // Never touch a victim the world no longer owns (see prune above).
    pruneStaleVictims(ctx);
    switch (phase_) {
        case Phase::Wave: {
            waveDist_ += kWaveSpeed * static_cast<float>(dt);
            catchSweep(ctx);
            if (waveDist_ >= kWaveRange) {
                phase_ = victims_.empty() ? Phase::Idle : Phase::Hold;
            }
            break;
        }
        case Phase::Hold:
            updateHold(ctx, dt);
            break;
        case Phase::Flying:
            updateFlying(ctx, dt);
            break;
        case Phase::Idle:
            break;
    }
}

AnimationState WaveOfDomination::suggestedCasterState() const {
    switch (phase_) {
        case Phase::Wave:   return AnimationState::CastWave;
        case Phase::Hold:   return AnimationState::Levitate;
        case Phase::Flying: return AnimationState::Launch;
        case Phase::Idle:   return AnimationState::Idle;
    }
    return AnimationState::Idle;
}

Vec3 WaveOfDomination::anchorBase(const RmbContext& ctx) const {
    const Vec3 p = ctx.caster->position();
    const Vec3 f = forward(ctx.casterYaw);
    return Vec3{p.x + f.x * kAnchorDist, p.y + kAnchorHeight,
                p.z + f.z * kAnchorDist};
}

void WaveOfDomination::catchSweep(RmbContext& ctx) {
    const Vec3 f = forward(waveYaw_);
    const Vec3 r = right(waveYaw_);
    for (Entity* e : ctx.entities) {
        if (e == nullptr || e == ctx.caster || !e->alive()) continue;
        if (isVictim(e->id())) continue;
        if (victims_.size() >= static_cast<size_t>(kMaxVictims)) return;
        const Vec3 rel{e->position().x - waveOrigin_.x, 0.0f,
                       e->position().z - waveOrigin_.z};
        const float along = rel.x * f.x + rel.z * f.z;
        const float lateral = rel.x * r.x + rel.z * r.z;
        if (along <= 0.0f) continue;
        if (std::fabs(along - waveDist_) > kWaveThickness * 0.5f) continue;
        if (std::fabs(lateral) > kWaveHalfWidth) continue;
        if (!isSusceptible(e)) continue;

        Victim v;
        v.entity = e;
        const float a = 2.0f * 3.14159265f *
                        static_cast<float>(victims_.size()) /
                        static_cast<float>(kMaxVictims);
        v.ringOffset = Vec3{std::cos(a) * 1.2f, 0.0f, std::sin(a) * 1.2f};
        victims_.push_back(v);
        ctx.effects.apply(e->id(), CCType::Root, kRootRefreshSec,
                          ctx.beliefs, ctx.bus);

        GameEvent ev(EventType::VictimLevitated);
        ev.sourceId = ctx.caster->id();
        ev.targetId = e->id();
        ev.pos = e->position();
        ctx.bus.publish(ev);
    }
}

void WaveOfDomination::updateHold(RmbContext& ctx, double dt) {
    // Smooth the mouse-driven swing offset.
    const float t = std::min(1.0f, kAnchorSmooth * static_cast<float>(dt));
    swingOffset_.x += (swingTarget_.x - swingOffset_.x) * t;
    swingOffset_.y += (swingTarget_.y - swingOffset_.y) * t;

    const Vec3 base = anchorBase(ctx);
    const Vec3 r = right(ctx.casterYaw);
    const Vec3 anchor{base.x + r.x * swingOffset_.x,
                      base.y + swingOffset_.y,
                      base.z + r.z * swingOffset_.x};

    // Walk backwards so removals are safe.
    for (size_t i = victims_.size(); i-- > 0;) {
        Victim& v = victims_[i];
        if (!v.entity->alive()) {
            victims_.erase(victims_.begin() + i);
            continue;
        }
        v.entity->setPosition(
            Vec3{anchor.x + v.ringOffset.x, anchor.y + v.ringOffset.y,
                 anchor.z + v.ringOffset.z});
        ctx.effects.apply(v.entity->id(), CCType::Root, kRootRefreshSec,
                          ctx.beliefs, ctx.bus);

        Entity* hit = findImpact(ctx, v.entity->position(), v.entity);
        if (hit != nullptr) {
            impactVictim(ctx, v, hit, kSlamDamage, "slam");
            victims_.erase(victims_.begin() + i);
        }
    }
    if (victims_.empty()) phase_ = Phase::Idle;
}

void WaveOfDomination::updateFlying(RmbContext& ctx, double dt) {
    const float step = kLaunchSpeed * static_cast<float>(dt);
    for (size_t i = victims_.size(); i-- > 0;) {
        Victim& v = victims_[i];
        if (!v.entity->alive()) {
            victims_.erase(victims_.begin() + i);
            continue;
        }
        Vec3 p = v.entity->position();
        p.x += v.velocity.x * static_cast<float>(dt);
        p.z += v.velocity.z * static_cast<float>(dt);
        // Arc slightly downward for drama; the engine renders the rest.
        p.y -= 2.0f * static_cast<float>(dt);
        v.entity->setPosition(p);
        v.flown += step;

        Entity* hit = findImpact(ctx, p, v.entity);
        if (hit != nullptr) {
            impactVictim(ctx, v, hit, kLaunchDamage, "launch");
            victims_.erase(victims_.begin() + i);
        } else if (v.flown >= kLaunchMaxDist) {
            // Ran out of sky: falls, survives with a stagger.
            dropOne(ctx, v, false);
            victims_.erase(victims_.begin() + i);
        }
    }
    if (victims_.empty()) phase_ = Phase::Idle;
}

Entity* WaveOfDomination::findImpact(const RmbContext& ctx, const Vec3& p,
                                     const Entity* self) const {
    for (Entity* e : ctx.entities) {
        if (e == nullptr || e == ctx.caster || e == self) continue;
        if (!e->alive()) continue;
        if (isVictim(e->id())) continue; // victims don't collide mid-swing
        const float radius = (e->type() == EntityType::Building)
                                 ? kBuildingHitRadius
                                 : kEntityHitRadius;
        // Top-down collision (XZ): victims float, targets stand.
        const float dx = e->position().x - p.x;
        const float dz = e->position().z - p.z;
        if (dx * dx + dz * dz <= radius * radius) return e;
    }
    return nullptr;
}

void WaveOfDomination::impactVictim(RmbContext& ctx, Victim& v, Entity* hit,
                                    float dmg, const char* tag) {
    hit->takeDamage(dmg);
    v.entity->takeDamage(1000000.0f); // the victim does not survive impact

    GameEvent ev(EventType::VictimSlammed);
    ev.sourceId = ctx.caster->id();
    ev.targetId = v.entity->id();
    ev.amount = dmg;
    ev.tag = tag;
    ev.pos = v.entity->position();
    ctx.bus.publish(ev);

    emitSlain(ctx, v.entity);
}

void WaveOfDomination::emitSlain(RmbContext& ctx, Entity* victim) {
    // All domination kills feed Onslaught through the civilian-slain feed.
    GameEvent e(EventType::CivilianSlain);
    e.sourceId = ctx.caster->id();
    e.targetId = victim->id();
    e.amount = 1.0f;
    e.pos = victim->position();
    ctx.bus.publish(e);
}

void WaveOfDomination::dropOne(RmbContext& ctx, Victim& v, bool fromHold) {
    (void)fromHold;
    const float ground = ctx.groundHeight(v.entity->position());
    const float height = v.entity->position().y - ground;
    GameEvent ev(EventType::VictimDropped);
    ev.sourceId = ctx.caster->id();
    ev.targetId = v.entity->id();
    ev.pos = v.entity->position();
    if (height > kLethalDropHeight) {
        v.entity->takeDamage(1000000.0f); // the fall kills
        ev.amount = 1.0f;
        ctx.bus.publish(ev);
        emitSlain(ctx, v.entity);
    } else {
        Vec3 p = v.entity->position();
        p.y = ground;
        v.entity->setPosition(p);
        ctx.effects.apply(v.entity->id(), CCType::Stun, kDropStaggerSec,
                          ctx.beliefs, ctx.bus);
        ev.amount = 0.0f;
        ctx.bus.publish(ev);
    }
}

void WaveOfDomination::dropVictims(RmbContext& ctx) {
    for (auto& v : victims_) {
        if (v.entity->alive()) dropOne(ctx, v, true);
    }
    victims_.clear();
    phase_ = Phase::Idle;
}

void WaveOfDomination::launchVictims(RmbContext& ctx) {
    GameEvent ev(EventType::VictimsLaunched);
    ev.sourceId = ctx.caster->id();
    ev.amount = static_cast<float>(victims_.size());
    ev.pos = ctx.caster->position();
    ctx.bus.publish(ev);

    const Vec3 f = forward(ctx.casterYaw);
    for (auto& v : victims_) {
        v.flying = true;
        v.velocity = Vec3{f.x * kLaunchSpeed, 0.0f, f.z * kLaunchSpeed};
        v.flown = 0.0f;
    }
    phase_ = Phase::Flying;
}

Entity* WaveOfDomination::findEntity(const RmbContext& ctx,
                                     uint64_t id) const {
    for (Entity* e : ctx.entities) {
        if (e != nullptr && e->id() == id) return e;
    }
    return nullptr;
}

bool WaveOfDomination::isVictim(uint64_t id) const {
    for (const auto& v : victims_) {
        if (v.entity != nullptr && v.entity->id() == id) return true;
    }
    return false;
}

} // namespace cultulhu
