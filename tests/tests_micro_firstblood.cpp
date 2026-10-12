// CULT-ULHU micro-dev regression tests: "first blood" announcement.
//
// The match's first death used to pass silently on the console and as a
// bare KILL line in the exported match log — the opening beat of every
// match arrived without any drama. The first death now publishes
// EventType::FirstBlood (once per match, whatever the cause). These pin
// (staging a real duel so the death goes through the production
// killPlayer path, same staging as tests_micro_respawn):
//
// 1. No FirstBlood before the first death.
// 2. The first death publishes exactly one FirstBlood, with the killer's
//    player id as sourceId and the victim's id as targetId.
// 3. Later deaths in the same match publish nothing more (once per match).
// 4. A fresh match resets the flag — first blood can be drawn again.

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

struct Hit {
    uint64_t sourceId;
    uint64_t targetId;
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

int main() {
    EventBus bus; GameClock clock; RNG rng(8);
    std::vector<Hit> hits;
    bus.subscribe(EventType::FirstBlood, [&hits](const GameEvent& e) {
        hits.push_back({e.sourceId, e.targetId});
    });

    Match m(bus, clock, rng);
    CHECK(m.start("capture"));
    MatchLogger logger;
    logger.attach(m);
    uint64_t a = m.addBot(0);
    uint64_t b = m.addBot(1);
    // Identical kits — the duel is decided by update order and directional
    // facing, not kit asymmetry (same staging as tests_wave21).
    CHECK(m.setPlayerKit(a, "cthulhu_avatar"));
    CHECK(m.setPlayerKit(b, "cthulhu_avatar"));
    m.movePlayer(a, Vec3(0, 0, 0));
    m.movePlayer(b, Vec3(1, 0, 0));
    CHECK(m.setFacingYaw(a, 0.0f));

    // 1. No first blood before the first death.
    for (int i = 0; i < 5; ++i) m.update(1.0);
    CHECK(hits.empty());

    // 2. First death: exactly one event, killer = a, victim = b
    //    (deterministic per tests_wave21 / tests_micro_respawn).
    duelUntilDeath(m, b);
    CHECK(hits.size() == 1);
    CHECK(hits[0].sourceId == a);
    CHECK(hits[0].targetId == b);
    // 2b. The exported match log carries the opening beat too.
    {
        int fb = 0;
        for (const auto& e : logger.events()) {
            if (e.kind == "FIRSTBLOOD") {
                ++fb;
                CHECK(e.text.find("Bot1") != std::string::npos);
                CHECK(e.text.find("Bot2") != std::string::npos);
            }
        }
        CHECK(fb == 1);
    }

    // 3. The same match never re-fires: 30 more ticks with no new death
    //    leave the count at exactly one.
    for (int i = 0; i < 30; ++i) m.update(1.0);
    CHECK(hits.size() == 1);

    // 4. Fresh match: the flag resets — first blood fires again. Fresh
    //    RNG(8) so the duel staging is byte-identical to the proven one.
    GameClock clock2; RNG rng2(8);
    Match m2(bus, clock2, rng2);
    CHECK(m2.start("capture"));
    uint64_t c = m2.addBot(0);
    uint64_t d = m2.addBot(1);
    CHECK(m2.setPlayerKit(c, "cthulhu_avatar"));
    CHECK(m2.setPlayerKit(d, "cthulhu_avatar"));
    m2.movePlayer(c, Vec3(0, 0, 0));
    m2.movePlayer(d, Vec3(1, 0, 0));
    CHECK(m2.setFacingYaw(c, 0.0f));
    duelUntilDeath(m2, d);
    CHECK(hits.size() == 2);
    CHECK(hits[1].sourceId == c);
    CHECK(hits[1].targetId == d);

    if (failures == 0) std::cout << "ALL FIRST-BLOOD CHECKS PASSED\n";
    return failures ? 1 : 0;
}
