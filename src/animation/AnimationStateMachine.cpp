#include "animation/AnimationStateMachine.h"

#include <cmath>

namespace cultulhu {

namespace {

// Advance a clip-local time, wrapping for looping clips.
void advanceClipTime(const AnimationClip& c, double& t, double dt) {
    t += dt;
    if (c.loop && c.durationSeconds > 0.0) {
        t = std::fmod(t, c.durationSeconds);
        if (t < 0.0) t += c.durationSeconds;
    }
}

double smoothstep(double t) {
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    return t * t * (3.0 - 2.0 * t);
}

} // namespace

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
        case AnimationState::CastWave: return "MindControlWave";
        case AnimationState::Levitate: return "LevitateHold";
        case AnimationState::Launch:  return "PsychicLaunch";
        case AnimationState::Levitated: return "Levitated";
        case AnimationState::Count:   return "";
    }
    return "";
}

void AnimationStateMachine::transitionTo(AnimationState s,
                                          double blendSeconds) {
    if (blendSeconds > 0.0 && s != state_) {
        const AnimationClip* c = clip(state_);
        hasPrevClip_ = (c != nullptr);
        if (c) prevClip_ = *c;
        prevTime_ = timeInState_;
        blendDuration_ = blendSeconds;
        blendT_ = 0.0;
        blendActive_ = true;
    } else {
        blendActive_ = false;
        blendT_ = 1.0;
    }
    state_ = s;
    timeInState_ = 0.0;
}

void AnimationStateMachine::requestState(AnimationState s, double blendSeconds) {
    if (s == AnimationState::Count) return;
    if (state_ == AnimationState::Death) return; // terminal
    if (s == state_) return;
    if (state_ == AnimationState::Stunned && s != AnimationState::Death)
        return; // stunned interrupts; only death overrides stun
    transitionTo(s, blendSeconds);
}

void AnimationStateMachine::update(double dt) {
    timeInState_ += dt;
    if (blendActive_) {
        blendT_ += dt / blendDuration_;
        if (hasPrevClip_) advanceClipTime(prevClip_, prevTime_, dt);
        if (blendT_ >= 1.0) {
            blendT_ = 1.0;
            blendActive_ = false;
        }
    }
    switch (state_) {
        case AnimationState::Attack:
        case AnimationState::Cast:
        case AnimationState::Channel:
            // One-shot states return to Idle when the clip finishes, unless
            // the bound clip loops.
            if (!clipLoops(state_) && timeInState_ >= clipDuration(state_)) {
                transitionTo(AnimationState::Idle, 0.25);
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

Pose AnimationStateMachine::sampleBlendedPose() const {
    const AnimationClip* cur = clip(state_);
    const Pose poseB = cur ? cur->sampleAt(timeInState_) : Pose{};
    if (!blendActive_ || !hasPrevClip_) return poseB;
    const Pose poseA = prevClip_.sampleAt(prevTime_);
    return blendPoses(poseA, poseB, smoothstep(blendT_));
}

Pose AnimationStateMachine::currentPose() const {
    if (blendActive_) return sampleBlendedPose();
    const AnimationClip* cur = clip(state_);
    return cur ? cur->sampleAt(timeInState_) : Pose{};
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
