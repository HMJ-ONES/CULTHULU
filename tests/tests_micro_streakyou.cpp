// CULT-ULHU micro-dev regression tests: kill streaks in second person.
//
// The driver's KillStreak callout now reads "*** YOUR RAMPAGE (5)! ***"
// when the joined human earns the streak, instead of the third-person
// "<your fighter> — RAMPAGE (5)". The branch selects on
// `e.sourceId == humanMatchId`, so the contract it depends on is:
//
// 1. Two player-attributed kills inside the 8s streak window publish
//    exactly one KillStreak with sourceId == the STREAKING player's id
//    (not a team, slot, or index) and tag == the player's name — the
//    driver compares sourceId to humanMatchId directly.
// 2. A lone kill publishes no KillStreak (the >= 2 threshold).

#include "modes/Match.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"

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

struct StreakHit {
    uint64_t sourceId;
    double amount;
    std::string tag;
};

// Put a victim in the killer's frontal cone, facing away so it never
// swings back (melee is a directional cone).
static void stageVictim(Match& m, uint64_t killer, uint64_t victim) {
    m.movePlayer(killer, Vec3(0, 0, 0));
    m.setFacingYaw(killer, 0.0f); // facing +x
    m.movePlayer(victim, Vec3(1.5f, 0, 0));
    m.setFacingYaw(victim, 0.0f); // facing +x: the killer is behind it
}

// Soften the victim to 1 HP through the test-only path, then let the
// production melee path land the killing blow (player-attributed).
static void killViaMelee(Match& m, uint64_t killer, uint64_t victim) {
    const auto* pv = m.findPlayer(victim);
    CHECK(pv != nullptr && pv->alive);
    if (pv && pv->alive && pv->hp > 1.0f)
        CHECK(m.debugDamagePlayer(victim, pv->hp - 1.0f));
    stageVictim(m, killer, victim);
    for (int i = 0; i < 40; ++i) {
        m.update(1.0);
        const auto* p = m.findPlayer(victim);
        CHECK(p != nullptr);
        if (p && !p->alive) return;
    }
    CHECK(false); // the victim never died — staging is broken
}

int main() {
    // ---------- 1. two quick kills: one KillStreak, player-id payload ---
    {
        EventBus bus; GameClock clock; RNG rng(710);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t killer = m.addBot(0);
        uint64_t v1 = m.addBot(1);
        uint64_t v2 = m.addBot(1);
        std::vector<StreakHit> hits;
        bus.subscribe(EventType::KillStreak, [&](const GameEvent& e) {
            hits.push_back({e.sourceId, e.amount, e.tag});
        });
        killViaMelee(m, killer, v1);
        killViaMelee(m, killer, v2); // staging is instant: inside the 8s window
        CHECK(hits.size() == 1);
        if (!hits.empty()) {
            // The id is the player's own id, usable against humanMatchId
            // directly — the driver's personal branch depends on this.
            CHECK(hits[0].sourceId == killer);
            CHECK(hits[0].amount == 2.0);
            const auto* k = m.findPlayer(hits[0].sourceId);
            CHECK(k != nullptr && !k->name.empty());
            CHECK(hits[0].tag == k->name);
        }
    }

    // ---------- 2. a lone kill publishes no KillStreak --------------------
    {
        EventBus bus; GameClock clock; RNG rng(711);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t killer = m.addBot(0);
        uint64_t v1 = m.addBot(1);
        int streaks = 0;
        bus.subscribe(EventType::KillStreak,
                      [&](const GameEvent&) { ++streaks; });
        killViaMelee(m, killer, v1);
        CHECK(streaks == 0);
    }

    if (failures == 0) std::cout << "tests_micro_streakyou: all green\n";
    return failures == 0 ? 0 : 1;
}
