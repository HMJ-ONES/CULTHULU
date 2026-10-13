// CULT-ULHU micro-dev regression tests: one-shot animation re-trigger.
//
// A monstrosity's MaulStruck event fires per damaging hit (Combat::dealDamage),
// and a Dhole's strike cadence can be faster than the 0.9s Maul clip. The
// state machine used to drop a re-request for the currently-active state, so
// the second hit's visual was silently swallowed while its damage still
// landed — visuals desynced from damage. Re-requesting an ACTIVE one-shot
// state now restarts it. Looping states keep the old ignore semantics so a
// per-tick re-request (AI fear-run, locomotion) can't jitter a loop.
//
// Pinned behavior:
// 1. Maul re-request mid-clip restarts: timeInState resets to ~0, blend
//    restart is active, and the maul still auto-returns to Idle after the
//    (restarted) clip finishes.
// 2. Looping states unaffected: re-requesting active Idle/Run ignores the
//    request (timeInState keeps accumulating — no jitter).
// 3. Guards unchanged: Death stays terminal; an active Stunned still refuses
//    to be interrupted by anything but Death (Stunned is not one-shot).
// 4. Brawl/Attack/Launch share the one-shot set; Launch restarts too.

#include "animation/AnimationStateMachine.h"

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

int main() {
    // 1: Maul re-request mid-clip restarts the clip.
    {
        AnimationStateMachine sm;
        sm.requestState(AnimationState::Maul, 0.0);
        sm.update(0.4);
        CHECK(sm.currentState() == AnimationState::Maul);
        CHECK(sm.timeInState() >= 0.39);
        sm.requestState(AnimationState::Maul, 0.0); // second hit mid-maul
        CHECK(sm.currentState() == AnimationState::Maul);
        CHECK(std::fabs(sm.timeInState()) < 1e-9); // restarted at t=0
        // The restarted clip still finishes and auto-returns to Idle
        // (procedural Maul fallback duration is 0.9s).
        sm.update(0.89);
        CHECK(sm.currentState() == AnimationState::Maul);
        sm.update(0.02);
        CHECK(sm.currentState() == AnimationState::Idle);
    }

    // 2: looping states ignore re-requests (no jitter on per-tick updates).
    {
        AnimationStateMachine sm;
        sm.requestState(AnimationState::Run, 0.0);
        sm.update(0.5);
        sm.requestState(AnimationState::Run, 0.0); // AI re-asserts each tick
        CHECK(sm.currentState() == AnimationState::Run);
        CHECK(sm.timeInState() >= 0.49); // kept accumulating, not restarted
        sm.requestState(AnimationState::Idle, 0.0);
        sm.update(0.3);
        sm.requestState(AnimationState::Idle, 0.0);
        CHECK(sm.currentState() == AnimationState::Idle);
        CHECK(sm.timeInState() >= 0.29);
    }

    // 3: guards unchanged — Death terminal, Stunned still interrupting.
    {
        AnimationStateMachine sm;
        sm.requestState(AnimationState::Death, 0.0);
        sm.update(0.5);
        sm.requestState(AnimationState::Maul, 0.0);
        CHECK(sm.currentState() == AnimationState::Death); // stays terminal

        AnimationStateMachine sm2;
        sm2.requestState(AnimationState::Stunned, 0.0);
        sm2.update(0.2);
        sm2.requestState(AnimationState::Stunned, 0.0);
        CHECK(sm2.currentState() == AnimationState::Stunned);
        CHECK(sm2.timeInState() >= 0.19); // not one-shot: no restart
        sm2.requestState(AnimationState::Death, 0.0);
        CHECK(sm2.currentState() == AnimationState::Death); // death overrides
    }

    // 4: the rest of the one-shot set restarts; Brawl keeps its one-shot
    // auto-return too.
    {
        AnimationStateMachine sm;
        sm.requestState(AnimationState::Launch, 0.0);
        sm.update(0.3);
        sm.requestState(AnimationState::Launch, 0.0);
        CHECK(std::fabs(sm.timeInState()) < 1e-9);

        sm.requestState(AnimationState::Brawl, 0.0);
        sm.update(0.5);
        sm.requestState(AnimationState::Brawl, 0.0); // repeat brawl re-throws
        CHECK(std::fabs(sm.timeInState()) < 1e-9);
        sm.update(0.9); // procedural Brawl fallback: 0.9s one-shot
        CHECK(sm.currentState() == AnimationState::Idle);
    }

    if (failures == 0) std::cout << "tests_micro_oneshotrestart: all green\n";
    return failures == 0 ? 0 : 1;
}
