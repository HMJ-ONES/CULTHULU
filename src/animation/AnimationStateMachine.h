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
        case AnimationState::Count:   return "Count";
    }
    return "Unknown";
}

// Drives one entity's animation state. Clips are optional: requestState()
// works with no clips bound (procedural fallback) so the whole game runs
// headless; the engine binding later swaps in real FBX clips per state.
// Transition rules: Death is terminal; Stunned interrupts anything but Death;
// Attack/Cast/Channel auto-return to Idle when their clip finishes (unless
// looping); Walk/Run/Idle loop freely.
class AnimationStateMachine {
public:
    AnimationStateMachine();

    void bindClip(AnimationState state, AnimationClip clip);
    bool hasClip(AnimationState state) const;
    const AnimationClip* clip(AnimationState state) const;

    // Mixamo-standard clip name for a state, e.g. Walk -> "Walking".
    // Useful when binding downloaded Mixamo packs without renaming.
    static std::string mixamoClipName(AnimationState state);

    void requestState(AnimationState s);
    void update(double dt);

    AnimationState currentState() const { return state_; }
    double timeInState() const { return timeInState_; }
    // Name of the clip that would play now ("<procedural>" when unbound).
    std::string currentClipName() const;
    bool proceduralFallback() const { return !hasClip(state_); }

private:
    AnimationState state_ = AnimationState::Idle;
    double timeInState_ = 0.0;

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
