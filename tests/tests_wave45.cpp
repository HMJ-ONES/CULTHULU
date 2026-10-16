// CULT-ULHU 2 PM build (2026-10-12): level-up event regression tests.
// The morning analysis flagged the XP/level loop as completely silent —
// levels 10/20/30 gate the whole tiered-invocation endgame and nothing
// was published or narrated when a player crossed a gate.
// 1. addXp publishes exactly ONE PlayerLeveledUp per call (even on a
//    multi-level jump), with sourceId = player id, amount = new level,
//    tag = player name.
// 2. No event fires when XP does not cross a level boundary.
// 3. The level-30 cap still reports a single event with amount == 30.
// 4. fxForEvent(PlayerLeveledUp) resolves to the existing "buff" preset
//    (no new FX asset; within the particle budget).

#include "modes/Match.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "core/Events.h"
#include "characters/AbilityFx.h"

#include <fstream>
#include <iostream>
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
    uint64_t src;
    int level;
    std::string tag;
};

static std::string repoPrefix() {
    std::ifstream p("assets/fx/ability_fx.def");
    if (p) return "";
    return "../";
}

int main() {
    // ---------- 1: one event per addXp call, correct payload ----------
    {
        EventBus bus; GameClock clock; RNG rng(450);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t pid = m.addPlayer("Hero", 0);
        std::vector<LevelHit> hits;
        bus.subscribe(EventType::PlayerLeveledUp, [&](const GameEvent& e) {
            hits.push_back(
                {e.sourceId, static_cast<int>(e.amount), e.tag});
        });
        // Level = 1 + xp/50. +400 XP jumps 1 -> 9 in one call.
        m.grantXp(pid, 400);
        CHECK(m.findPlayer(pid)->level == 9);
        CHECK(hits.size() == 1);
        CHECK(hits[0].src == pid);
        CHECK(hits[0].level == 9);
        CHECK(hits[0].tag == "Hero");
        // +200 more -> 13: second call, second (single) event.
        m.grantXp(pid, 200);
        CHECK(m.findPlayer(pid)->level == 13);
        CHECK(hits.size() == 2);
        CHECK(hits[1].level == 13);
    }
    // ---------- 2: no boundary crossed, no event ----------
    {
        EventBus bus; GameClock clock; RNG rng(451);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t pid = m.addPlayer("Quiet", 1);
        int seen = 0;
        bus.subscribe(EventType::PlayerLeveledUp,
                      [&](const GameEvent&) { ++seen; });
        m.grantXp(pid, 10); // still level 1
        CHECK(m.findPlayer(pid)->level == 1);
        CHECK(seen == 0);
    }
    // ---------- 3: level-30 cap still reports one event at 30 ----------
    {
        EventBus bus; GameClock clock; RNG rng(452);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t pid = m.addPlayer("Max", 0);
        std::vector<LevelHit> hits;
        bus.subscribe(EventType::PlayerLeveledUp, [&](const GameEvent& e) {
            hits.push_back(
                {e.sourceId, static_cast<int>(e.amount), e.tag});
        });
        m.grantXp(pid, 5000); // would be 1+100 = 101, capped at 30
        CHECK(m.findPlayer(pid)->level == 30);
        CHECK(hits.size() == 1);
        CHECK(hits[0].level == 30);
        CHECK(hits[0].tag == "Max");
    }
    // ---------- 4: FX resolves to the buff preset ----------
    {
        FxLoadResult r =
            loadFxLibrary(repoPrefix() + "assets/fx/ability_fx.def");
        CHECK(r.ok);
        if (r.ok) {
            const FxPreset* viaEvent =
                fxForEvent(EventType::PlayerLeveledUp, r.library);
            const FxPreset* buff = r.library.find("buff");
            CHECK(buff != nullptr);
            CHECK(viaEvent == buff);
        }
    }
    if (failures == 0) std::cout << "wave45: all checks passed\n";
    else std::cout << "wave45: " << failures << " checks FAILED\n";
    return failures == 0 ? 0 : 1;
}
