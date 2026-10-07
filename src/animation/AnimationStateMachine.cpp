#include "animation/AnimationStateMachine.h"

namespace cultulhu {

AnimationStateMachine::AnimationStateMachine() = default;

void AnimationStateMachine::bindClip(AnimationState state, AnimationClip clip) {
    clips_[state] = std::move(clip);
}

bool AnimationStateMachine::hasClip(AnimationState state) const {
    return clips_.count(state) > 0;
}

const AnimationClip* AnimationStateMachine::clip(AnimationState state) const {
    auto it = clips_.find(state);
    return it != clips_.end() ? &it->second : nullptr;
}

std::string AnimationStateMachine::mixamoClipName(AnimationState state) {
    // Standard Mixamo pack names (https://www.mixamo.com).
    switch (state) {
        case AnimationState::Idle:    return "Idle";
        case AnimationState::Walk:    return "Walking";
        case AnimationState::Run:     return "Running";
        case AnimationState::Attack:  return "SwordAndShieldSlash";
        case AnimationState::Cast:    return "Spellcast";
        case AnimationState::Stunned: return "Stunned";
        case AnimationState::Death:   return "Death";
        case AnimationState::Channel: return "Channel";
        case AnimationState::Count:   return "";
    }
    return "";
}

void AnimationStateMachine::requestState(AnimationState s) {
    if (s == AnimationState::Count) return;
    if (state_ == AnimationState::Death) return; // terminal
    if (s == state_) return;
    if (state_ == AnimationState::Stunned && s != AnimationState::Death)
        return; // stunned interrupts; only death overrides stun
    state_ = s;
    timeInState_ = 0.0;
}

void AnimationStateMachine::update(double dt) {
    timeInState_ += dt;
    switch (state_) {
        case AnimationState::Attack:
        case AnimationState::Cast:
        case AnimationState::Channel:
            // One-shot states return to Idle when the clip finishes, unless
            // the bound clip loops.
            if (!clipLoops(state_) && timeInState_ >= clipDuration(state_)) {
                state_ = AnimationState::Idle;
                timeInState_ = 0.0;
            }
            break;
        default:
            break;
    }
}

std::string AnimationStateMachine::currentClipName() const {
    const AnimationClip* c = clip(state_);
    if (c) return c->name;
    return std::string("<procedural:") + animationStateName(state_) + ">";
}

double AnimationStateMachine::clipDuration(AnimationState s) const {
    const AnimationClip* c = clip(s);
    if (c) return c->durationSeconds;
    // Procedural fallback durations (creative liberty; see README).
    switch (s) {
        case AnimationState::Attack: return 0.8;
        case AnimationState::Cast:   return 1.2;
        case AnimationState::Channel:return 3.0;
        case AnimationState::Death:  return 2.0;
        default:                     return 1.0;
    }
}

bool AnimationStateMachine::clipLoops(AnimationState s) const {
    const AnimationClip* c = clip(s);
    if (c) return c->loop;
    // One-shot states don't loop procedurally; locomotion/idle do.
    return s == AnimationState::Idle || s == AnimationState::Walk ||
           s == AnimationState::Run || s == AnimationState::Stunned;
}

} // namespace cultulhu
