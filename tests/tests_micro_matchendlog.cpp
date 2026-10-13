// CULT-ULHU 2 PM build (2026-10-13): "close the match story" pass.
// The exported GitHub log (MatchLogger) opened with a START beat but had
// no closing beat and no achievement lines — the story just stopped.
//
// 1. A team-0 Domination win leaves exactly one END line naming the mode,
//    the winning team, and the final score; it fires exactly once.
// 2. A drawn Domination match (tie at the clock, no overtime score) logs
//    the draw branch ("draw", no winning team).
// 3. An AchievementUnlocked attributed to a player lands as one ACHIEVE
//    line with the player's name/kit/team and "unlocked <tag>".
// 4. An unattributed unlock (sourceId 0, sim-driven) falls back to the
//    bare tag, not an empty attribution.
//
// Staging for 1/2 follows tests_micro_matchend_you: every fighter is
// stunned for 900 s (nobody moves or kills). Team 0 parked on the point
// banks 1 pt/s uncontested and reaches 400 in 400 ticks. Both teams parked
// far away bank nothing: 600 s tie, overtime starts, 120 s cap with no
// score ends it as a draw (winner() == -1).
// No new console output: the driver's MatchEnded announcement already
// exists; this pass touches the exported event stream only.

#include "modes/Match.h"
#include "modes/MatchLogger.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "core/Events.h"

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

static int countKind(const MatchLogger& log, const std::string& kind) {
    int n = 0;
    for (const auto& e : log.events())
        if (e.kind == kind) ++n;
    return n;
}

static const LoggedEvent* lastOf(const MatchLogger& log,
                                 const std::string& kind) {
    const LoggedEvent* found = nullptr;
    for (const auto& e : log.events())
        if (e.kind == kind) found = &e;
    return found;
}

int main() {
    // ---------- 1: team-0 win writes the closing beat, once ----------
    {
        EventBus bus; GameClock clock; RNG rng(13101);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        m.botfill();
        MatchLogger log;
        log.attach(m);
        for (size_t i = 0; i < m.playerCount(); ++i) {
            const uint64_t id = m.player(i).id;
            CHECK(m.debugStunPlayer(id, 900.0)); // nobody moves or kills
            CHECK(m.movePlayer(id, m.player(i).team == 0 ? Vec3(0, 0, 0)
                                                         : Vec3(500, 0, 500)));
        }
        for (int t = 0; t < 600 && !m.isOver(); ++t) m.update(1.0);
        CHECK(m.isOver()); // team 0 banked 400 uncontested
        CHECK(m.winner() == 0);
        CHECK(countKind(log, "END") == 1);
        const LoggedEvent* end = lastOf(log, "END");
        CHECK(end != nullptr);
        CHECK(end->text.find("END Domination") != std::string::npos);
        CHECK(end->text.find("team 0 wins") != std::string::npos);
        CHECK(end->text.find("400") != std::string::npos); // final score
        for (int t = 0; t < 10; ++t) m.update(1.0);
        CHECK(countKind(log, "END") == 1); // the end beat fires once
    }

    // ---------- 2: a draw logs the draw branch ----------
    {
        EventBus bus; GameClock clock; RNG rng(13102);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        m.botfill();
        MatchLogger log;
        log.attach(m);
        for (size_t i = 0; i < m.playerCount(); ++i) {
            const uint64_t id = m.player(i).id;
            CHECK(m.debugStunPlayer(id, 900.0));
            // Both teams far from the point: nobody banks, the clock runs
            // out tied, overtime burns its 120 s cap with no score.
            CHECK(m.movePlayer(id, m.player(i).team == 0 ? Vec3(500, 0, 500)
                                                         : Vec3(-500, 0, -500)));
        }
        for (int t = 0; t < 900 && !m.isOver(); ++t) m.update(1.0);
        CHECK(m.isOver());
        CHECK(m.winner() == -1);
        CHECK(countKind(log, "END") == 1);
        const LoggedEvent* end = lastOf(log, "END");
        CHECK(end != nullptr);
        CHECK(end->text.find("END Domination") != std::string::npos);
        CHECK(end->text.find("draw") != std::string::npos);
        CHECK(end->text.find("wins") == std::string::npos); // no winner
    }

    // ---------- 3: attributed unlock lands in the story ----------
    {
        EventBus bus; GameClock clock; RNG rng(13103);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        const uint64_t pid = m.addPlayer("Achiever", 0);
        MatchLogger log;
        log.attach(m);
        GameEvent e(EventType::AchievementUnlocked);
        e.tag = "first_blood";
        e.sourceId = pid; // the production publisher stamps the trigger
        bus.publish(e);
        CHECK(countKind(log, "ACHIEVE") == 1);
        const LoggedEvent* a = lastOf(log, "ACHIEVE");
        CHECK(a != nullptr);
        CHECK(a->text.find("Achiever") != std::string::npos);
        CHECK(a->text.find("t0") != std::string::npos);
        CHECK(a->text.find("unlocked first_blood") != std::string::npos);
    }

    // ---------- 4: unattributed unlock falls back to the bare tag ----------
    {
        EventBus bus; GameClock clock; RNG rng(13104);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        MatchLogger log;
        log.attach(m);
        GameEvent e(EventType::AchievementUnlocked);
        e.tag = "godslayer";
        e.sourceId = 0; // sim-driven, no trigger to attribute
        bus.publish(e);
        CHECK(countKind(log, "ACHIEVE") == 1);
        const LoggedEvent* a = lastOf(log, "ACHIEVE");
        CHECK(a != nullptr);
        CHECK(a->text == "ACHIEVE godslayer");
    }

    if (failures == 0) std::cout << "micro_matchendlog: all checks passed\n";
    else std::cout << "micro_matchendlog: " << failures << " checks FAILED\n";
    return failures == 0 ? 0 : 1;
}
