// CULT-ULHU micro-dev regression tests: match-end "victory" announcement.
//
// A 5v5 match used to end with nothing on the console — the only sign was
// the match-log upload line, so the final result never landed as a beat.
// The mode already published EventType::MatchEnded (tag = mode name,
// faction = winner team, -1 on a draw) exactly once; the driver's new
// subscriber names the winner out loud. These pin the event contract the
// callout depends on:
//
// 1. Hold-driven win: team 0 reaches TARGET_SCORE -> exactly one MatchEnded
//    with faction=0, tag="capture"; no second event on later ticks.
// 2. Kill-driven win at the whistle: team 1 leads when the clock expires ->
//    one MatchEnded, faction=1.
// 3. Scoreless draw: tied through overtime to the OVERTIME_LIMIT cap ->
//    one MatchEnded, faction=-1 (draw).
//
// The console wording itself (VICTORY/DRAW lines) is verified live in the
// driver — see the micro-dev log for 2026-10-12.

#include "core/EventBus.h"
#include "core/GameClock.h"
#include "modes/CapturePointMode.h"

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

struct Hit {
    int faction;
    std::string tag;
};

int main() {
    // ---------- 1. hold-driven win (team 0) ----------
    {
        EventBus bus;
        GameClock clock;
        std::vector<Hit> hits;
        bus.subscribe(EventType::MatchEnded,
                      [&hits](const GameEvent& e) {
                          hits.push_back({e.faction, e.tag});
                      });

        CapturePointMode mode(bus, clock);
        mode.setupDominationPoint();
        mode.setOccupants(0, 1, 0); // team 0 holds uncontested: 1 pt/s
        int ticks = 0;
        while (!mode.isOver() && ticks < 500) {
            mode.update(1.0);
            ++ticks;
        }
        CHECK(mode.isOver());
        CHECK(mode.winner() == 0);
        CHECK(hits.size() == 1);
        CHECK(hits[0].faction == 0);
        CHECK(hits[0].tag == "capture");

        // Later ticks never re-fire the ending.
        for (int i = 0; i < 30; ++i) mode.update(1.0);
        CHECK(hits.size() == 1);
    }

    // ---------- 2. kill-driven win at the whistle (team 1) ----------
    {
        EventBus bus;
        GameClock clock;
        std::vector<Hit> hits;
        bus.subscribe(EventType::MatchEnded,
                      [&hits](const GameEvent& e) {
                          hits.push_back({e.faction, e.tag});
                      });

        CapturePointMode mode(bus, clock);
        mode.setupDominationPoint();
        for (int i = 0; i < 5; ++i) mode.notePlayerKill(1); // 25-0
        mode.update(CapturePointMode::TIME_LIMIT);          // single huge dt
        CHECK(mode.isOver());
        CHECK(mode.winner() == 1);
        CHECK(hits.size() == 1);
        CHECK(hits[0].faction == 1);
        CHECK(hits[0].tag == "capture");
    }

    // ---------- 3. scoreless draw: tie burns through overtime ----------
    {
        EventBus bus;
        GameClock clock;
        std::vector<Hit> hits;
        bus.subscribe(EventType::MatchEnded,
                      [&hits](const GameEvent& e) {
                          hits.push_back({e.faction, e.tag});
                      });

        CapturePointMode mode(bus, clock);
        mode.setupDominationPoint();
        mode.setOccupants(0, 0, 0); // empty point: nobody banks
        mode.update(CapturePointMode::TIME_LIMIT);
        CHECK(mode.overtime()); // tied whistle -> sudden death
        int ticks = 0;
        while (!mode.isOver() && ticks < 200) {
            mode.update(1.0);
            ++ticks;
        }
        CHECK(mode.isOver());
        CHECK(mode.winner() == -1); // overtime cap expired tied: draw
        CHECK(hits.size() == 1);
        CHECK(hits[0].faction == -1);
        CHECK(hits[0].tag == "capture");
    }

    if (failures == 0) std::cout << "ALL VICTORY CHECKS PASSED\n";
    return failures == 0 ? 0 : 1;
}
