// CULT-ULHU micro-dev regression tests: low-health warning.
//
// Match bots already retreat at 35% HP (their self-preservation pass); the
// joined human got no equivalent cue and died silent. Match now publishes
// PlayerLowHp once per 35%-crossing (re-armed by healing above 50% or by
// respawning), and the driver prints "*** LOW HEALTH — Nhp left! ***" for
// the joined human only. These pin:
//
// 1. Contract: sourceId = victim id, faction = team, tag = name,
//    0 < amount < 0.35; nothing at/above the line.
// 2. Exactly-once: further damage — even the killing blow — re-fires
//    nothing (death has its own announcement).
// 3. Re-arm on respawn: death -> respawn -> next dip warns again.
// 4. Re-arm on heal: a mother_hydra F heal back above 50% re-arms; the
//    next dip warns again.

#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "modes/Match.h"

#include <cmath>
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

struct LowHit {
    uint64_t sourceId;
    int faction;
    float amount;
    std::string tag;
};

static void tickMatch(Match& m, int seconds) {
    for (int i = 0; i < seconds && !m.isOver(); ++i) m.update(1.0);
}

int main() {
    // ---------- 1+2: contract, thresholds, exactly-once ----------
    {
        EventBus bus; GameClock clock; RNG rng(21);
        std::vector<LowHit> hits;
        bus.subscribe(EventType::PlayerLowHp,
                      [&hits](const GameEvent& e) {
                          hits.push_back({e.sourceId, e.faction,
                                          e.amount, e.tag});
                      });
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        const uint64_t a = m.addBot(0);
        const Match::Player* pl = m.findPlayer(a);
        CHECK(pl != nullptr);
        const float maxHp = pl->maxHp;

        CHECK(m.debugDamagePlayer(a, 0.10f * maxHp)); // 90% — silent
        CHECK(hits.empty());
        CHECK(m.debugDamagePlayer(a, 0.55f * maxHp)); // 35% exactly — silent
        CHECK(hits.empty());
        CHECK(m.debugDamagePlayer(a, 0.01f * maxHp)); // 34% — warns once
        CHECK(hits.size() == 1);
        const Match::Player* p = m.findPlayer(a);
        CHECK(hits[0].sourceId == a);
        CHECK(hits[0].faction == 0);
        CHECK(hits[0].tag == p->name);
        CHECK(!hits[0].tag.empty());
        CHECK(hits[0].amount > 0.0f && hits[0].amount < 0.35f);
        CHECK(std::fabs(hits[0].amount - p->hp / p->maxHp) < 1e-4f);

        // 2. More damage — even the killing blow — re-fires nothing.
        CHECK(m.debugDamagePlayer(a, 0.10f * maxHp));
        CHECK(hits.size() == 1);
        CHECK(m.debugDamagePlayer(a, 10.0f * maxHp)); // lethal
        CHECK(!m.findPlayer(a)->alive);
        CHECK(hits.size() == 1);
    }

    // ---------- 3: re-arm on respawn ----------
    {
        EventBus bus; GameClock clock; RNG rng(22);
        int hits = 0;
        bus.subscribe(EventType::PlayerLowHp,
                      [&hits](const GameEvent&) { ++hits; });
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        const uint64_t a = m.addBot(0);
        const float maxHp = m.findPlayer(a)->maxHp;

        CHECK(m.debugDamagePlayer(a, 0.80f * maxHp)); // 20% — warns
        CHECK(hits == 1);
        CHECK(m.debugDamagePlayer(a, 10.0f * maxHp)); // lethal — no re-warn
        CHECK(hits == 1);
        tickMatch(m, 6); // Domination respawns in 5s
        CHECK(m.findPlayer(a)->alive);
        CHECK(m.debugDamagePlayer(a, 0.80f * maxHp)); // 20% again — warns
        CHECK(hits == 2);
    }

    // ---------- 4: re-arm on heal ----------
    {
        EventBus bus; GameClock clock; RNG rng(23);
        int hits = 0;
        bus.subscribe(EventType::PlayerLowHp,
                      [&hits](const GameEvent&) { ++hits; });
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        const uint64_t a = m.addBot(0);
        CHECK(m.setPlayerKit(a, "mother_hydra"));
        const float maxHp = m.findPlayer(a)->maxHp;

        CHECK(m.debugDamagePlayer(a, 0.70f * maxHp)); // 30% — warns
        CHECK(hits == 1);
        // F is the heal (power 150 -> +300hp, tops up from anywhere).
        CHECK(m.humanCast(a, 1, 0.0f));
        CHECK(m.findPlayer(a)->hp >= 0.5f * maxHp);
        CHECK(m.debugDamagePlayer(a, 0.70f * maxHp)); // 30% again — warns
        CHECK(hits == 2);
    }

    // ---------- 5: silence contract — bots fire the event, the console
    // subscriber is what stays human-only (driver-side filter). ----------
    {
        EventBus bus; GameClock clock; RNG rng(24);
        int hits = 0;
        bus.subscribe(EventType::PlayerLowHp,
                      [&hits](const GameEvent&) { ++hits; });
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        const uint64_t a = m.addBot(0);
        const uint64_t b = m.addBot(1);
        const float maxHp = m.findPlayer(a)->maxHp;
        CHECK(m.debugDamagePlayer(a, 0.70f * maxHp));
        CHECK(m.debugDamagePlayer(b, 0.70f * maxHp));
        CHECK(hits == 2); // one per fighter, exactly-once each
    }

    if (failures == 0) std::cout << "tests_micro_lowhp: all green\n";
    return failures == 0 ? 0 : 1;
}
