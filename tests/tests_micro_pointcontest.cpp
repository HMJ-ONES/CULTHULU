// CULT-ULHU 2 PM build regression tests: the Domination point-contest beat.
//
// CapturePointMode::update used to flip a held point to contested with no
// event at all — the bank stopped ticking and the exported story could
// never answer "when did we lose the point." Now a team->contested flip
// publishes PointContested (faction = the team that lost the bank) and the
// MatchLogger carries a CONTESTED line. These pin, through the production
// mode and logger:
//
// 1. team->contested fires PointContested exactly once, with the losing
//    team as faction, point index 0 as targetId, and the point pos stamped.
//    PointCaptured does NOT re-fire on the flip.
// 2. The flicker guard: staying contested across ticks fires nothing more.
// 3. neutral->contested (nobody lost a bank) and team->empty (bank lost
//    but not contested) stay silent.
// 4. contested->team still resolves to PointCaptured, not PointContested.
// 5. MatchLogger turns the event into a CONTESTED line naming the team.

#include "modes/CapturePointMode.h"
#include "modes/Match.h"
#include "modes/MatchLogger.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"

#include <cmath>
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

static bool near(const Vec3& a, const Vec3& b) {
    return std::fabs(a.x - b.x) < 1e-3f && std::fabs(a.y - b.y) < 1e-3f &&
           std::fabs(a.z - b.z) < 1e-3f;
}

int main() {
    // ---------- 1+2: team->contested fires once, flicker-guarded ----------
    {
        EventBus bus;
        GameClock clock;
        CapturePointMode m(bus, clock);
        m.setupDominationPoint();
        std::vector<GameEvent> contested, captured;
        bus.subscribe(EventType::PointContested,
                      [&contested](const GameEvent& e) {
                          contested.push_back(e);
                      });
        bus.subscribe(EventType::PointCaptured,
                      [&captured](const GameEvent& e) {
                          captured.push_back(e);
                      });
        m.setOccupants(0, 1, 0);
        m.update(1.0);
        CHECK(captured.size() == 1);
        CHECK(captured[0].faction == 0);
        CHECK(contested.empty());

        // The enemy steps in: bank stops, one beat.
        m.setOccupants(0, 1, 1);
        m.update(1.0);
        CHECK(m.contested());
        CHECK(m.holder() == -1);
        CHECK(contested.size() == 1);
        CHECK(contested[0].faction == 0); // team 0 lost the bank
        CHECK(contested[0].targetId == 0);
        CHECK(near(contested[0].pos, m.pointPos(0)));
        // PointCaptured must not re-fire on the flip.
        CHECK(captured.size() == 1);

        // Staying contested across ticks: no more beats.
        m.update(1.0);
        m.update(1.0);
        CHECK(contested.size() == 1);
    }

    // ---------- 3a: neutral->contested is silent ----------
    {
        EventBus bus;
        GameClock clock;
        CapturePointMode m(bus, clock);
        m.setupDominationPoint();
        std::vector<GameEvent> contested, captured;
        bus.subscribe(EventType::PointContested,
                      [&contested](const GameEvent& e) {
                          contested.push_back(e);
                      });
        bus.subscribe(EventType::PointCaptured,
                      [&captured](const GameEvent& e) {
                          captured.push_back(e);
                      });
        // Nobody held it; both teams arrive at once.
        m.setOccupants(0, 1, 1);
        m.update(1.0);
        CHECK(m.contested());
        CHECK(contested.empty());
        CHECK(captured.empty());
    }

    // ---------- 3b: team->empty (bank lost, not contested) is silent ------
    {
        EventBus bus;
        GameClock clock;
        CapturePointMode m(bus, clock);
        m.setupDominationPoint();
        std::vector<GameEvent> contested, captured;
        bus.subscribe(EventType::PointContested,
                      [&contested](const GameEvent& e) {
                          contested.push_back(e);
                      });
        bus.subscribe(EventType::PointCaptured,
                      [&captured](const GameEvent& e) {
                          captured.push_back(e);
                      });
        m.setOccupants(0, 1, 0);
        m.update(1.0);
        CHECK(captured.size() == 1);
        // The holder walks off alone: bank stops, but nothing contests it.
        m.setOccupants(0, 0, 0);
        m.update(1.0);
        CHECK(!m.contested());
        CHECK(m.holder() == -1);
        CHECK(contested.empty());
    }

    // ---------- 4: contested->team is PointCaptured, not PointContested ---
    {
        EventBus bus;
        GameClock clock;
        CapturePointMode m(bus, clock);
        m.setupDominationPoint();
        std::vector<GameEvent> contested, captured;
        bus.subscribe(EventType::PointContested,
                      [&contested](const GameEvent& e) {
                          contested.push_back(e);
                      });
        bus.subscribe(EventType::PointCaptured,
                      [&captured](const GameEvent& e) {
                          captured.push_back(e);
                      });
        m.setOccupants(0, 1, 0);
        m.update(1.0);
        m.setOccupants(0, 1, 1);
        m.update(1.0);
        CHECK(contested.size() == 1);
        // Team 0 wipes team 1 off the point: a clean re-capture.
        m.setOccupants(0, 0, 1);
        m.update(1.0);
        CHECK(captured.size() == 2);
        CHECK(captured[1].faction == 1);
        CHECK(contested.size() == 1);
    }

    // ---------- 5: MatchLogger CONTESTED line ----------
    {
        EventBus bus;
        GameClock clock;
        RNG rng(7);
        Match m(bus, clock, rng);
        MatchLogger logger;
        logger.attach(m);
        GameEvent e(EventType::PointContested);
        e.targetId = 0;
        e.faction = 1;
        e.amount = 1.0f;
        bus.publish(e);
        bool sawContested = false;
        for (const LoggedEvent& le : logger.events()) {
            if (le.kind == "CONTESTED") {
                sawContested = true;
                CHECK(le.text.find("t1") != std::string::npos);
                CHECK(le.text.find("contested") != std::string::npos);
            }
        }
        CHECK(sawContested);
        logger.detach();
    }

    if (failures == 0) std::cout << "tests_micro_pointcontest: all green\n";
    return failures == 0 ? 0 : 1;
}
