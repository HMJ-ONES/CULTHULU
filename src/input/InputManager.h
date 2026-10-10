#pragma once

// Wave 7 (Worker B): the owner's full input binding map as DATA + logic.
// Consumes abstract input (no OS calls): the engine fills an InputState
// each frame from the real keyboard/mouse and calls update(). All tuning
// is named constants below.
//
// Bindings (documented for the engine binding + README):
//   WASD ......... move (XZ plane, normalized)
//   Space ........ jump (coyote time + jump buffer; grounded is engine-filled)
//   Shift ........ sprint (drains the stamina gauge)
//   Q / F / R .... ability slots 0/1/2 (cooldown-gated)
//   E ............ interact (nearest Interactable within radius)
//   Tab .......... toggle stats overlay
//   Alt+LMB ...... open the radial command menu (see ui/CommandMenu)
//   LMB .......... melee attack / combo chain input
//   RMB .......... heavy / ranged attack (per character archetype)

#include "core/Vec3.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace cultulhu {

// Per-key edge state for one frame: held (down this frame), pressed (went
// down this frame), released (went up this frame).
struct KeyState {
    bool held = false;
    bool pressed = false;
    bool released = false;
};

// One frame of abstract input from the engine. mouseX/mouseY are the cursor
// position in world space (engine-projected) used for Alt+LMB menu placement.
struct InputState {
    KeyState w, a, s, d;
    KeyState space;
    KeyState shift;
    KeyState q, e, f, r;
    KeyState tab;
    KeyState alt;
    KeyState lmb;
    KeyState rmb;
    float mouseX = 0.0f;
    float mouseY = 0.0f;
};

// 0..100 stamina pool. Sprinting drains; exhaustion blocks sprint until the
// gauge regenerates above EXHAUST_RECOVER.
struct StaminaGauge {
    float value = 100.0f;
    bool exhausted = false;

    // Sprinting consumes stamina; idling regenerates. Returns true while
    // the sprint is actually allowed (blocked while exhausted).
    bool update(bool sprintWanted, float dt);
};

// One ability slot bound to Q/F/R.
struct AbilitySlot {
    std::string abilityId; // e.g. "tentacle_slam"; engine-defined
    float cooldown = 0.0f; // full cooldown, seconds
    float remaining = 0.0f; // time left until ready, seconds

    bool ready() const { return remaining <= 0.0f; }
    // Start the cooldown; returns false if not ready.
    bool trigger();
    void update(float dt);
};

// Something the player can press E on.
struct Interactable {
    enum class Kind { Altar, Captive, Relic, Door };

    uint64_t entityId = 0;
    Kind kind = Kind::Door;
    std::string prompt; // e.g. "E: perform ritual"
    Vec3 pos;
};

class InputManager {
public:
    // ---- Tuning (engine can still clamp/override at a higher level) ----
    static constexpr float SPRINT_DRAIN_PER_SEC = 18.0f; // stamina/sec
    static constexpr float STAMINA_REGEN_PER_SEC = 12.0f; // stamina/sec
    static constexpr float EXHAUST_RECOVER_AT = 30.0f;    // regen threshold
    static constexpr float COYOTE_TIME_SEC = 0.12f;      // jump forgiveness
    static constexpr float JUMP_BUFFER_SEC = 0.15f;      // early-press buffer
    static constexpr float INTERACT_RADIUS = 3.0f;       // world units
    static constexpr size_t ABILITY_COUNT = 3;           // Q / F / R

    InputManager();

    // Advance one frame: edges, stamina, cooldowns, jump buffering,
    // overlay toggle, Alt+LMB detection, click timestamps.
    void update(const InputState& state, float dt);

    // Engine-filled: is the player character grounded this frame?
    // Drives coyote time; default true so headless tests can jump.
    void setGrounded(bool grounded);

    // WASD movement vector on the XZ plane (x = right, z = forward),
    // normalized (diagonals don't go faster).
    Vec3 moveVector() const;

    // True this frame when a jump should fire: buffered Space press within
    // JUMP_BUFFER_SEC while grounded OR within COYOTE_TIME_SEC of leaving
    // the ground. Consumes the buffer.
    bool jumpPressed();

    // True while the player holds Shift and has stamina to sprint.
    bool sprintHeld() const;

    StaminaGauge& stamina() { return stamina_; }
    const StaminaGauge& stamina() const { return stamina_; }

    std::array<AbilitySlot, ABILITY_COUNT>& abilities() { return abilities_; }
    // True this frame when ability slot i (0=Q, 1=F, 2=R) fired.
    bool abilityFired(size_t i) const;

    // Nearest Interactable within INTERACT_RADIUS of pos, or nullptr.
    const Interactable* findInteractable(Vec3 pos,
        const std::vector<Interactable>& candidates) const;
    // Target chosen by the last E press (entityId 0 = none). The engine
    // calls findInteractable() and records the choice via setInteractTarget().
    uint64_t lastInteractTarget() const { return lastInteractTarget_; }
    void setInteractTarget(uint64_t entityId) { lastInteractTarget_ = entityId; }

    bool statsOverlayVisible() const { return statsOverlayVisible_; }

    // True this frame when Alt is held and LMB was pressed: the engine
    // should open the radial command menu at menuRequestPos().
    bool altLeftClick() const { return menuRequested_; }
    Vec3 menuRequestPos() const { return menuRequestPos_; }

    bool lmbHeld() const { return lmbHeld_; }
    bool rmbHeld() const { return rmbHeld_; }
    // Game-time (seconds, accumulated in update) of the last LMB/RMB press
    // edge; -1 when never pressed. Feeds the combo system.
    double lmbLastClickTime() const { return lmbLastClickTime_; }
    double rmbLastClickTime() const { return rmbLastClickTime_; }

    void bindAbility(size_t i, const std::string& abilityId, float cooldown);

private:
    InputState prev_;          // previous frame for edge bookkeeping
    bool firstUpdate_ = true;

    double gameTime_ = 0.0;    // seconds accumulated via update()
    bool grounded_ = true;
    double leftGroundAt_ = -1e9; // gameTime_ when we last left the ground
    double jumpBufferedAt_ = -1e9; // gameTime_ of last unconsumed Space press

    StaminaGauge stamina_;
    std::array<AbilitySlot, ABILITY_COUNT> abilities_;
    std::array<bool, ABILITY_COUNT> abilityFired_{};

    uint64_t lastInteractTarget_ = 0;

    bool statsOverlayVisible_ = false;
    bool menuRequested_ = false;
    Vec3 menuRequestPos_;

    bool lmbHeld_ = false;
    bool rmbHeld_ = false;
    double lmbLastClickTime_ = -1.0;
    double rmbLastClickTime_ = -1.0;

    Vec3 moveVec_;
    bool sprintWanted_ = false;

    static bool pressedEdge(const KeyState& now, const KeyState& prev) {
        return now.held && !prev.held;
    }
};

} // namespace cultulhu
