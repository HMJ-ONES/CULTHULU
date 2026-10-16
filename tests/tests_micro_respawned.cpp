// CULT-ULHU micro-dev regression tests: respawn announcement.
//
// Dead players used to come back silently — the "DEAD (respawn Ns)" row in
// `match status` just cleared and the exported match log jumped from a KILL
// line to nothing. The respawn-timer expiry now publishes
// EventType::PlayerRespawned (once per revival), the driver announces the
// return on the console, and the match log carries a RESPAWNED line. These
// pin (staging a real duel so the death goes through the production
// killPlayer path, same staging as tests_micro_respawn / tests_micro_firstblood):
//
// 1. A fresh match has no respawns (initial spawns at match start stay silent).
// 2. During the death countdown, nothing publishes — only the timer-expiry
//    tick fires the event.
// 3. Exactly one PlayerRespawned on revival, with sourceId = the revived
//    player's id, tag = name, faction = team.
// 4. The exported match log carries the RESPAWNED line.
// 5. A later death→revival cycle publishes again (per-occurrence, not once
//    per match like FirstBlood).

#include "modes/Match.h"
#include "modes/MatchLogger.h"
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

struct SpawnHit {
    uint64_t sourceId;
    std::string tag;
    int faction;
};

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
    EventBus bus; GameClock clock; RNG rng(8);
    std::vector<SpawnHit> hits;
    bus.subscribe(EventType::PlayerRespawned, [&hits](const GameEvent& e) {
        hits.push_back({e.sourceId, e.tag, e.faction});
    });

    Match m(bus, clock, rng);
    MatchLogger logger;
    uint64_t a, b;
    stageDuel(m, a, b);
    logger.attach(m);

    // 1. Fresh match: initial spawns are silent.
    for (int i = 0; i < 5; ++i) m.update(1.0);
    CHECK(hits.empty());

    // 2. The duel kills b (deterministic per tests_micro_respawn); the
    // countdown ticks produce no respawn events.
    duelUntilDeath(m, b);
    CHECK(hits.empty());
    for (int i = 0; i < 2; ++i) m.update(1.0);
    CHECK(!m.findPlayer(b)->alive);
    CHECK(hits.empty());

    // 3. The timer-expiry tick revives b and fires exactly one event.
    reviveUntilAlive(m, b);
    CHECK(hits.size() == 1);
    CHECK(hits[0].sourceId == b);
    CHECK(hits[0].tag == "Bot2");
    CHECK(hits[0].faction == 1);

    // 4. The exported match log carries the return.
    {
        int n = 0;
        for (const auto& e : logger.events()) {
            if (e.kind == "RESPAWNED") {
                ++n;
                CHECK(e.text.find("Bot2") != std::string::npos);
                CHECK(e.text.find("t1") != std::string::npos);
            }
        }
        CHECK(n == 1);
    }

    // 5. No once-per-match latch: a fresh match fires the event again
    // (unlike FirstBlood, which is pinned to once per match).
    {
        EventBus bus2; GameClock clock2; RNG rng2(8);
        std::vector<SpawnHit> hits2;
        bus2.subscribe(EventType::PlayerRespawned,
                       [&hits2](const GameEvent& e) {
                           hits2.push_back({e.sourceId, e.tag, e.faction});
                       });
        Match m2(bus2, clock2, rng2);
        uint64_t c, d;
        stageDuel(m2, c, d);
        duelUntilDeath(m2, d);
        CHECK(hits2.empty());
        reviveUntilAlive(m2, d);
        CHECK(hits2.size() == 1);
        CHECK(hits2[0].sourceId == d);
        CHECK(hits2[0].tag == "Bot2");
    }

    if (failures == 0) std::cout << "ALL RESPAWNED CHECKS PASSED\n";
    return failures ? 1 : 0;
}
