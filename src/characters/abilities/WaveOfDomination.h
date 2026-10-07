#pragma once

// CULT-ULHU "Wave of Domination" — Cthulhu Avatar's right-click ability.
//
// A mind-control wave travels forward from Cthulhu. It catches civilians,
// adventurers and non-Chaos cultists (Chaos-aligned lunatics are immune:
// madness shields the mind; feral monstrosities are mindless and immune).
// While RMB is held the victims levitate at an anchor floating in front of
// Cthulhu and can be swung with the mouse. Slamming a victim into a
// building or another entity deals heavy damage and kills the victim;
// pressing LMB hurls all victims as projectiles (2x damage, they die);
// releasing RMB drops them gently (they live, briefly staggered) unless
// dropped from lethal height, in which case the fall kills them.
// Kills feed Onslaught exertion through the existing CivilianSlain event.
//
// Alternate cast: RMB pressed with an explicit entity targeted under the
// cursor stuns that entity for 2.5 s instead (no wave, no levitation).
//
// Cooldown 7 s starts on cast. This is the reference implementation for
// per-character RMB state machines (see RmbAbility.h).

#include "characters/abilities/RmbAbility.h"

namespace cultulhu {

class WaveOfDomination : public RmbAbility {
public:
    // ---- Tuning (full table in README) ----
    static constexpr float kCooldownSec = 7.0f;
    static constexpr float kWaveRange = 25.0f;      // meters
    static constexpr float kWaveSpeed = 12.0f;      // m/s
    static constexpr float kWaveHalfWidth = 2.0f;   // lateral half-width (m)
    static constexpr float kWaveThickness = 1.0f;   // wavefront band (m)
    static constexpr int kMaxVictims = 5;
    static constexpr float kAnchorDist = 3.0f;       // anchor meters in front
    static constexpr float kAnchorHeight = 2.0f;     // anchor meters up
    static constexpr float kMouseGain = 0.02f;       // anchor m per mouse unit
    static constexpr float kMaxSwing = 6.0f;         // max swing offset (m)
    static constexpr float kAnchorSmooth = 10.0f;    // lerp rate (1/s)
    static constexpr float kSlamDamage = 150.0f;
    static constexpr float kLaunchDamage = 300.0f;   // 2x slam
    static constexpr float kLaunchSpeed = 30.0f;     // m/s
    static constexpr float kLaunchMaxDist = 40.0f;
    static constexpr float kBuildingHitRadius = 2.5f;
    static constexpr float kEntityHitRadius = 1.2f;
    static constexpr float kLethalDropHeight = 8.0f;
    static constexpr float kDropStaggerSec = 1.0f;   // Stun on gentle drop
    static constexpr float kDirectStunSec = 2.5f;
    static constexpr float kRootRefreshSec = 0.6f;   // immobilize refresh

    enum class Phase { Idle, Wave, Hold, Flying };

    WaveOfDomination() = default;

    const char* abilityId() const override { return "wave_of_domination"; }
    const char* displayName() const override { return "Wave of Domination"; }
    float cooldownSec() const override { return kCooldownSec; }
    bool ready() const override {
        return cooldown_ <= 0.0f && phase_ == Phase::Idle;
    }

    void onPress(RmbContext& ctx) override;
    void onRelease(RmbContext& ctx) override;
    void onMouseMove(RmbContext& ctx, float dx, float dy) override;
    void onLeftClick(RmbContext& ctx) override;
    void update(RmbContext& ctx, double dt) override;
    AnimationState suggestedCasterState() const override;

    // Inspection for tests / game layer.
    Phase phase() const { return phase_; }
    size_t victimCount() const { return victims_.size(); }
    float cooldownRemaining() const { return cooldown_; }

    // Susceptibility rule (documented): civilians, adventurers, and
    // cultists of any faith except Chaos. Chaos-aligned lunatics are
    // immune (madness shields the mind); feral monstrosities are immune
    // (mindless). Everything else is unaffected by the wave.
    static bool isSusceptible(const Entity* e);

private:
    struct Victim {
        Entity* entity = nullptr;
        Vec3 ringOffset;   // fixed offset around the swing anchor
        bool flying = false;
        Vec3 velocity;
        float flown = 0.0f; // distance travelled as projectile
    };

    Phase phase_ = Phase::Idle;
    float cooldown_ = 0.0f;
    float waveDist_ = 0.0f;
    Vec3 waveOrigin_;
    float waveYaw_ = 0.0f;
    Vec3 swingOffset_;  // smoothed (x = lateral, y = vertical)
    Vec3 swingTarget_;

    std::vector<Victim> victims_;

    static Vec3 forward(float yaw) {
        return Vec3{std::cos(yaw), 0.0f, std::sin(yaw)};
    }
    static Vec3 right(float yaw) {
        return Vec3{-std::sin(yaw), 0.0f, std::cos(yaw)};
    }

    Vec3 anchorBase(const RmbContext& ctx) const;
    void catchSweep(RmbContext& ctx);
    void updateHold(RmbContext& ctx, double dt);
    void updateFlying(RmbContext& ctx, double dt);
    // Returns the building/entity hit at p (excluding caster and victims),
    // or nullptr. Radius depends on target kind (buildings are bigger).
    Entity* findImpact(const RmbContext& ctx, const Vec3& p,
                       const Entity* self) const;
    void impactVictim(RmbContext& ctx, Victim& v, Entity* hit, float dmg,
                      const char* tag);
    void emitSlain(RmbContext& ctx, Entity* victim);
    void dropOne(RmbContext& ctx, Victim& v, bool fromHold);
    void dropVictims(RmbContext& ctx);
    void launchVictims(RmbContext& ctx);
    Entity* findEntity(const RmbContext& ctx, uint64_t id) const;
    bool isVictim(uint64_t id) const;
};

} // namespace cultulhu
