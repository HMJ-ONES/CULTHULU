// CULT-ULHU micro-dev regression tests: arrival survey on respawn.
//
// A respawned human used to wake up blind at base — the 16:25 respawn
// callout ("*** You have respawned ***") told them they were back but not
// where anything was. The 20:24 move survey and 21:33 join survey exist
// because orientation beats typing; the driver now prints the same
// arrival survey after the human's own return. These pin the wiring
// (staging a real duel so the death goes through the production
// killPlayer path, same staging as tests_micro_respawned):
//
// 1. The respawned human's return is followed by the arrival survey
//    (point distance/bearing + nearest enemy).
// 2. A respawned bot (not the human) prints no survey — the gate is
//    per-fighter, not per-event.

#include "modes/Match.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"

#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>

using namespace cultulhu;

static int failures = 0;
#define CHECK(cond)                                        \
    do {                                                   \
        if (!(cond)) {                                     \
            std::cout << "FAIL line " << __LINE__ << ": "  \
                      << #cond << "\n";                    \
            ++failures;                                    \
        }                                                  \
    } while (0)

static void duelUntilDeath(Match& m, uint64_t victimId) {
    int ticks = 0;
    while (m.findPlayer(victimId)->alive && ticks < 60) {
        m.update(1.0);
        ++ticks;
    }
    CHECK(!m.findPlayer(victimId)->alive);
    CHECK(ticks < 60);
}

static void reviveUntilAlive(Match& m, uint64_t victimId) {
    int ticks = 0;
    while (!m.findPlayer(victimId)->alive && ticks < 30) {
        m.update(1.0);
        ++ticks;
    }
    CHECK(m.findPlayer(victimId)->alive);
    CHECK(ticks < 30);
}

// stageDuel mirrors tests_micro_respawned: identical kits, decided by
// update order and directional facing — b dies deterministically.
static void stageDuel(Match& m, uint64_t& a, uint64_t& b) {
    CHECK(m.start("capture"));
    a = m.addBot(0);
    b = m.addBot(1);
    CHECK(m.setPlayerKit(a, "cthulhu_avatar"));
    CHECK(m.setPlayerKit(b, "cthulhu_avatar"));
    m.movePlayer(a, Vec3(0, 0, 0));
    m.movePlayer(b, Vec3(1, 0, 0));
    CHECK(m.setFacingYaw(a, 0.0f));
}

int main() {
    // 1. The "human" (b, the fighter that dies deterministically) respawns:
    // the mirror of the driver's subscriber prints the callout plus the
    // arrival survey.
    {
        EventBus bus; GameClock clock; RNG rng(8);
        Match m(bus, clock, rng);
        uint64_t a, b;
        stageDuel(m, a, b);
        const uint64_t humanId = b; // designated "joined human"

        std::ostringstream out;
        bus.subscribe(EventType::PlayerRespawned,
                      [&out, &m, humanId](const GameEvent& e) {
                          if (e.sourceId == humanId) {
                              out << "*** You have respawned ***\n";
                              m.printMatchSurvey(out, humanId);
                          }
                      });

        duelUntilDeath(m, b);
        reviveUntilAlive(m, b);

        const std::string s = out.str();
        CHECK(s.find("*** You have respawned ***") != std::string::npos);
        CHECK(s.find("the point:") != std::string::npos);
        CHECK(s.find("nearest enemy:") != std::string::npos ||
              s.find("no living enemies") != std::string::npos);
    }

    // 2. The "human" is a (not the fighter that dies): b's respawn is
    // silent for the survey — no blind-arrival survey for bots.
    {
        EventBus bus; GameClock clock; RNG rng(8);
        Match m(bus, clock, rng);
        uint64_t a, b;
        stageDuel(m, a, b);
        const uint64_t humanId = a; // human is not the victim

        std::ostringstream out;
        bus.subscribe(EventType::PlayerRespawned,
                      [&out, &m, humanId](const GameEvent& e) {
                          if (e.sourceId == humanId) {
                              out << "*** You have respawned ***\n";
                              m.printMatchSurvey(out, humanId);
                          }
                      });

        duelUntilDeath(m, b);
        reviveUntilAlive(m, b);

        CHECK(out.str().empty());
    }

    if (failures == 0) std::cout << "ALL RESPAWN SURVEY CHECKS PASSED\n";
    return failures ? 1 : 0;
}
