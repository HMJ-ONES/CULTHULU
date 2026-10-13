// CULT-ULHU micro-dev regression tests: one-shot restart self-blend.
//
// The 16:19 micro-dev made an active one-shot re-requested mid-clip restart
// (a Dhole's second Maul at 0.7s into the 0.9s clip) so every hit lands its
// visual. But transitionTo skipped blend setup when s == state_, so the
// restart snapped from the mid-clip pose straight back to frame 0 — a
// visible pop. The restart now self-blends: the mid-clip pose becomes the
// blend source and the restarted clip fades in over blendSeconds.
//
// Pinned behavior:
// 1. Restart with blendSeconds > 0 activates a blend (blending() true,
//    blendT ~0), and the sampled pose at the restart instant still reads
//    the MID-CLIP pose — not frame 0 (the pop is gone).
// 2. Mid-blend the pose is the smoothstep-weighted mix of the advancing
//    mid-clip pose and the restarted clip; when the blend finishes the
//    pose reads the restarted clip alone.
// 3. blendSeconds == 0 keeps the old hard snap (blending() false, frame 0).
// 4. A blended restart still auto-returns to Idle when the restarted clip
//    finishes.

#include "animation/AnimationClip.h"
#include "animation/AnimationStateMachine.h"
#include "animation/BoneTrack.h"

#include <cmath>
#include <iostream>

using namespace cultulhu;

static int failures = 0;
#define CHECK(cond)                                        \
    do {                                                   \
        if (!(cond)) {                                     \
            std::cout << "FAIL line " << __LINE__ << ": "  \
                      << #cond << "\n";                    \
            ++failures;                                    \
        }                                                  \
    } while (0)
#define CHECK_CLOSE(a, b, eps)                                            \
    do {                                                                  \
        if (std::fabs((a) - (b)) > (eps)) {                               \
            std::cout << "FAIL line " << __LINE__ << ": " #a " (" << (a)   \
                      << ") != " #b " (" << (b) << ")\n";                  \
            ++failures;                                                   \
        }                                                                 \
    } while (0)

static Keyframe kf(double t, float px, float py, float pz, float rx, float ry,
                   float rz) {
    Keyframe k;
    k.time = t;
    k.pos = Vec3(px, py, pz);
    k.rotEuler = Vec3(rx, ry, rz);
    return k;
}

// Two-key one-shot clip on one bone: rotX 0 -> 90 over 0.9s (Maul-like).
static AnimationClip maulClip() {
    AnimationClip c;
    c.name = "MaulTest";
    c.durationSeconds = 0.9;
    c.loop = false;
    BoneTrack tr;
    tr.bone = "hips";
    tr.keys.push_back(kf(0.0, 0, 0, 0, 0, 0, 0));
    tr.keys.push_back(kf(0.9, 0, 10, 0, 90, 0, 0));
    c.tracks["hips"] = tr;
    return c;
}

int main() {
    // 1+2: blended restart — no pop, smoothstep mix mid-blend.
    {
        AnimationStateMachine sm;
        sm.bindClip(AnimationState::Maul, maulClip());
        sm.requestState(AnimationState::Maul, 0.25);
        sm.update(0.45); // mid-clip: rotX = 45
        CHECK_CLOSE(sm.currentPose()["hips"].rotEuler.x, 45.0, 1e-4);

        sm.requestState(AnimationState::Maul, 0.25); // second hit, mid-maul
        CHECK(sm.currentState() == AnimationState::Maul);
        CHECK(std::fabs(sm.timeInState()) < 1e-9); // restarted at t=0
        CHECK(sm.blending());                      // self-blend is active
        CHECK_CLOSE(sm.blendT(), 0.0, 1e-9);
        // At the restart instant the pose still reads the mid-clip pose —
        // smoothstep(0) = 0 picks the blend source. Before the fix this
        // read frame 0 (the pop).
        CHECK_CLOSE(sm.currentPose()["hips"].rotEuler.x, 45.0, 1e-4);
        CHECK_CLOSE(sm.currentPose()["hips"].pos.y, 5.0, 1e-4);

        sm.update(0.125); // blendT = 0.5 -> smoothstep = 0.5
        // prev pose advanced to t=0.575 (rotX 57.5), cur at t=0.125
        // (rotX 12.5); even mix = 35.
        CHECK(sm.blending());
        CHECK_CLOSE(sm.currentPose()["hips"].rotEuler.x, 35.0, 1.0);

        sm.update(0.125); // blendT = 1 -> blend done
        CHECK(!sm.blending());
        CHECK_CLOSE(sm.blendT(), 1.0, 1e-9);
        CHECK_CLOSE(sm.currentPose()["hips"].rotEuler.x, 25.0, 1.0);
    }

    // 3: blendSeconds == 0 keeps the old hard snap.
    {
        AnimationStateMachine sm;
        sm.bindClip(AnimationState::Maul, maulClip());
        sm.requestState(AnimationState::Maul, 0.0);
        sm.update(0.45);
        sm.requestState(AnimationState::Maul, 0.0);
        CHECK(!sm.blending());
        CHECK_CLOSE(sm.currentPose()["hips"].rotEuler.x, 0.0, 1e-4);
    }

    // 4: blended restart still auto-returns to Idle at clip end.
    {
        AnimationStateMachine sm;
        sm.bindClip(AnimationState::Maul, maulClip());
        sm.requestState(AnimationState::Maul, 0.25);
        sm.update(0.45);
        sm.requestState(AnimationState::Maul, 0.25);
        sm.update(0.9); // restarted 0.9s clip finishes
        CHECK(sm.currentState() == AnimationState::Idle);
    }

    if (failures == 0) std::cout << "tests_micro_oneshotblend: all green\n";
    return failures == 0 ? 0 : 1;
}
