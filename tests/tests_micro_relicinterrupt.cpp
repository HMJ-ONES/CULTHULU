// CULT-ULHU micro-dev regression tests: relic-channel-interrupt callout.
//
// Tier-2/3 jungle relic channels (4s/6s) announced their start but a broken
// channel died silently — the channeler saw "start channeling", then nothing
// until (maybe) the rival's claim line. These pin, through the production
// match loop, the new RelicChannelInterrupted event:
//
// 1. Channeling ~2s of a tier-2 relic then walking away publishes exactly
//    one RelicChannelInterrupted — sourceId = channeler's player id,
//    faction = team, tag = relic name (non-empty), amount = seconds already
//    invested (> 0), pos = relic site.
// 2. The interrupt fires exactly once per break — idling with nobody near
//    the relic publishes nothing more (the r.channelBy != 0 guard).
// 3. Restarting the channel after a break re-arms it: a second break
//    publishes a second interrupt.
// 4. The console gate (same predicate as src/driver/main.cpp) fires for the
//    match interrupt and stays silent for a foreign id.

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

// Same gate as the driver's console subscriber in main.cpp: announce only
// when the interrupted channeler's id resolves to a live match player.
static bool consoleFiresFor(const Match& m, const GameEvent& e) {
    return m.findPlayer(e.sourceId) != nullptr;
}

int main() {
    EventBus bus;
    GameClock clock;
    RNG rng(11);
    std::vector<GameEvent> interrupts;
    bus.subscribe(EventType::RelicChannelInterrupted,
                  [&interrupts](const GameEvent& e) { interrupts.push_back(e); });

    Match m(bus, clock, rng);
    CHECK(m.start("moba"));
    const uint64_t p = m.addBot(0);
    CHECK(m.setPlayerKit(p, "cthulhu_avatar"));

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

    // 1: start a channel and break it mid-cast. 6 ticks at 0.25s = ~1.5s
    // of a 4s channel — well short of the claim, then walk away.
    for (int i = 0; i < 6; ++i) {
        m.movePlayer(p, relicPos);
        m.update(0.25);
    }
    m.movePlayer(p, Vec3(-80, 0, -80));
    int ticks = 0;
    while (interrupts.empty() && ticks < 20) {
        m.update(0.25);
        ++ticks;
    }
    CHECK(interrupts.size() == 1);
    if (!interrupts.empty()) {
        CHECK(interrupts[0].sourceId == p);
        CHECK(interrupts[0].faction == 0);
        CHECK(interrupts[0].tag == relicName);
        CHECK(interrupts[0].amount > 0.5f);
        CHECK(interrupts[0].amount < 2.0f);
        CHECK(interrupts[0].pos.distance(relicPos) < 1.0f);
    }

    // 2: exactly-once — idling with nobody near the relic must not
    // re-fire the interrupt (the dropped-channel guard).
    for (int i = 0; i < 10; ++i) m.update(0.25);
    CHECK(interrupts.size() == 1);

    // 3: re-arm — channel again, break again, second interrupt fires with
    // the same payload contract.
    for (int i = 0; i < 6; ++i) {
        m.movePlayer(p, relicPos);
        m.update(0.25);
    }
    m.movePlayer(p, Vec3(-80, 0, -80));
    ticks = 0;
    while (interrupts.size() == 1 && ticks < 20) {
        m.update(0.25);
        ++ticks;
    }
    CHECK(interrupts.size() == 2);
    if (interrupts.size() == 2) {
        CHECK(interrupts[1].sourceId == p);
        CHECK(interrupts[1].tag == relicName);
        CHECK(interrupts[1].amount > 0.5f);
        CHECK(interrupts[1].amount < 2.0f);
    }

    // 4: console gate — fires for the match interrupt, silent for a
    // foreign id (e.g. free-roam never publishes this event).
    if (!interrupts.empty()) CHECK(consoleFiresFor(m, interrupts[0]));
    GameEvent foreign(EventType::RelicChannelInterrupted);
    foreign.sourceId = 424242;
    foreign.tag = relicName;
    CHECK(!consoleFiresFor(m, foreign));

    if (failures == 0) std::cout << "tests_micro_relicinterrupt: all green\n";
    return failures == 0 ? 0 : 1;
}
