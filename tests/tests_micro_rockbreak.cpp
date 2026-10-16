// CULT-ULHU micro-dev regression tests: rock-break console callout.
//
// Cthulhu's Domination rock used to die silently — the match log recorded
// the break, but the console never told the holder their shield was gone.
// breakRock publishes AbilityCast tagged "the rock shatters" (blocked
// hits) or "the rock crumbles to dust" (swung to pieces), with the
// holder's player id as sourceId; the driver's console subscriber filters
// exactly those tags. These pin, through the production match loop:
//
// 1. Ripping the rock goes through the public cast path (humanCast, RMB
//    aimed down) and sets hasRock.
// 2. Blocked frontal damage drains the rock through the production path
//    (bot Q/melee -> applyRockShield) and publishes exactly one
//    "the rock shatters" AbilityCast with the holder's id as sourceId.
//    The holder is a joined human so no bot AI ever swings or hurls the
//    rock mid-test — the shatter path is deterministic.
// 3. No second break is published afterwards — one callout, never a
//    repeat.
// 4. The driver-side filter (same predicate as src/driver/main.cpp) fires
//    for both break tags and ignores ordinary ability casts.

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

// Same predicate as the driver's console subscriber in main.cpp.
static bool consoleFiresFor(const GameEvent& e) {
    return e.tag == "the rock shatters" ||
           e.tag == "the rock crumbles to dust";
}

int main() {
    // 1-3: a human holds the rock while a bot pounds it — the shield
    // breaks through the production blocked-damage path.
    {
        EventBus bus; GameClock clock; RNG rng(17);
        std::vector<GameEvent> casts;
        bus.subscribe(EventType::AbilityCast,
                      [&casts](const GameEvent& e) { casts.push_back(e); });
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        const uint64_t a = m.addBot(0);
        const uint64_t b = m.addBot(1);
        CHECK(m.setPlayerKit(a, "cthulhu_avatar"));
        CHECK(m.setPlayerKit(b, "cthulhu_avatar"));
        // The holder is human: no bot AI will swing or hurl the rock.
        const uint64_t h = m.joinAsHuman(0);
        CHECK(h == a);
        m.movePlayer(h, Vec3(0, 0, 0));
        m.movePlayer(b, Vec3(1, 0, 0)); // in h's frontal arc: shield blocks
        CHECK(m.setFacingYaw(h, 0.0f));

        // 1: rip through the public cast path, like `mcast rmb down`.
        CHECK(m.humanCast(h, 3, 0.0f, Match::RmbAim::Down));
        size_t ih = (m.player(0).id == h) ? 0 : 1;
        CHECK(m.player(ih).hasRock);
        casts.clear(); // ignore the rip's own "Sunder the Earth" cast

        // 2: the bot's blocked hits break the rock.
        int ticks = 0;
        while (m.player(ih).hasRock && ticks < 80) {
            m.update(0.25);
            ++ticks;
        }
        CHECK(ticks < 80); // the rock did break
        CHECK(!m.player(ih).hasRock);
        int shatters = 0;
        uint64_t shatterSrc = 0;
        for (const GameEvent& e : casts)
            if (e.tag == "the rock shatters") {
                ++shatters;
                shatterSrc = e.sourceId;
            }
        CHECK(shatters == 1);
        CHECK(shatterSrc == h);

        // 3: keep fighting — the break must not be announced twice.
        const size_t before = casts.size();
        for (int i = 0; i < 20; ++i) m.update(0.25);
        int extra = 0;
        for (size_t i = before; i < casts.size(); ++i)
            if (casts[i].tag == "the rock shatters" ||
                casts[i].tag == "the rock crumbles to dust")
                ++extra;
        CHECK(extra == 0);
    }

    // 4: the driver-side filter fires for break tags, ignores the rest.
    {
        GameEvent shatter(EventType::AbilityCast);
        shatter.tag = "the rock shatters";
        GameEvent crumble(EventType::AbilityCast);
        crumble.tag = "the rock crumbles to dust";
        GameEvent rip(EventType::AbilityCast);
        rip.tag = "Sunder the Earth"; // kRockRipTag — ordinary cast
        GameEvent hurl(EventType::AbilityCast);
        hurl.tag = "Hurl the Mountain";
        GameEvent q(EventType::AbilityCast);
        q.tag = "Pounce";
        CHECK(consoleFiresFor(shatter));
        CHECK(consoleFiresFor(crumble));
        CHECK(!consoleFiresFor(rip));
        CHECK(!consoleFiresFor(hurl));
        CHECK(!consoleFiresFor(q));
    }

    if (failures == 0) std::cout << "tests_micro_rockbreak: all green\n";
    return failures;
}
