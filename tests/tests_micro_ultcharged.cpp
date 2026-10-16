// CULT-ULHU micro-dev regression tests: Domination "ult ready" ping.
//
// Domination ults start at 0% charge on a full 40–60s cooldown — the one
// cooldown a player actually waits on — but nothing announced the moment
// the opening charge completed; you had to poll `match status`. The tick
// now publishes EventType::UltCharged once per player per match, which
// gets a console callout (main.cpp) and a ULTREADY line in the exported
// match log. These pin:
//
// 1. Domination: both fighters' opening charges fire exactly one
//    UltCharged each — sourceId is the player, tag names the ult.
// 2. No re-fire: after the ping, another 60s of play (the bot casts its
//    ult when it can, re-charging rCd) produces no second event.
// 3. DOTGOO: 60s of MOBA produce no UltCharged — MOBA ults arrive via
//    the level-6 draft (AbilityUnlocked), never via a charge.
// 4. Production path: MatchLogger records one ULTREADY line per fighter.
// 5. setPlayerKit (which zeroes cooldowns today): no phantom ping on the
//    following ticks — the event only fires on a real charge transition.

#include "modes/Match.h"
#include "modes/MatchLogger.h"
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

struct UltHit {
    uint64_t id;
    std::string tag;
    int faction;
};

static void tickMatch(Match& m, int seconds) {
    for (int i = 0; i < seconds && !m.isOver(); ++i) m.update(1.0);
}

int main() {
    // ---------- 1+2+4: Domination production path ----------
    {
        EventBus bus; GameClock clock; RNG rng(12);
        std::vector<UltHit> hits;
        bus.subscribe(EventType::UltCharged, [&hits](const GameEvent& e) {
            hits.push_back({e.sourceId, e.tag, e.faction});
        });

        Match m(bus, clock, rng);
        MatchLogger logger;
        CHECK(m.start("capture"));
        // No setPlayerKit: the normal add path arms the opening charge via
        // initAbilityUnlocks (round-robin kits, whatever they are).
        const uint64_t a = m.addBot(0);
        const uint64_t b = m.addBot(1);
        m.movePlayer(a, Vec3(0, 0, 0));
        m.movePlayer(b, Vec3(1, 0, 0));
        logger.attach(m);

        // 1. Both opening charges complete within the 60s ult budget.
        tickMatch(m, 75);
        CHECK(!m.isOver());
        int hitsA = 0, hitsB = 0;
        for (const auto& h : hits) {
            CHECK(!h.tag.empty()); // the ping names the ult
            if (h.id == a) { ++hitsA; CHECK(h.faction == 0); }
            else if (h.id == b) { ++hitsB; CHECK(h.faction == 1); }
            else CHECK(false); // no stranger may fire
        }
        CHECK(hitsA == 1);
        CHECK(hitsB == 1);

        // 4. The exported story carries both charges coming online.
        {
            int n = 0;
            for (const auto& e : logger.events()) {
                if (e.kind == "ULTREADY") {
                    ++n;
                    CHECK(e.text.find("charged") != std::string::npos);
                }
            }
            CHECK(n == 2);
        }

        // 2. No re-fire: another 60s (bots cast the ult when an enemy is
        // near, re-charging rCd) stays silent.
        tickMatch(m, 60);
        CHECK(hits.size() == 2);
    }

    // ---------- 3: DOTGOO stays silent ----------
    {
        EventBus bus; GameClock clock; RNG rng(12);
        int hits = 0;
        bus.subscribe(EventType::UltCharged,
                      [&hits](const GameEvent&) { ++hits; });

        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        const uint64_t a = m.addBot(0);
        const uint64_t b = m.addBot(1);
        (void)a; (void)b;
        tickMatch(m, 60);
        CHECK(!m.isOver());
        CHECK(hits == 0);
    }

    // ---------- 5: setPlayerKit zeroes rCd — no phantom ping ----------
    {
        EventBus bus; GameClock clock; RNG rng(12);
        int hits = 0;
        bus.subscribe(EventType::UltCharged,
                      [&hits](const GameEvent&) { ++hits; });

        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        const uint64_t a = m.addBot(0);
        CHECK(m.setPlayerKit(a, "cthulhu_avatar")); // rCd -> 0, not charging
        tickMatch(m, 10);
        CHECK(hits == 0);
    }

    if (failures == 0) std::cout << "tests_micro_ultcharged: ALL GREEN\n";
    return failures == 0 ? 0 : 1;
}
