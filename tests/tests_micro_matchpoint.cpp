// CULT-ULHU micro-dev regression tests: Domination "match point"
// announcement.
//
// The final stretch of a Domination match used to be invisible — the score
// ticked from 299 to 400 with no callout, so the climax arrived without
// drama. Crossing MATCH_POINT_SCORE (300/400) now publishes EventType::
// MatchPoint (faction = team) exactly once per team. These pin:
//
// 1. Hold-driven crossing: team 0 holds the point; the event fires with
//    faction=0 at the first tick where score >= 300, not earlier.
// 2. Exactly once: many more ticks (past 400 would end a real match; the
//    mode itself doesn't gate) publish no second event for team 0.
// 3. Kill-driven crossing: notePlayerKill pushes team 1 over 300 mid-tick —
//    the event still fires with faction=1 (kills are fed before
//    mode_->update in Match::update).
// 4. Both teams can each get one: independence of the two flags.

#include "core/EventBus.h"
#include "core/GameClock.h"
#include "modes/CapturePointMode.h"

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

struct Hit {
    int faction;
    float amount;
};

int main() {
    EventBus bus;
    GameClock clock;
    std::vector<Hit> hits;
    bus.subscribe(EventType::MatchPoint, [&hits](const GameEvent& e) {
        hits.push_back({e.faction, e.amount});
    });

    CapturePointMode mode(bus, clock);
    mode.setupDominationPoint();
    mode.setOccupants(0, 1, 0); // team 0 holds uncontested: 1 pt/s

    // 1. No event before the crossing.
    for (int i = 0; i < 250; ++i) mode.update(1.0);
    CHECK(hits.empty());
    CHECK(mode.score(0) < CapturePointMode::MATCH_POINT_SCORE);

    // Cross 300: the event fires with faction=0, once.
    for (int i = 0; i < 60; ++i) mode.update(1.0);
    CHECK(hits.size() == 1);
    CHECK(hits[0].faction == 0);
    CHECK(hits[0].amount == CapturePointMode::MATCH_POINT_SCORE);
    CHECK(mode.score(0) >= CapturePointMode::MATCH_POINT_SCORE);

    // 2. More ticks: no second event for team 0.
    for (int i = 0; i < 100; ++i) mode.update(1.0);
    CHECK(hits.size() == 1);

    // 3. Kill-driven crossing for team 1: 60 kills x 5 = 300.
    for (int i = 0; i < 60; ++i) mode.notePlayerKill(1);
    mode.update(1.0); // the check runs after kills are scored
    CHECK(hits.size() == 2);
    CHECK(hits[1].faction == 1);

    // 4. More kills for team 1: still no repeat.
    for (int i = 0; i < 20; ++i) mode.notePlayerKill(1);
    mode.update(1.0);
    CHECK(hits.size() == 2);

    if (failures == 0) std::cout << "tests_micro_matchpoint: ALL GREEN\n";
    return failures ? 1 : 0;
}
