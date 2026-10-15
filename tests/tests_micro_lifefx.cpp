// CULT-ULHU micro-dev regression tests: match lifecycle FX land on the
// fighter, not the world origin.
//
// UltCharged, PlayerLeveledUp, PlayerRespawned resolve to the "buff"
// halo in fxForEvent and FirstBlood resolves to the impact smoke — all
// positioned visuals — but their publish sites in src/modes/Match.cpp
// never stamped pos, so the engine played them at (0,0,0). The same
// "worse than silence" class the 10:09/12:33 cast-pos micros closed for
// AbilityCast. These pin, through the production match loop:
//
// 1. UltCharged carries the charged fighter's position at fire time.
// 2. PlayerLeveledUp (via grantXp) carries the leveling fighter's pos.
// 3. FirstBlood carries the fallen fighter's position.
// 4. PlayerRespawned carries the actual spawn point (team base +
//    scatter) — the publish moved after spawnPlayer so pos is read
//    after the player is placed, not at the death site.

#include "modes/Match.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"

#include <cmath>
#include <cstdint>
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

static bool near(const Vec3& a, const Vec3& b) {
    return std::fabs(a.x - b.x) < 1e-3f && std::fabs(a.y - b.y) < 1e-3f &&
           std::fabs(a.z - b.z) < 1e-3f;
}

static bool atOrigin(const Vec3& v) {
    return std::fabs(v.x) < 1e-6f && std::fabs(v.y) < 1e-6f &&
           std::fabs(v.z) < 1e-6f;
}

static void stageDuel(Match& m, uint64_t& a, uint64_t& b) {
    CHECK(m.start("capture"));
    a = m.addBot(0);
    b = m.addBot(1);
    // Identical kits — the duel is decided by update order and directional
    // facing, not kit asymmetry (same staging as tests_wave21).
    CHECK(m.setPlayerKit(a, "cthulhu_avatar"));
    CHECK(m.setPlayerKit(b, "cthulhu_avatar"));
    m.movePlayer(a, Vec3(0, 0, 0));
    m.movePlayer(b, Vec3(1, 0, 0));
    CHECK(m.setFacingYaw(a, 0.0f));
}

int main() {
    // ---------- 1: UltCharged ----------
    {
        EventBus bus;
        GameClock clock;
        RNG rng(21);
        Match m(bus, clock, rng);
        struct Hit {
            Vec3 eventPos;
            Vec3 playerPos;
        };
        std::vector<Hit> hits;
        bus.subscribe(EventType::UltCharged,
                      [&m, &hits](const GameEvent& e) {
                          const Match::Player* p = m.findPlayer(e.sourceId);
                          hits.push_back({e.pos, p ? p->pos : Vec3()});
                      });
        CHECK(m.start("capture"));
        const uint64_t a = m.addBot(0);
        const uint64_t b = m.addBot(1);
        // The bots chase and duel, so their positions at fire time are
        // wherever the fight took them — the invariant is that the event
        // carries the fighter's ACTUAL position at fire time, not that it
        // avoids any particular point.
        m.movePlayer(a, Vec3(30, 0, 0));
        m.movePlayer(b, Vec3(31, 0, 0));
        for (int i = 0; i < 75 && !m.isOver(); ++i) m.update(1.0);
        CHECK(hits.size() == 2);
        bool sawLiveStamp = false;
        for (const auto& h : hits) {
            CHECK(near(h.eventPos, h.playerPos));
            // Positive control: at least one fighter was somewhere real
            // (not the default origin) when their ult charged.
            if (!atOrigin(h.playerPos)) sawLiveStamp = true;
        }
        CHECK(sawLiveStamp);
    }

    // ---------- 2: PlayerLeveledUp ----------
    {
        EventBus bus;
        GameClock clock;
        RNG rng(22);
        Match m(bus, clock, rng);
        struct Hit {
            Vec3 eventPos;
            Vec3 playerPos;
        };
        std::vector<Hit> hits;
        bus.subscribe(EventType::PlayerLeveledUp,
                      [&m, &hits](const GameEvent& e) {
                          const Match::Player* p = m.findPlayer(e.sourceId);
                          hits.push_back({e.pos, p ? p->pos : Vec3()});
                      });
        CHECK(m.start("capture"));
        const uint64_t pid = m.addPlayer("Hero", 0);
        m.movePlayer(pid, Vec3(120, 0, 30));
        m.grantXp(pid, 450); // level 10
        CHECK(hits.size() == 1);
        CHECK(!atOrigin(hits[0].eventPos));
        CHECK(near(hits[0].eventPos, hits[0].playerPos));
    }

    // ---------- 3: FirstBlood ----------
    {
        EventBus bus;
        GameClock clock;
        RNG rng(8);
        Match m(bus, clock, rng);
        struct Hit {
            Vec3 eventPos;
            Vec3 victimPos;
        };
        std::vector<Hit> hits;
        bus.subscribe(EventType::FirstBlood,
                      [&m, &hits](const GameEvent& e) {
                          const Match::Player* v = m.findPlayer(e.targetId);
                          hits.push_back({e.pos, v ? v->pos : Vec3()});
                      });
        uint64_t a = 0, b = 0;
        stageDuel(m, a, b);
        int ticks = 0;
        while (m.findPlayer(b)->alive && ticks < 60) {
            m.update(1.0);
            ++ticks;
        }
        CHECK(!m.findPlayer(b)->alive);
        CHECK(hits.size() == 1);
        CHECK(!atOrigin(hits[0].eventPos));
        CHECK(near(hits[0].eventPos, hits[0].victimPos));
    }

    // ---------- 4: PlayerRespawned ----------
    {
        EventBus bus;
        GameClock clock;
        RNG rng(8);
        Match m(bus, clock, rng);
        struct Hit {
            Vec3 eventPos;
            Vec3 playerPos;
        };
        std::vector<Hit> hits;
        bus.subscribe(EventType::PlayerRespawned,
                      [&m, &hits](const GameEvent& e) {
                          const Match::Player* p = m.findPlayer(e.sourceId);
                          hits.push_back({e.pos, p ? p->pos : Vec3()});
                      });
        uint64_t a = 0, b = 0;
        stageDuel(m, a, b);
        int ticks = 0;
        while (m.findPlayer(b)->alive && ticks < 60) {
            m.update(1.0);
            ++ticks;
        }
        CHECK(!m.findPlayer(b)->alive);
        ticks = 0;
        while (!m.findPlayer(b)->alive && ticks < 30) {
            m.update(1.0);
            ++ticks;
        }
        CHECK(m.findPlayer(b)->alive);
        CHECK(hits.size() == 1);
        // The halo lands at the spawn point (team base + scatter), and
        // the player is already placed when the event fires.
        CHECK(!atOrigin(hits[0].eventPos));
        CHECK(near(hits[0].eventPos, hits[0].playerPos));
        CHECK(m.findPlayer(b)->alive);
    }

    if (failures == 0) std::cout << "tests_micro_lifefx: all green\n";
    return failures == 0 ? 0 : 1;
}
