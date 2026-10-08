#pragma once

#include "animation/AnimationClip.h"

#include <string>
#include <unordered_map>

namespace cultulhu {

// Animation states for any animated entity.
enum class AnimationState {
    Idle,
    Walk,
    Run,
    Attack,
    Cast,
    Stunned,
    Death,
    Channel, // rituals / long casts
    // Wave of Domination (Cthulhu Avatar RMB, wave 7) hooks.
    CastWave,  // mind-control wave travelling forward (one-shot)
    Levitate,  // caster holding levitated victims (loops while RMB held)
    Launch,    // hurling victims forward (one-shot)
    Levitated, // victim: floating, immobilized (loops while held)
    // Wave 18 hooks: fear/frenzy locomotion, brawling, sacrifice ritual.
    FearRun,            // panicked sprint (loops like Run)
    Brawl,              // wild melee brawling (one-shot, auto-returns like Attack)
    SacrificePerformer, // priest performing a sacrifice (loops like Channel,
                       // held during the rite)
    SacrificeVictim,    // bound victim kneeling/struggling (loops)
    Maul,               // beast pounce-and-tear (one-shot like Attack)
    // Wave 18 extension: disciplined combat and labor loops.
    WarBattle,          // disciplined war fighting (loops while engaged)
    Build,              // construction work (loops while building)
    Repair,             // fixing damaged structures (loops while repairing)
    Count
};

inline const char* animationStateName(AnimationState s) {
    switch (s) {
        case AnimationState::Idle:    return "Idle";
        case AnimationState::Walk:    return "Walk";
        case AnimationState::Run:     return "Run";
        case AnimationState::Attack:  return "Attack";
        case AnimationState::Cast:    return "Cast";
        case AnimationState::Stunned: return "Stunned";
        case AnimationState::Death:   return "Death";
        case AnimationState::Channel: return "Channel";
        case AnimationState::CastWave: return "CastWave";
        case AnimationState::Levitate: return "LevitateHold";
        case AnimationState::Launch:  return "Launch";
        case AnimationState::Levitated: return "Levitated";
        case AnimationState::FearRun: return "FearRun";
        case AnimationState::Brawl:   return "Brawl";
        case AnimationState::SacrificePerformer: return "SacrificePerformer";
        case AnimationState::SacrificeVictim:    return "SacrificeVictim";
        case AnimationState::Maul:    return "Maul";
        case AnimationState::WarBattle: return "WarBattle";
        case AnimationState::Build:    return "Build";
        case AnimationState::Repair:   return "Repair";
        case AnimationState::Count:   return "Count";
    }
    return "Unknown";
}

// Drives one entity's animation state. Clips are optional: requestState()
// works with no clips bound (procedural fallback) so the whole game runs
// headless; the engine binding later swaps in real FBX clips per state.
// Transition rules: Death is terminal; Stunned interrupts anything but Death;
// Attack/Cast/Channel/Brawl/Maul auto-return to Idle when their clip finishes
// (unless looping); Walk/Run/Idle/FearRun/SacrificePerformer/SacrificeVictim/
// WarBattle/Build/Repair loop freely.
class AnimationStateMachine {
public:
    AnimationStateMachine();

    void bindClip(AnimationState state, AnimationClip clip);
    bool hasClip(AnimationState state) const;
    const AnimationClip* clip(AnimationState state) const;

    // Mixamo-standard clip name for a state, e.g. Walk -> "Walking".
    // Useful when binding downloaded Mixamo packs without renaming.
    static std::string mixamoClipName(AnimationState state);

    void requestState(AnimationState s, double blendSeconds = 0.25);
    void update(double dt);

    AnimationState currentState() const { return state_; }
    double timeInState() const { return timeInState_; }
    // Name of the clip that would play now ("<procedural>" when unbound).
    std::string currentClipName() const;
    bool proceduralFallback() const { return !hasClip(state_); }

    // Pose of the current clip at timeInState(). Blend-aware: during a
    // transition this returns sampleBlendedPose().
    Pose currentPose() const;
    // Pose lerped between the previous clip's pose and the new clip's pose,
    // weighted by blendT() smoothed with smoothstep. Outside a blend this
    // equals the current clip's pose.
    Pose sampleBlendedPose() const;
    // 0 at the start of a transition, 1 when the blend has finished.
    double blendT() const { return blendActive_ ? blendT_ : 1.0; }
    bool blending() const { return blendActive_; }

private:
    AnimationState state_ = AnimationState::Idle;
    double timeInState_ = 0.0;

    // Blend state: previous clip keeps playing while the new one fades in.
    bool blendActive_ = false;
    double blendT_ = 1.0;
    double blendDuration_ = 0.25;
    double prevTime_ = 0.0;
    bool hasPrevClip_ = false;
    AnimationClip prevClip_;

    void transitionTo(AnimationState s, double blendSeconds);

    struct StateHash {
        size_t operator()(AnimationState s) const noexcept {
            return static_cast<size_t>(s);
        }
    };
    std::unordered_map<AnimationState, AnimationClip, StateHash> clips_;

    double clipDuration(AnimationState s) const;
    bool clipLoops(AnimationState s) const;
};

// Maps entity motion/action flags to an animation state. The game loop calls
// this (or requestState directly) so visuals follow gameplay.
inline AnimationState suggestAnimState(bool moving, bool running,
                                       bool attacking, bool casting,
                                       bool channeling, bool stunned,
                                       bool dead) {
    if (dead) return AnimationState::Death;
    if (stunned) return AnimationState::Stunned;
    if (attacking) return AnimationState::Attack;
    if (casting) return AnimationState::Cast;
    if (channeling) return AnimationState::Channel;
    if (moving) return running ? AnimationState::Run : AnimationState::Walk;
    return AnimationState::Idle;
}

} // namespace cultulhu
