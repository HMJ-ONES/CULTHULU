// CULT-ULHU micro-dev regression tests: invocation-tier gate callout.
//
// Crossing an invocation tier gate (level 10/20/30) is the joined human's
// loudest progression moment — the whole tiered-invocation endgame turns
// on it. The driver's PlayerLeveledUp subscriber reads the crossing in
// second person ("*** You reached level 10 — INVOCATION TIER 1
// unlocked! ***") when the event's sourceId matches the joined human, and
// keeps the third-person "[level] <name> reached level N" read for bots.
// That branch only works if the event carries the crossing player's id
// as sourceId with the new level as amount and the player name as tag.
// These pin (production grantXp path):
//
// 1. grantXp to exactly level 10 publishes one PlayerLeveledUp with
//    sourceId == the player's id, amount == 10, tag == the player's name.
// 2. An XP jump landing past a gate (1 -> 15) publishes exactly one
//    event with amount == 15 (one event per grantXp call, the addXp
//    contract — the subscriber reads the final level, so gate detection
//    must use amount, not old-vs-new).
// 3. Gates 20 and 30 carry the same payload contract.
// 4. A non-gate level (e.g. 7) carries amount == 7 — the subscriber's
//    gate check must not fire there.

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
    // ---------- 1: exactly level 10 ----------
    {
        EventBus bus; GameClock clock; RNG rng(1001);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        uint64_t a = m.addBot(0);
        std::vector<LevelHit> hits;
        bus.subscribe(EventType::PlayerLeveledUp, [&](const GameEvent& e) {
            hits.push_back({e.sourceId, static_cast<int>(e.amount),
                            e.tag});
        });
        // Level = 1 + xp/50: 450 xp lands exactly on 10.
        CHECK(m.grantXp(a, 450));
        const Match::Player* p = m.findPlayer(a);
        CHECK(p && p->level == 10);
        CHECK(hits.size() == 1);
        CHECK(lastOf(hits).sourceId == a); // driver matches this to the human
        CHECK(lastOf(hits).level == 10);
        CHECK(lastOf(hits).tag == p->name);
    }
    // ---------- 2: XP jump 1 -> 15 still one event, amount == 15 ----------
    {
        EventBus bus; GameClock clock; RNG rng(1002);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        uint64_t a = m.addBot(0);
        std::vector<LevelHit> hits;
        bus.subscribe(EventType::PlayerLeveledUp, [&](const GameEvent& e) {
            hits.push_back({e.sourceId, static_cast<int>(e.amount),
                            e.tag});
        });
        // 700 xp: 1 + 700/50 = 15 — one event for the whole jump.
        CHECK(m.grantXp(a, 700));
        const Match::Player* p = m.findPlayer(a);
        CHECK(p && p->level == 15);
        CHECK(hits.size() == 1);
        CHECK(lastOf(hits).sourceId == a);
        CHECK(lastOf(hits).level == 15);
    }
    // ---------- 3: gates 20 and 30 ----------
    {
        EventBus bus; GameClock clock; RNG rng(1003);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        uint64_t a = m.addBot(0);
        std::vector<LevelHit> hits;
        bus.subscribe(EventType::PlayerLeveledUp, [&](const GameEvent& e) {
            hits.push_back({e.sourceId, static_cast<int>(e.amount),
                            e.tag});
        });
        // 950 xp: level 20; another 500 xp: level 30.
        CHECK(m.grantXp(a, 950));
        CHECK(m.grantXp(a, 500));
        const Match::Player* p = m.findPlayer(a);
        CHECK(p && p->level == 30);
        CHECK(hits.size() == 2);
        CHECK(hits[0].sourceId == a && hits[0].level == 20);
        CHECK(hits[1].sourceId == a && hits[1].level == 30);
    }
    // ---------- 4: non-gate level stays gate-free ----------
    {
        EventBus bus; GameClock clock; RNG rng(1004);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t a = m.addBot(0);
        std::vector<LevelHit> hits;
        bus.subscribe(EventType::PlayerLeveledUp, [&](const GameEvent& e) {
            hits.push_back({e.sourceId, static_cast<int>(e.amount),
                            e.tag});
        });
        CHECK(m.grantXp(a, 300)); // lands on level 7
        CHECK(hits.size() == 1);
        const int lv = lastOf(hits).level;
        CHECK(lv == 7);
        CHECK(!(lv == 10 || lv == 20 || lv == 30));
    }

    if (failures == 0) std::cout << "ALL CHECKS PASSED\n";
    return failures == 0 ? 0 : 1;
}
