// CULT-ULHU micro-dev regression tests: relic-claim console callout.
//
// Jungle relic claims in DOTGOO were console-silent — the exported match
// log recorded RELIC lines, but a player watching the console never saw
// the power-up land. These pin, through the production match loop:
//
// 1. Walking a bot onto a tier-1 jungle relic publishes exactly one
//    RelicClaimed with sourceId = the claimant's player id, tag = the
//    relic name (non-empty), amount = the relic's amplifier (> 0). The
//    driver's console subscriber formats "(+N% damage)" from amount, so
//    the fields it reads are pinned here.
// 2. The claim is not repeated on later ticks.
// 3. The driver-side console gate (same predicate as src/driver/main.cpp)
//    fires for match-player claims and stays silent for RelicClaimed
//    events whose sourceId is not a match player (free-roam avatar and
//    ritual claims print their own lines).

#include "modes/Match.h"
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

// Same gate as the driver's console subscriber in main.cpp: announce
// only when the claimer's id resolves to a live match player.
static bool consoleFiresFor(const Match& m, const GameEvent& e) {
    return m.findPlayer(e.sourceId) != nullptr;
}

int main() {
    EventBus bus;
    GameClock clock;
    RNG rng(7);
    std::vector<GameEvent> claims;
    bus.subscribe(EventType::RelicClaimed,
                  [&claims](const GameEvent& e) { claims.push_back(e); });

    Match m(bus, clock, rng);
    CHECK(m.start("moba"));
    const uint64_t p = m.addBot(0);
    CHECK(m.setPlayerKit(p, "cthulhu_avatar"));

    // Tier-1 relics are walk-over pickups present from spawn (spawnTime 0).
    const auto& relics = m.moba()->relics();
    const MobaDefense::RelicPickup* r1 = nullptr;
    for (const auto& r : relics) {
        if (r.tier == 1 && r.spawnTime == 0.0 && r.claimedBy == 0) {
            r1 = &r;
            break;
        }
    }
    CHECK(r1 != nullptr);
    const std::string relicName = r1->name;
    const float relicAmp = r1->amplifier;
    const Vec3 relicPos = r1->pos;
    CHECK(!relicName.empty());
    CHECK(relicAmp > 0.0f);

    // 1: walk onto the relic through the production loop. Re-pin each tick
    // so bot AI wander can't dodge the 6 m claim radius.
    int ticks = 0;
    while (claims.empty() && ticks < 40) {
        m.movePlayer(p, relicPos);
        m.update(0.25);
        ++ticks;
    }
    CHECK(claims.size() == 1);
    if (!claims.empty()) {
        CHECK(claims[0].sourceId == p);
        CHECK(claims[0].tag == relicName);
        CHECK(claims[0].amount == relicAmp);
    }

    // 2: no repeat on later ticks.
    for (int i = 0; i < 20; ++i) m.update(0.25);
    CHECK(claims.size() == 1);

    // 3: the console gate fires for the match claim, stays silent for a
    // non-player claim (free-roam avatar id 424242 — not a match player).
    if (!claims.empty()) CHECK(consoleFiresFor(m, claims[0]));
    GameEvent foreign(EventType::RelicClaimed);
    foreign.sourceId = 424242;
    foreign.tag = relicName;
    foreign.amount = relicAmp;
    CHECK(!consoleFiresFor(m, foreign));

    if (failures == 0) std::cout << "tests_micro_relicclaim: all green\n";
    return failures == 0 ? 0 : 1;
}
