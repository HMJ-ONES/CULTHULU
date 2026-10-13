// CULT-ULHU micro-dev regression tests: second-person achievement
// unlocks.
//
// Unlocking an achievement is a loud personal moment, but the published
// EventType::AchievementUnlocked carried no attribution — the driver could
// not tell whether the joined human earned it, so every unlock read in the
// omniscient third person ("* The dark takes note: ..."). The achievement
// system now stamps the triggering event's sourceId onto the published
// unlock, and the driver narrates the human's own unlock as
// "*** You unlocked <name>! ***" (humanMatchId-gated like the
// kill/streak/shutdown/first-blood callouts).
//
// These pin the payload contract (the driver's branch is a pure
// sourceId == humanMatchId comparison, same as the kill/streak pattern):
//
// 1. An attributed trigger (ConversionPerformed, sourceId = 42) unlocks
//    "first_flesh" with sourceId 42 on the published event.
// 2. An unattributed trigger (DistrictRazed with default sourceId 0)
//    publishes sourceId 0 — the human gate can never misfire on it.
// 3. Only one unlock per trigger; the second conversion fires nothing new
//    (first_flesh is one-shot).

#include "achievements/AchievementSystem.h"
#include "core/EventBus.h"
#include "core/Events.h"
#include "entities/Entity.h"

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

struct Unlock {
    std::string tag;
    uint64_t sourceId;
};

static GameEvent conversion(uint64_t src) {
    GameEvent e(EventType::ConversionPerformed);
    e.sourceId = src;
    e.amount = 1.0f;
    e.faction = FACTION_NEUTRAL; // no turncoat side-unlock
    return e;
}

int main() {
    // 1. Attributed trigger carries its sourceId through the unlock.
    {
        EventBus bus;
        AchievementSystem asys(bus);
        std::vector<Unlock> got;
        bus.subscribe(EventType::AchievementUnlocked,
                      [&got](const GameEvent& e) {
                          got.push_back({e.tag, e.sourceId});
                      });
        bus.publish(conversion(42));
        CHECK(got.size() == 1);
        CHECK(got[0].tag == "first_flesh");
        CHECK(got[0].sourceId == 42);
        // 3. Repeat conversion: one-shot, nothing more.
        bus.publish(conversion(42));
        CHECK(got.size() == 1);
    }

    // 2. Unattributed trigger publishes sourceId 0.
    {
        EventBus bus;
        AchievementSystem asys(bus);
        std::vector<Unlock> got;
        bus.subscribe(EventType::AchievementUnlocked,
                      [&got](const GameEvent& e) {
                          got.push_back({e.tag, e.sourceId});
                      });
        GameEvent e(EventType::DistrictRazed); // sourceId defaults to 0
        bus.publish(e);
        CHECK(got.size() == 1);
        CHECK(got[0].tag == "ashes_of_man");
        CHECK(got[0].sourceId == 0);
    }

    if (failures == 0) std::cout << "tests_micro_achievementyou: all green\n";
    return failures == 0 ? 0 : 1;
}
