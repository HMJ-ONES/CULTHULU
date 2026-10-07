#pragma once

// Wave 7 (Worker B): melee combo chains. LMB click edges (with explicit
// game-time timestamps — no wall clock, fully testable) advance a staged
// combo: timed chains of strikes, each stage carrying the animation state
// name to play, a damage multiplier, and an optional crowd-control effect.
//
// The tracker only reports WHICH stage is active; the caller applies the
// stage: damage = base * stage.damageMult through the existing damage
// pipeline (combat::strikeMelee / calcDamage), CC through ActiveEffects
// with the stage's ccType/ccSeconds, and the anim via
// AnimationStateMachine::requestState() using stage.animState.
//
// animState names must match animationStateName() ("Attack", "Cast", ...).
// ccType names must match ccTypeName() ("Stun", "Slow", "Root", "Fear");
// empty ccType means no crowd control on that stage.

#include <string>
#include <vector>

namespace cultulhu {

struct ComboStage {
    std::string animState; // e.g. "Attack"
    float damageMult = 1.0f;
    std::string ccType;    // "" = none, else "Stun"/"Slow"/"Root"/"Fear"
    float ccSeconds = 0.0f;
};

struct ComboDef {
    std::string id;
    std::vector<ComboStage> stages;
    float chainWindowSec = 1.2f; // max gap between clicks to keep chaining
};

// Default 3-stage flurry: jab -> sweeping strike (slow) -> heavy slam (stun).
inline ComboDef defaultBasicFlurry() {
    ComboDef def;
    def.id = "basic_flurry";
    def.chainWindowSec = 1.2f;
    def.stages = {
        {"Attack", 1.00f, "",     0.0f},  // stage 1: quick jab
        {"Attack", 1.15f, "Slow", 1.0f},  // stage 2: sweeping strike, slows
        {"Attack", 1.50f, "Stun", 0.75f}, // stage 3: heavy slam, stuns
    };
    return def;
}

class ComboTracker {
public:
    explicit ComboTracker(ComboDef def = defaultBasicFlurry());

    // Feed an LMB press edge with its game-time timestamp (seconds).
    // Within chainWindowSec of the previous click the chain advances one
    // stage; after a gap the chain resets to stage 1. A click on the final
    // stage starts a fresh chain at stage 1 (combo end -> reset).
    void onLmbClick(double timestamp);

    // Explicit timeout check for drivers that don't feed a click this
    // frame: resets when timestamp is past the chain window.
    void update(double timestamp);

    // 0 when no chain is active, else 1-based stage number.
    int currentStage() const { return active_ ? stage_ + 1 : 0; }
    bool active() const { return active_; }

    const ComboDef& def() const { return def_; }
    // The stage currently playing (nullptr when inactive).
    const ComboStage* stageDef() const;

    void reset();

private:
    ComboDef def_;
    bool active_ = false;
    int stage_ = 0;          // 0-based index into def_.stages
    double lastClickAt_ = 0.0;
};

} // namespace cultulhu
