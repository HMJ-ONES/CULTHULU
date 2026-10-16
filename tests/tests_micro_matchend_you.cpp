// CULT-ULHU micro-dev regression tests: match-end callout in second person.
//
// The driver's MatchEnded console read used to be third-person for
// everyone: "*** VICTORY — team 0 wins the Domination match! (400-12) ***".
// The joined human now hears it in second person:
//   "*** VICTORY — YOUR TEAM wins the Domination match! (400-12) ***"
// when their team takes the match, and
//   "*** DEFEAT — team 0 takes the Domination match. (400-375) ***"
// when the other team does. The draw read and the no-human third-person
// read are unchanged.
//
// The driver branches on `match->findPlayer(humanMatchId)->team` versus
// `e.faction` (the winner team). These pin, through the production
// start/botfill/joinAsHuman/tick path, exactly what those branches need:
//
// 1. A team-0 win fires exactly one MatchEnded (tag "capture",
//    faction == winner() == 0); the joined human's player resolves to
//    team 0, so the "YOUR TEAM wins" branch can fire.
// 2. Same staging with the human on team 1: faction == 0 while the
//    human's team is 1, so the "DEFEAT" branch can fire.
// 3. Ticks after the end never re-fire MatchEnded.
//
// Staging: every fighter is stunned for 900 s (nobody moves or kills),
// team 0 is parked on the Domination point (0,0,0) radius 10, team 1 far
// away — team 0 banks 1 pt/s uncontested and reaches 400 in 400 ticks.

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

struct EndEvent {
    int faction;
    std::string tag;
};

// (subscribes to the production MatchEnded path)
static void bus_subscribe(Match& m, std::vector<EndEvent>& ends) {
    m.bus().subscribe(EventType::MatchEnded,
                      [&](const GameEvent& e) {
                          ends.push_back({e.faction, e.tag});
                      });
}

int main() {
    // ---------- 1. Human's team wins: YOUR TEAM branch reachable ----------
    {
        EventBus bus; GameClock clock; RNG rng(7701);
        Match m(bus, clock, rng);
        std::vector<EndEvent> ends;
        CHECK(m.start("capture"));
        m.botfill();
        const uint64_t human = m.joinAsHuman(0);
        CHECK(human != 0);
        const Match::Player* hp0 = m.findPlayer(human);
        CHECK(hp0 != nullptr);
        CHECK(hp0->team == 0);
        bus_subscribe(m, ends); // subscribe before staging, miss nothing
        for (size_t i = 0; i < m.playerCount(); ++i) {
            const uint64_t id = m.player(i).id;
            CHECK(m.debugStunPlayer(id, 900.0)); // nobody moves or kills
            CHECK(m.movePlayer(id, m.player(i).team == 0 ? Vec3(0, 0, 0)
                                                         : Vec3(500, 0, 500)));
        }
        for (int t = 0; t < 600 && !m.isOver(); ++t) m.update(1.0);
        CHECK(m.isOver()); // team 0 banked 400 uncontested
        CHECK(m.winner() == 0);
        CHECK(ends.size() == 1);
        CHECK(ends[0].tag == "capture");
        CHECK(ends[0].faction == 0);
        CHECK(ends[0].faction == m.winner());
        const Match::Player* hp = m.findPlayer(human);
        CHECK(hp != nullptr);
        CHECK(hp->team == 0);
        CHECK(hp->team == ends[0].faction); // "YOUR TEAM wins" keys on this
        for (int t = 0; t < 10; ++t) m.update(1.0);
        CHECK(ends.size() == 1); // the whistle for the end blows once
    }

    // ---------- 2. Human's team loses: DEFEAT branch reachable ----------
    {
        EventBus bus; GameClock clock; RNG rng(7702);
        Match m(bus, clock, rng);
        std::vector<EndEvent> ends;
        CHECK(m.start("capture"));
        m.botfill();
        const uint64_t human = m.joinAsHuman(1);
        CHECK(human != 0);
        bus_subscribe(m, ends);
        for (size_t i = 0; i < m.playerCount(); ++i) {
            const uint64_t id = m.player(i).id;
            CHECK(m.debugStunPlayer(id, 900.0));
            CHECK(m.movePlayer(id, m.player(i).team == 0 ? Vec3(0, 0, 0)
                                                         : Vec3(500, 0, 500)));
        }
        for (int t = 0; t < 600 && !m.isOver(); ++t) m.update(1.0);
        CHECK(m.isOver());
        CHECK(m.winner() == 0);
        CHECK(ends.size() == 1);
        CHECK(ends[0].faction == 0);
        const Match::Player* hp = m.findPlayer(human);
        CHECK(hp != nullptr);
        CHECK(hp->team == 1);
        CHECK(hp->team != ends[0].faction); // "DEFEAT" keys on this
    }

    if (failures == 0) std::cout << "ALL MATCHEND_YOU OK\n";
    return failures ? 1 : 0;
}
