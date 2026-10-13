// CULT-ULHU 8 PM bug-fix regression tests (2026-10-12): per-match event
// bus subscriptions.
//
// Two bugs, one class: handlers subscribed on the shared session EventBus
// outlived the object that installed them, because EventBus had no
// unsubscribe API.
//
// 1. EventBus: subscribe() returns a handle; unsubscribe() removes exactly
//    that handler (unknown/zero ids are no-ops).
// 2. MatchLogger: attach() twice never accumulates handlers; detach()
//    removes all 14; publishing after detach records nothing (the old code
//    double-logged every event of a second `match` in one session and
//    called match.findPlayer()/elapsed() on the destroyed Match).
// 3. Match: destroying a Match removes its constructor-time RelicClaimed
//    handler (the sibling [this]-capture the analysis's sweep caught).
// 4. setPlayerKit in Domination re-applies the full R cooldown (45s for
//    cthulhu_avatar) and re-arms the ready-ping — `match join <kit-id>`
//    no longer grants an instant ult; the DOTGOO draft path is untouched.
// 5. DOTGOO bots trip home to channel with exactly 1 pending invoke
//    (threshold was >= 2; a bot banking one charge sat on it forever).

#include "modes/Match.h"
#include "modes/MatchLogger.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"

#include <cstdint>
#include <iostream>
#include <string>

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

static void testEventBusUnsubscribe() {
    EventBus bus;
    int a = 0, b = 0;
    auto sa = bus.subscribe(EventType::PlayerKilled,
                            [&](const GameEvent&) { ++a; });
    auto sb = bus.subscribe(EventType::PlayerKilled,
                            [&](const GameEvent&) { ++b; });
    CHECK(sa != 0 && sb != 0 && sa != sb);
    GameEvent e(EventType::PlayerKilled);
    bus.publish(e);
    CHECK(a == 1 && b == 1);
    bus.unsubscribe(sa);
    CHECK(bus.handlerCount(EventType::PlayerKilled) == 1);
    bus.publish(e);
    CHECK(a == 1 && b == 2); // only the surviving handler fires
    bus.unsubscribe(99999);  // unknown id: no-op
    bus.unsubscribe(0);      // invalid id: no-op
    CHECK(bus.handlerCount(EventType::PlayerKilled) == 1);
    bus.unsubscribe(sb);
    CHECK(bus.handlerCount(EventType::PlayerKilled) == 0);
    bus.publish(e);
    CHECK(a == 1 && b == 2);
}

static void testMatchLoggerDetach() {
    EventBus bus;
    GameClock clock;
    RNG rng(5);
    Match m(bus, clock, rng);
    CHECK(m.start("capture"));

    MatchLogger log;
    log.attach(m);
    CHECK(bus.handlerCount(EventType::PlayerKilled) == 1);

    // A second match in the same session must not accumulate handlers.
    log.attach(m);
    CHECK(bus.handlerCount(EventType::PlayerKilled) == 1);

    log.detach();
    CHECK(bus.handlerCount(EventType::PlayerKilled) == 0);

    // Publishing after detach records nothing — and, critically, touches
    // no destroyed match object.
    GameEvent e(EventType::PlayerKilled);
    e.sourceId = 424242;
    e.targetId = 434343;
    bus.publish(e);
    CHECK(log.events().empty());

    // detach() is idempotent.
    log.detach();
    CHECK(bus.handlerCount(EventType::PlayerKilled) == 0);
}

static void testMatchDestructorUnsubscribes() {
    EventBus bus;
    GameClock clock;
    RNG rng(6);
    CHECK(bus.handlerCount(EventType::RelicClaimed) == 0);
    {
        Match m(bus, clock, rng);
        CHECK(bus.handlerCount(EventType::RelicClaimed) == 1);
        CHECK(m.start("moba"));
    }
    // Without the ~Match fix this stays 1: a dead handler capturing a
    // destroyed Match.
    CHECK(bus.handlerCount(EventType::RelicClaimed) == 0);
}

static void testSetPlayerKitReappliesUltCooldown() {
    // Domination: no free ult on kit swap.
    {
        EventBus bus;
        GameClock clock;
        RNG rng(11);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        const uint64_t id = m.addBot(0);
        CHECK(m.setPlayerKit(id, "cthulhu_avatar"));
        const Match::Player* p = m.findPlayer(id);
        CHECK(p != nullptr);
        if (p) {
            // Cthulhu's R cooldown is 45s: the kit swap starts the ult at
            // 0% charge, exactly like the opening whistle.
            CHECK(p->rCd == 45.0);
            CHECK(p->rChargePending); // ready-ping re-armed for this charge
            // Q/F stay ready; unlocks untouched.
            CHECK(p->qCd == 0.0 && p->fCd == 0.0);
            CHECK(p->qUnlocked && p->fUnlocked && p->rUnlocked);
        }
    }
    // DOTGOO: draft path untouched — zeroed cooldowns, no charge, and the
    // level-1 draft state is exactly as initAbilityUnlocks left it.
    {
        EventBus bus;
        GameClock clock;
        RNG rng(12);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        const uint64_t id = m.addBot(0);
        CHECK(m.setPlayerKit(id, "cthulhu_avatar"));
        const Match::Player* p = m.findPlayer(id);
        CHECK(p != nullptr);
        if (p) {
            CHECK(p->rCd == 0.0);
            CHECK(!p->rChargePending);
            CHECK(!p->qUnlocked && !p->fUnlocked && !p->rUnlocked);
            CHECK(p->pendingPicks == 1);
        }
    }
}

static void testBotTripsHomeOnOneInvoke() {
    EventBus bus;
    GameClock clock;
    RNG rng(99);
    Match m(bus, clock, rng);
    CHECK(m.start("moba"));
    const uint64_t id = m.addBot(0);
    CHECK(m.setPlayerKit(id, "cthulhu_avatar"));
    // Level 10 via the production XP path, then pin exactly one charge —
    // the state a bot banks when it stops leveling mid-game.
    CHECK(m.grantXp(id, 450));
    const Match::Player* p0 = m.findPlayer(id);
    CHECK(p0 != nullptr && p0->level >= 10);
    m.debugSetPendingInvokes(0, 1);

    // The 180s trip gate is already open (lastInvokeTrip starts at -1000),
    // so the very first ticks must start the invoke trip. With the old
    // >= 2 threshold this never fires.
    double tripAt = -1.0;
    for (int i = 0; i < 40; ++i) {
        m.update(0.25);
        const Match::Player* p = m.findPlayer(id);
        if (p && p->lastInvokeTrip > -1000.0) {
            tripAt = p->lastInvokeTrip;
            break;
        }
    }
    CHECK(tripAt >= 0.0);
}

int main() {
    testEventBusUnsubscribe();
    testMatchLoggerDetach();
    testMatchDestructorUnsubscribes();
    testSetPlayerKitReappliesUltCooldown();
    testBotTripsHomeOnOneInvoke();
    if (failures == 0) std::cout << "tests_micro_detach: all green\n";
    return failures == 0 ? 0 : 1;
}
