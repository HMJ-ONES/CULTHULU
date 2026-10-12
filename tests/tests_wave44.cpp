// CULT-ULHU micro-dev regression tests: match fighters are roster-only.
//
// On Oct 11 Henrique caught match bots fighting as civilian/cultist NPC
// packages ("change that immediately, these are NPCs") and ordered a
// codebase-wide sweep, which turned up six issues of the same class. The
// core rule (wave 42: assignKit in MatchKits.cpp, isNpcPackage in
// RosterUtils.h): the 6 NPC packages (civilians, cultists, echo_of_the_void)
// are minions and ambient life — never match fighters, never wearable
// player bodies. The rule shipped with zero coverage, and recent experience
// says fresh untested paths are where the bugs live. These pin it:
//
// 1. botfill (the real 5v5 entry path) deals only playable kits in
//    Domination — every kitId non-empty and never an NPC package.
// 2. Same for DOTGOO.
// 3. Round-robin deals distinct kits across a full 10-player match — no
//    duplicate kits, no silently dropped assignments.
// 4. The exclusion boundary itself is stable: the 6 known NPC ids stay
//    excluded while known roster fighters stay playable.

#include "modes/Match.h"
#include "modes/RosterUtils.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"

#include <iostream>
#include <set>
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

// Full 5v5 via botfill: every fighter gets a non-empty, non-NPC kit.
static void fill_checks(const char* mode, uint64_t seed) {
    EventBus bus; GameClock clock; RNG rng(seed);
    Match m(bus, clock, rng);
    CHECK(m.start(mode));
    m.botfill();
    CHECK(m.playerCount() == 10);
    for (size_t i = 0; i < m.playerCount(); ++i) {
        const std::string& kid = m.player(i).kitId;
        CHECK(!kid.empty());
        CHECK(!isNpcPackage(kid));
    }
}

// Round-robin deals distinct kits — no duplicates, nothing dropped.
static void distinct_checks(const char* mode, uint64_t seed) {
    EventBus bus; GameClock clock; RNG rng(seed);
    Match m(bus, clock, rng);
    CHECK(m.start(mode));
    m.botfill();
    std::set<std::string> seen;
    for (size_t i = 0; i < m.playerCount(); ++i)
        seen.insert(m.player(i).kitId);
    CHECK(seen.size() == m.playerCount());
}

// The exclusion boundary: the 6 NPC ids stay excluded; roster fighters
// stay playable.
static void boundary_checks() {
    for (const auto& n : npcPackageIds()) CHECK(isNpcPackage(n));
    CHECK(isNpcPackage("civilian_guard"));
    CHECK(isNpcPackage("echo_of_the_void"));
    CHECK(!isNpcPackage("cthulhu_avatar"));
    CHECK(!isNpcPackage("yog_sothoth"));
    CHECK(!isNpcPackage("hypnos"));
    CHECK(!isNpcPackage("sghllor"));
}

int main() {
    for (uint64_t s = 440; s < 445; ++s) {
        fill_checks("capture", s);
        fill_checks("moba", s + 100);
        distinct_checks("capture", s + 200);
        distinct_checks("moba", s + 300);
    }
    boundary_checks();
    if (failures == 0) std::cout << "wave44: all checks passed\n";
    else std::cout << "wave44: " << failures << " checks FAILED\n";
    return failures == 0 ? 0 : 1;
}
