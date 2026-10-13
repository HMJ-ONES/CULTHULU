// CULT-ULHU micro-dev regression tests: the local human's row in
// `match status` is marked " (YOU)".
//
// A 10-row bot roster gives the joined human no way to spot their own
// fighter at a glance — the row just says the name. `match status` now
// takes the human's player id and marks exactly their row. These pin:
//
// 1. With no human (id 0) there is no " (YOU)" marker anywhere.
// 2. After joinAsHuman, printStatus(os, humanId) marks exactly one row —
//    the human's — with " (YOU)".
// 3. An unknown human id marks nothing (no accidental hits on other rows).

#include "modes/Match.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"

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

static std::string statusOf(const Match& m, uint64_t humanId = 0) {
    std::ostringstream oss;
    m.printStatus(oss, humanId);
    return oss.str();
}

static size_t countOccurrences(const std::string& s, const std::string& sub) {
    size_t n = 0, pos = 0;
    while ((pos = s.find(sub, pos)) != std::string::npos) {
        ++n;
        pos += sub.size();
    }
    return n;
}

int main() {
    EventBus bus; GameClock clock; RNG rng(9);
    Match m(bus, clock, rng);
    CHECK(m.start("capture"));
    uint64_t a = m.addBot(0);
    uint64_t b = m.addBot(1);
    CHECK(a != 0 && b != 0 && a != b);

    // 1. No human: the old one-arg behavior must not print the marker.
    {
        std::ostringstream oss;
        m.printStatus(oss);
        CHECK(countOccurrences(oss.str(), "(YOU)") == 0);
        CHECK(countOccurrences(statusOf(m, 0), "(YOU)") == 0);
    }

    // 2. Join a human: exactly one row carries the marker.
    uint64_t h = m.joinAsHuman(0);
    CHECK(h != 0);
    {
        std::string s = statusOf(m, h);
        CHECK(countOccurrences(s, "(YOU)") == 1);
        // The marker sits on the human's own row (name "You").
        std::string::size_type marker = s.find("(YOU)");
        CHECK(marker != std::string::npos);
        CHECK(s.rfind("    You", marker) != std::string::npos);
    }

    // 3. Unknown id: nothing is marked.
    CHECK(countOccurrences(statusOf(m, h + 999999), "(YOU)") == 0);

    if (failures == 0) std::cout << "tests_micro_you: ALL OK\n";
    return failures == 0 ? 0 : 1;
}
