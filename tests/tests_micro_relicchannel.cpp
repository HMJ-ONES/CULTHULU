// CULT-ULHU micro-dev regression tests: relic-channel-start callout.
//
// Tier-2/3 jungle relic channels (4s/6s) were invisible — the mythic relic
// manifests with a global announcement, but the channel itself was silent,
// so a contesting player never knew someone was already claiming it.
// These pin, through the production match loop:
//
// 1. Teleporting a bot onto a tier-2 jungle relic publishes exactly one
//    RelicChannelStarted — sourceId = channeler's player id,
//    faction = team, tag = relic name (non-empty), amount = tier (2),
//    pos = relic site — and it does NOT re-fire while the channel runs.
// 2. Interrupting the channel (walk away before the 4s complete) and
//    restarting it fires a second RelicChannelStarted.
// 3. Tier-1 walk-over claims never publish RelicChannelStarted.
// 4. The console gate (same predicate as src/driver/main.cpp) fires for
//    match-player channels and stays silent for foreign ids.

#include "modes/Match.h"
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

// Same gate as the driver's console subscriber in main.cpp: announce
// only when the channeler's id resolves to a live match player.
static bool consoleFiresFor(const Match& m, const GameEvent& e) {
    return m.findPlayer(e.sourceId) != nullptr;
}

int main() {
    EventBus bus;
    GameClock clock;
    RNG rng(11);
    std::vector<GameEvent> starts;
    std::vector<GameEvent> claims;
    bus.subscribe(EventType::RelicChannelStarted,
                  [&starts](const GameEvent& e) { starts.push_back(e); });
    bus.subscribe(EventType::RelicClaimed,
                  [&claims](const GameEvent& e) { claims.push_back(e); });

    Match m(bus, clock, rng);
    CHECK(m.start("moba"));
    const uint64_t p = m.addBot(0);
    CHECK(m.setPlayerKit(p, "cthulhu_avatar"));

    // Tier-2 relics are mid-jungle, present from spawn (spawnTime 0).
    const MobaDefense::RelicPickup* r2 = nullptr;
    for (const auto& r : m.moba()->relics()) {
        if (r.tier == 2 && r.spawnTime == 0.0 && r.claimedBy == 0) {
            r2 = &r;
            break;
        }
    }
    CHECK(r2 != nullptr);
    const std::string relicName = r2->name;
    const Vec3 relicPos = r2->pos;
    CHECK(!relicName.empty());

    // 1: channel start fires exactly once on the production loop. Re-pin
    // each tick so bot AI wander can't dodge the 6 m claim radius.
    int ticks = 0;
    while (starts.empty() && ticks < 40) {
        m.movePlayer(p, relicPos);
        m.update(0.25);
        ++ticks;
    }
    CHECK(starts.size() == 1);
    if (!starts.empty()) {
        CHECK(starts[0].sourceId == p);
        CHECK(starts[0].faction == 0);
        CHECK(starts[0].tag == relicName);
        CHECK(starts[0].amount == 2.0f);
        CHECK(starts[0].pos.distance(relicPos) < 1.0f);
    }

    // 1b: no re-fire while the channel keeps running.
    for (int i = 0; i < 4; ++i) {
        m.movePlayer(p, relicPos);
        m.update(0.25);
    }
    CHECK(starts.size() == 1);

    // 2: interrupt (walk away well short of the 4s channel) and restart —
    // a second start fires.
    m.movePlayer(p, Vec3(-80, 0, -80));
    for (int i = 0; i < 4; ++i) m.update(0.25);
    ticks = 0;
    while (starts.size() == 1 && ticks < 40) {
        m.movePlayer(p, relicPos);
        m.update(0.25);
        ++ticks;
    }
    CHECK(starts.size() == 2);
    if (starts.size() == 2) {
        CHECK(starts[1].sourceId == p);
        CHECK(starts[1].tag == relicName);
        CHECK(starts[1].amount == 2.0f);
    }

    // 3: tier-1 walk-over claims never channel. Claim one and confirm
    // RelicChannelStarted stays quiet while RelicClaimed fires.
    const size_t startsBefore = starts.size();
    const MobaDefense::RelicPickup* r1 = nullptr;
    for (const auto& r : m.moba()->relics()) {
        if (r.tier == 1 && r.claimedBy == 0) {
            r1 = &r;
            break;
        }
    }
    CHECK(r1 != nullptr);
    const Vec3 r1pos = r1->pos;
    ticks = 0;
    while (claims.empty() && ticks < 40) {
        m.movePlayer(p, r1pos);
        m.update(0.25);
        ++ticks;
    }
    CHECK(claims.size() == 1);
    CHECK(starts.size() == startsBefore);

    // 4: console gate — fires for the match claim, silent for a foreign id
    // (e.g. free-roam claims print their own lines).
    if (!starts.empty()) CHECK(consoleFiresFor(m, starts[0]));
    GameEvent foreign(EventType::RelicChannelStarted);
    foreign.sourceId = 424242;
    foreign.tag = relicName;
    foreign.amount = 2.0f;
    CHECK(!consoleFiresFor(m, foreign));

    if (failures == 0) std::cout << "tests_micro_relicchannel: all green\n";
    return failures == 0 ? 0 : 1;
}
