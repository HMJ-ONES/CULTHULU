// CULT-ULHU micro-dev regression tests: kill credit in second person.
//
// The driver's PlayerKilled callout now reads "*** You slew <victim>! ***"
// when the joined human lands the kill, instead of the third-person
// "<victim> was slain by <your fighter>". The branch selects on
// `e.sourceId == humanMatchId`, so the contract it depends on is:
//
// 1. A player-on-player kill through the production melee path publishes
//    exactly one PlayerKilled with sourceId == the KILLER's player id and
//    targetId == the victim's player id (not a team, slot, or index), and
//    both ids resolve through findPlayer (the driver looks up both names).
// 2. A minion/tower-attributed kill (killerIdx < 0) publishes PlayerKilled
//    with sourceId == the given entity id, which resolves to NO player —
//    so the personal branch cannot misfire on minion kills and the driver
//    keeps the "the minions" read.

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

struct KillHit {
    uint64_t sourceId;
    uint64_t targetId;
};

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
    // ---------- 1. player kill: payload carries the killer's id ----------
    {
        EventBus bus; GameClock clock; RNG rng(700);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t killer = m.addBot(0);
        uint64_t victim = m.addBot(1);
        std::vector<KillHit> hits;
        bus.subscribe(EventType::PlayerKilled, [&](const GameEvent& e) {
            hits.push_back({e.sourceId, e.targetId});
        });
        stageVictim(m, killer, victim);
        killUntilDead(m, victim);
        CHECK(hits.size() == 1);
        if (!hits.empty()) {
            // The ids are player ids, usable against humanMatchId directly.
            CHECK(hits[0].sourceId == killer);
            CHECK(hits[0].targetId == victim);
            // The driver resolves both names for the callout line.
            const auto* k = m.findPlayer(hits[0].sourceId);
            const auto* v = m.findPlayer(hits[0].targetId);
            CHECK(k != nullptr && !k->name.empty());
            CHECK(v != nullptr && !v->name.empty());
        }
    }

    // ---------- 2. minion-attributed kill: sourceId is not a player ------
    {
        EventBus bus; GameClock clock; RNG rng(701);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t victim = m.addBot(1);
        std::vector<KillHit> hits;
        bus.subscribe(EventType::PlayerKilled, [&](const GameEvent& e) {
            hits.push_back({e.sourceId, e.targetId});
        });
        // debugDamagePlayer is the test-only raw-damage path: a lethal hit
        // routes through killPlayer(idx, -1, 0) — the minion/tower branch.
        CHECK(m.debugDamagePlayer(victim, 100000.0f));
        CHECK(hits.size() == 1);
        if (!hits.empty()) {
            CHECK(hits[0].sourceId == 0);
            CHECK(hits[0].targetId == victim);
            // No player resolves: the personal branch (sourceId ==
            // humanMatchId) cannot misfire, the driver reads "the minions".
            CHECK(m.findPlayer(hits[0].sourceId) == nullptr);
        }
    }

    if (failures == 0) std::cout << "tests_micro_killcredit: all green\n";
    return failures == 0 ? 0 : 1;
}
