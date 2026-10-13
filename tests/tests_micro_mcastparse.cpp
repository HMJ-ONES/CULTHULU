// CULT-ULHU micro-dev regression tests: `mcast` tail-arg parsing.
//
// `mcast rmb down` / `mcast rmb launch` (and `mcast q 90`) are parsed by
// Match::parseCastArgs. The original driver code read the next token as a
// float FIRST, so "down"/"launch" set failbit and the verb never reached
// `aim` — Cthulhu's rock could never be ripped or hurled from the driver.
// The words-before-numbers fix was extracted into a testable static; these
// pin the contract:
//
// 1. "down" / "launch" set the aim and keep the default yaw.
// 2. A bare number sets the yaw (degrees -> radians) with Normal aim.
// 3. Number + verb combines both.
// 4. No args / unknown token keeps the default yaw and Normal aim.
// 5. A verb first stops reading — "down 90" aims Down with default yaw.

#include "modes/Match.h"

#include <cmath>
#include <iostream>
#include <sstream>
#include <string>

using namespace cultulhu;

static int failures = 0;
#define CHECK(cond)                                        \
    do {                                                   \
        if (!(cond)) {                                     \
            std::cout << "FAIL line " << __LINE__ << ": "  \
                      << #cond << "\n";                     \
            ++failures;                                    \
        }                                                  \
    } while (0)

static std::pair<float, Match::RmbAim> parse(const std::string& tail,
                                             float defYaw = 1.234f) {
    std::istringstream in(tail);
    return Match::parseCastArgs(in, defYaw);
}

static bool yawEq(float a, float b) { return std::fabs(a - b) < 1e-5f; }

int main() {
    // 1: verbs set the aim, yaw untouched.
    {
        auto [yaw, aim] = parse("down");
        CHECK(aim == Match::RmbAim::Down);
        CHECK(yawEq(yaw, 1.234f));
    }
    {
        auto [yaw, aim] = parse("launch");
        CHECK(aim == Match::RmbAim::Launch);
        CHECK(yawEq(yaw, 1.234f));
    }
    // 2: bare number -> radians, Normal aim.
    {
        auto [yaw, aim] = parse("90");
        CHECK(aim == Match::RmbAim::Normal);
        CHECK(yawEq(yaw, 3.14159265f / 2.0f));
    }
    {
        auto [yaw, aim] = parse("-45");
        CHECK(aim == Match::RmbAim::Normal);
        CHECK(yawEq(yaw, -3.14159265f / 4.0f));
    }
    // 3: number + verb combines.
    {
        auto [yaw, aim] = parse("90 launch");
        CHECK(aim == Match::RmbAim::Launch);
        CHECK(yawEq(yaw, 3.14159265f / 2.0f));
    }
    {
        auto [yaw, aim] = parse("180 down");
        CHECK(aim == Match::RmbAim::Down);
        CHECK(yawEq(yaw, 3.14159265f));
    }
    // 4: empty tail and unknown words keep the default facing.
    {
        auto [yaw, aim] = parse("");
        CHECK(aim == Match::RmbAim::Normal);
        CHECK(yawEq(yaw, 1.234f));
    }
    {
        auto [yaw, aim] = parse("bogus");
        CHECK(aim == Match::RmbAim::Normal);
        CHECK(yawEq(yaw, 1.234f));
    }
    {
        // Unknown first word still lets a trailing verb through, like the
        // driver's original behavior.
        auto [yaw, aim] = parse("bogus launch");
        CHECK(aim == Match::RmbAim::Launch);
        CHECK(yawEq(yaw, 1.234f));
    }
    // 5: verb first stops reading — trailing numbers are left alone.
    {
        auto [yaw, aim] = parse("down 90");
        CHECK(aim == Match::RmbAim::Down);
        CHECK(yawEq(yaw, 1.234f));
    }

    if (failures == 0) std::cout << "tests_micro_mcastparse: all green\n";
    return failures;
}
