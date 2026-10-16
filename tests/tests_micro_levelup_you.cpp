// CULT-ULHU micro-dev regression tests: second-person level-ups.
//
// The driver's PlayerLeveledUp subscriber reads the joined human's own
// level-ups in second person ("*** You reached level 7 ***") and keeps
// the third-person "[level] <name> reached level N" read for bots (the
// tier-gate 10/20/30 branch already reads in second person). That branch
// only works for non-gate levels if the event carries the leveling
// player's id as sourceId with the new level as amount and the player
// name as tag. These pin (production grantXp path):
//
// 1. grantXp to a non-gate level (7) publishes one PlayerLeveledUp with
//    sourceId == the player's id, amount == 7, tag == the player's name.
// 2. An XP jump landing on a non-gate level (1 -> 4) publishes exactly
//    one event with amount == 4 and sourceId == the player's id.

#include "modes/Match.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"

#include <cstdint>
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

struct LevelHit {
    uint64_t sourceId;
    int level;
    std::string tag;
};

static LevelHit lastOf(const std::vector<LevelHit>& v) { return v.back(); }

int main() {
    // ---------- 1: non-gate level 7 carries the player id ----------
    {
        EventBus bus; GameClock clock; RNG rng(1101);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        uint64_t a = m.addBot(0);
        std::vector<LevelHit> hits;
        bus.subscribe(EventType::PlayerLeveledUp, [&](const GameEvent& e) {
            hits.push_back({e.sourceId, static_cast<int>(e.amount),
                            e.tag});
        });
        // Level = 1 + xp/50: 300 xp lands exactly on 7.
        CHECK(m.grantXp(a, 300));
        const Match::Player* p = m.findPlayer(a);
        CHECK(p && p->level == 7);
        CHECK(hits.size() == 1);
        CHECK(lastOf(hits).sourceId == a); // driver's "mine" gate needs this
        CHECK(lastOf(hits).level == 7);
        CHECK(lastOf(hits).tag == p->name);
    }
    // ---------- 2: XP jump 1 -> 4: one event, player id, amount 4 ----------
    {
        EventBus bus; GameClock clock; RNG rng(1102);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t a = m.addBot(1);
        std::vector<LevelHit> hits;
        bus.subscribe(EventType::PlayerLeveledUp, [&](const GameEvent& e) {
            hits.push_back({e.sourceId, static_cast<int>(e.amount),
                            e.tag});
        });
        // 150 xp: 1 + 150/50 = 4 — one event for the whole jump.
        CHECK(m.grantXp(a, 150));
        const Match::Player* p = m.findPlayer(a);
        CHECK(p && p->level == 4);
        CHECK(hits.size() == 1);
        CHECK(lastOf(hits).sourceId == a);
        CHECK(lastOf(hits).level == 4);
    }

    if (failures == 0) std::cout << "ALL CHECKS PASSED\n";
    return failures == 0 ? 0 : 1;
}
