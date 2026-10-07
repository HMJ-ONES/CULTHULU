#include "input/InputManager.h"

#include <cmath>

namespace cultulhu {

// ---------------- StaminaGauge ----------------

bool StaminaGauge::update(bool sprintWanted, float dt) {
    if (sprintWanted && !exhausted) {
        value -= InputManager::SPRINT_DRAIN_PER_SEC * dt;
        if (value <= 0.0f) {
            value = 0.0f;
            exhausted = true;
        }
    } else {
        value += InputManager::STAMINA_REGEN_PER_SEC * dt;
        if (value > 100.0f) value = 100.0f;
        if (exhausted && value >= InputManager::EXHAUST_RECOVER_AT)
            exhausted = false;
    }
    return sprintWanted && !exhausted;
}

// ---------------- AbilitySlot ----------------

bool AbilitySlot::trigger() {
    if (!ready()) return false;
    remaining = cooldown;
    return true;
}

void AbilitySlot::update(float dt) {
    if (remaining > 0.0f) {
        remaining -= dt;
        if (remaining < 0.0f) remaining = 0.0f;
    }
}

// ---------------- InputManager ----------------

InputManager::InputManager() {
    abilities_[0].abilityId = "ability_q";
    abilities_[1].abilityId = "ability_f";
    abilities_[2].abilityId = "ability_r";
}

void InputManager::setGrounded(bool grounded) {
    if (grounded_ && !grounded) leftGroundAt_ = gameTime_;
    grounded_ = grounded;
}

void InputManager::update(const InputState& state, float dt) {
    gameTime_ += dt;

    const InputState& prev = firstUpdate_ ? InputState{} : prev_;

    // ---- Edges (first frame: InputState{} so pressed = held) ----
    const bool spacePressed = pressedEdge(state.space, prev.space);
    const bool qPressed = pressedEdge(state.q, prev.q);
    const bool fPressed = pressedEdge(state.f, prev.f);
    const bool rPressed = pressedEdge(state.r, prev.r);
    const bool tabPressed = pressedEdge(state.tab, prev.tab);
    const bool lmbPressed = pressedEdge(state.lmb, prev.lmb);
    const bool rmbPressed = pressedEdge(state.rmb, prev.rmb);

    // ---- Movement vector (WASD on XZ, normalized) ----
    float mx = (state.d.held ? 1.0f : 0.0f) - (state.a.held ? 1.0f : 0.0f);
    float mz = (state.w.held ? 1.0f : 0.0f) - (state.s.held ? 1.0f : 0.0f);
    moveVec_ = Vec3(mx, 0.0f, mz);
    const float len = moveVec_.length();
    if (len > 1.0f) moveVec_ = moveVec_ * (1.0f / len);

    // ---- Jump: buffer the press, resolve in jumpPressed() ----
    if (spacePressed) jumpBufferedAt_ = gameTime_;

    // ---- Sprint / stamina ----
    sprintWanted_ = state.shift.held;
    stamina_.update(sprintWanted_, dt);

    // ---- Ability slots Q/F/R ----
    abilityFired_ = {false, false, false};
    const bool firedIn[ABILITY_COUNT] = {qPressed, fPressed, rPressed};
    for (size_t i = 0; i < ABILITY_COUNT; ++i) {
        abilities_[i].update(dt);
        if (firedIn[i]) abilityFired_[i] = abilities_[i].trigger();
    }

    // ---- E interact: nearest candidate within radius ----
    // (The candidate list is engine-supplied at press time; here we just
    // clear the latch. The driver calls findInteractable + sets target.)
    if (pressedEdge(state.e, prev.e)) lastInteractTarget_ = 0;

    // ---- Tab: toggle the stats overlay ----
    if (tabPressed) statsOverlayVisible_ = !statsOverlayVisible_;

    // ---- Alt+LMB: command-menu open request ----
    menuRequested_ = state.alt.held && lmbPressed;
    if (menuRequested_)
        menuRequestPos_ = Vec3(state.mouseX, state.mouseY, 0.0f);

    // ---- Mouse state + click timestamps (game time, no wall clock) ----
    lmbHeld_ = state.lmb.held;
    rmbHeld_ = state.rmb.held;
    if (lmbPressed) lmbLastClickTime_ = gameTime_;
    if (rmbPressed) rmbLastClickTime_ = gameTime_;

    prev_ = state;
    firstUpdate_ = false;
}

Vec3 InputManager::moveVector() const { return moveVec_; }

bool InputManager::jumpPressed() {
    const bool buffered = (gameTime_ - jumpBufferedAt_) <= JUMP_BUFFER_SEC;
    if (!buffered) return false;
    const bool canJump = grounded_ ||
        ((gameTime_ - leftGroundAt_) <= COYOTE_TIME_SEC);
    if (canJump) {
        jumpBufferedAt_ = -1e9; // consume
        return true;
    }
    return false;
}

bool InputManager::sprintHeld() const {
    return sprintWanted_ && !stamina_.exhausted;
}

bool InputManager::abilityFired(size_t i) const {
    return i < ABILITY_COUNT ? abilityFired_[i] : false;
}

const Interactable* InputManager::findInteractable(
        Vec3 pos, const std::vector<Interactable>& candidates) const {
    const Interactable* best = nullptr;
    float bestDist = INTERACT_RADIUS;
    for (const auto& c : candidates) {
        const float d = pos.distance(c.pos);
        if (d <= bestDist) {
            bestDist = d;
            best = &c;
        }
    }
    return best;
}

void InputManager::bindAbility(size_t i, const std::string& abilityId,
                               float cooldown) {
    if (i >= ABILITY_COUNT) return;
    abilities_[i].abilityId = abilityId;
    abilities_[i].cooldown = cooldown;
    abilities_[i].remaining = 0.0f;
}

} // namespace cultulhu
