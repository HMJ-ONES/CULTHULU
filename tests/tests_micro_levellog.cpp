// CULT-ULHU micro-dev (2026-10-13): level-gate log regression tests.
// MatchLogger records the exported GitHub match log — Henrique reads it
// as "the story of the match" — but the tiered-invocation gates at
// 10/20/30 (wave 34, 2 PM build) never landed there, even though they are
// the match's progression beats. Only gate crossings are logged (never
// per-level noise), and multi-level jumps that skip over a gate are
// still caught, because PlayerLeveledUp only carries the final level.
// 1. Crossing 10 via production grantXp logs exactly one LEVEL line with
//    the tier-1 text and the player/team tag.
// 2. Non-gate level-ups log nothing (the story stays readable).
// 3. A multi-level jump landing past a gate (9 -> 11) still logs gate 10.
// 4. Gates 20 and 30 each fire exactly once, even across separate jumps.

#include "modes/Match.h"
#include "modes/MatchLogger.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "core/Events.h"

#include <iostream>
#include <string>
#include <vector>

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

static int countKind(const MatchLogger& log, const std::string& kind) {
    int n = 0;
    for (const auto& e : log.events())
        if (e.kind == kind) ++n;
    return n;
}

int main() {
    // ---------- 1: gate 10 logs one LEVEL line ----------
    {
        EventBus bus; GameClock clock; RNG rng(1313);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t pid = m.addPlayer("Hero", 0);
        MatchLogger log;
        log.attach(m);
        m.grantXp(pid, 450); // 1 + 450/50 = 10
        CHECK(m.findPlayer(pid)->level == 10);
        CHECK(countKind(log, "LEVEL") == 1);
        CHECK(log.events().back().text.find("hits level 10") !=
              std::string::npos);
        CHECK(log.events().back().text.find("invocation tier 1") !=
              std::string::npos);
        CHECK(log.events().back().text.find("Hero") != std::string::npos);
    }
    // ---------- 2: non-gate levels stay silent ----------
    {
        EventBus bus; GameClock clock; RNG rng(1314);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t pid = m.addPlayer("Quiet", 1);
        MatchLogger log;
        log.attach(m);
        m.grantXp(pid, 60);  // level 2
        m.grantXp(pid, 300); // level 8
        CHECK(m.findPlayer(pid)->level == 8);
        CHECK(countKind(log, "LEVEL") == 0);
    }
    // ---------- 3: jump over the gate still logs it ----------
    {
        EventBus bus; GameClock clock; RNG rng(1315);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t pid = m.addPlayer("Jumper", 0);
        MatchLogger log;
        log.attach(m);
        m.grantXp(pid, 400); // level 9 — no gate crossed
        CHECK(countKind(log, "LEVEL") == 0);
        m.grantXp(pid, 100); // level 11 — gate 10 crossed mid-jump
        CHECK(m.findPlayer(pid)->level == 11);
        CHECK(countKind(log, "LEVEL") == 1);
        CHECK(log.events().back().text.find("hits level 10") !=
              std::string::npos);
    }
    // ---------- 4: gates 20 and 30 each fire once ----------
    {
        EventBus bus; GameClock clock; RNG rng(1316);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t pid = m.addPlayer("Climber", 0);
        MatchLogger log;
        log.attach(m);
        m.grantXp(pid, 450); // 10
        m.grantXp(pid, 500); // 20
        CHECK(m.findPlayer(pid)->level == 20);
        CHECK(countKind(log, "LEVEL") == 2);
        CHECK(log.events().back().text.find("hits level 20") !=
              std::string::npos);
        CHECK(log.events().back().text.find("invocation tier 2") !=
              std::string::npos);
        m.grantXp(pid, 500); // 30
        CHECK(m.findPlayer(pid)->level == 30);
        CHECK(countKind(log, "LEVEL") == 3);
        CHECK(log.events().back().text.find("hits level 30") !=
              std::string::npos);
        CHECK(log.events().back().text.find("invocation tier 3") !=
              std::string::npos);
        // Repeat attach must not accumulate handlers or double-log.
        log.attach(m);
        m.grantXp(pid, 60); // still 30, no new level
        CHECK(countKind(log, "LEVEL") == 0);
    }
    if (failures == 0) std::cout << "micro_levellog: all checks passed\n";
    else std::cout << "micro_levellog: " << failures << " checks FAILED\n";
    return failures == 0 ? 0 : 1;
}
