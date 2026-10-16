// CULT-ULHU micro-dev regression tests: 30s select-window nudge.
//
// The driver prints one "[select] 10s left" nudge before the character-select
// window closes (30s after match start), then the window-closed message owns
// the >= 30s boundary. The boundary logic is the pure predicate
// cultulhu::driver::selectNudgeDue (src/driver/SelectNudge.h); these pin it:
//
// 1. Silent early: 0s, 10s, 19.999s — no nudge yet.
// 2. Fires in the lead window: exactly 20s, 25s, 29.999s.
// 3. Closed boundary: exactly 30s and past it stay silent here (the
//    window-closed message owns that beat).
// 4. Exactly-once: alreadyNudged suppresses a re-fire inside the window.
//
// The driver also gates on matchActive && !playerJoined (a joined human or
// an ended match never gets the nudge); those gates live in main.cpp's
// loop and are live-verified in the driver, not pinned here.

#include "driver/SelectNudge.h"

#include <chrono>
#include <iostream>

using namespace cultulhu::driver;
using Steady = std::chrono::steady_clock;

static int failures = 0;
#define CHECK(cond)                                        \
    do {                                                   \
        if (!(cond)) {                                     \
            std::cout << "FAIL line " << __LINE__ << ": "  \
                      << #cond << "\n";                     \
            ++failures;                                    \
        }                                                  \
    } while (0)

static bool dueAt(double seconds, bool alreadyNudged = false) {
    const auto start = Steady::now();
    const auto now =
        start + std::chrono::duration_cast<Steady::duration>(
                    std::chrono::duration<double>(seconds));
    return selectNudgeDue(start, now, alreadyNudged);
}

int main() {
    // Window constants are the contract: 30s window, 10s lead.
    CHECK(kSelectWindow == std::chrono::seconds(30));
    CHECK(kSelectNudgeLead == std::chrono::seconds(10));

    // ---------- 1. Silent early ----------
    CHECK(!dueAt(0.0));
    CHECK(!dueAt(10.0));
    CHECK(!dueAt(19.999));

    // ---------- 2. Fires in the lead window ----------
    CHECK(dueAt(20.0));
    CHECK(dueAt(25.0));
    CHECK(dueAt(29.999));

    // ---------- 3. Closed boundary belongs to the close message ----------
    CHECK(!dueAt(30.0));
    CHECK(!dueAt(30.001));
    CHECK(!dueAt(120.0));

    // ---------- 4. Exactly-once ----------
    CHECK(!dueAt(25.0, true));
    CHECK(!dueAt(20.0, true));

    if (failures == 0) std::cout << "selectnudge: all boundary checks passed\n";
    return failures ? 1 : 0;
}
