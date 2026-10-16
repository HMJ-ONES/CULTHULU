// CULT-ULHU micro-dev regression tests: "streak ended" (shutdown) callout.
//
// Kill streaks announce at 2+ (wave 42), but a big streak dying used to pass
// silently — the victim's killStreak just reset to 0 with no record of the
// shutdown. The killPlayer path now publishes EventType::StreakEnded when a
// victim dies holding a 3+ streak. These pin (reusing the wave-42 staging:
// victims placed in the killer's frontal cone facing away so the killer
// takes no return damage; XP doesn't change melee damage in capture mode,
// so every kill lands on tick 7, inside the 8s streak window):
//
// 1. A 3-streak victim's death publishes exactly one StreakEnded:
//    sourceId = killer's player id, targetId = victim id, amount = 3,
//    tag = killer's name. The victim's streak resets to 0.
// 2. A 2-streak death publishes nothing (shutdowns are for triple+).
// 3. The exported match log carries a SHUTDOWN line with both names.
// 4. A later death in the same match doesn't re-fire; a fresh match's new
//    3-streak fires again.

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

struct EndHit {
    uint64_t sourceId;
    uint64_t targetId;
    int amount;
    std::string tag;
};

static const Vec3 kFar(40.0f, 0, 40.0f);

// Put a victim in the killer's frontal cone, facing away so it never
// swings back (melee is a directional cone).
static void stageVictim(Match& m, uint64_t killer, uint64_t victim) {
    m.movePlayer(killer, Vec3(0, 0, 0));
    m.setFacingYaw(killer, 0.0f); // facing +x
    m.movePlayer(victim, Vec3(1.5f, 0, 0));
    m.setFacingYaw(victim, 0.0f); // facing +x: the killer is behind it
}

// Tick until `victim` dies; returns the elapsed match time of the kill.
static double killUntilDead(Match& m, uint64_t victim) {
    for (int i = 0; i < 40; ++i) {
        m.update(1.0);
        const auto* pv = m.findPlayer(victim);
        CHECK(pv != nullptr);
        if (pv && !pv->alive) return m.elapsed();
    }
    CHECK(false); // the victim never died — staging is broken
    return m.elapsed();
}

int main() {
    // ---------- 1. A 3-streak shutdown publishes exactly once ----------
    {
        EventBus bus; GameClock clock; RNG rng(429);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        std::vector<EndHit> hits;
        bus.subscribe(EventType::StreakEnded, [&](const GameEvent& e) {
            hits.push_back({e.sourceId, e.targetId,
                            static_cast<int>(e.amount), e.tag});
        });
        MatchLogger logger;
        logger.attach(m);
        uint64_t k = m.addPlayer("K", 0);
        uint64_t v1 = m.addPlayer("V1", 1);
        uint64_t v2 = m.addPlayer("V2", 1);
        uint64_t v3 = m.addPlayer("V3", 1);
        uint64_t e4 = m.addPlayer("E4", 1);
        m.movePlayer(v2, kFar);
        m.movePlayer(v3, kFar);
        m.movePlayer(e4, kFar);
        CHECK(m.setPlayerKit(k, "cthulhu_avatar"));
        CHECK(m.setPlayerKit(e4, "cthulhu_avatar"));
        stageVictim(m, k, v1);
        killUntilDead(m, v1);
        stageVictim(m, k, v2);
        killUntilDead(m, v2);
        stageVictim(m, k, v3);
        killUntilDead(m, v3);
        CHECK(m.findPlayer(k)->killStreak == 3);
        // E4 faces the streak-holder while the holder looks away: only
        // E4 swings, exactly like the wave-42 death staging.
        m.movePlayer(k, Vec3(0, 0, 0));
        m.setFacingYaw(k, 0.0f);
        m.movePlayer(e4, Vec3(-1.5f, 0, 0));
        m.setFacingYaw(e4, 0.0f); // facing +x: toward the holder
        killUntilDead(m, k);
        CHECK(hits.size() == 1);
        CHECK(hits[0].sourceId == e4);
        CHECK(hits[0].targetId == k);
        CHECK(hits[0].amount == 3);
        CHECK(hits[0].tag == "E4");
        CHECK(m.findPlayer(k)->killStreak == 0); // the streak is over
        // The exported log records the shutdown beat.
        int sd = 0;
        for (const auto& e : logger.events()) {
            if (e.kind == "SHUTDOWN") {
                ++sd;
                CHECK(e.text.find("E4") != std::string::npos);
                CHECK(e.text.find("K") != std::string::npos);
                CHECK(e.text.find("TRIPLE KILL") != std::string::npos);
            }
        }
        CHECK(sd == 1);
        // No further 3+ streak dies in this match: extra ticks stay silent.
        for (int i = 0; i < 10; ++i) m.update(1.0);
        CHECK(hits.size() == 1);
    }

    // ---------- 2. A 2-streak death is not a shutdown ----------
    {
        EventBus bus; GameClock clock; RNG rng(430);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        std::vector<EndHit> hits;
        bus.subscribe(EventType::StreakEnded, [&](const GameEvent& e) {
            hits.push_back({e.sourceId, e.targetId,
                            static_cast<int>(e.amount), e.tag});
        });
        uint64_t k = m.addPlayer("K", 0);
        uint64_t v1 = m.addPlayer("V1", 1);
        uint64_t v2 = m.addPlayer("V2", 1);
        uint64_t e3 = m.addPlayer("E3", 1);
        m.movePlayer(v2, kFar);
        m.movePlayer(e3, kFar);
        stageVictim(m, k, v1);
        killUntilDead(m, v1);
        stageVictim(m, k, v2);
        killUntilDead(m, v2);
        CHECK(m.findPlayer(k)->killStreak == 2);
        m.movePlayer(k, Vec3(0, 0, 0));
        m.setFacingYaw(k, 0.0f);
        m.movePlayer(e3, Vec3(-1.5f, 0, 0));
        m.setFacingYaw(e3, 0.0f);
        killUntilDead(m, k);
        CHECK(hits.empty()); // DOUBLE KILL dying is silent
    }

    if (failures == 0) std::cout << "ALL STREAK-END CHECKS PASSED\n";
    return failures ? 1 : 0;
}
