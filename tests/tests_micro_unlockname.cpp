// CULT-ULHU micro-dev regression tests: AbilityUnlocked names the ability.
//
// The DOTGOO draft published AbilityUnlocked with a bare slot token
// ("Q"/"F"/"ULT") — the console [unlock] line and the exported match log
// never said WHAT was unlocked, and the driver's `mpick` printed a bare
// "unlocked q!". These pin:
//
// 1. draftAbility (human `mpick` path) publishes AbilityUnlocked with a
//    "Q (name)" tag carrying the kit's real Q ability name.
// 2. The level-6 ult publishes "ULT (name)" with the kit's R name.
// 3. Bot auto-draft publishes "Q (name)" tags through the production
//    update() loop.
// 4. Tag format contract: slot token, space, parenthesized non-empty name.
// 5. Control: a second draftAbility call (no picks) publishes nothing.

#include "modes/Match.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"

#include <iostream>
#include <sstream>
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

// Tag contract shared by every publish site.
static bool wellFormed(const std::string& tag, const std::string& slot) {
    const std::string prefix = slot + " (";
    if (tag.rfind(prefix, 0) != 0) return false;
    if (tag.size() <= prefix.size() + 1) return false; // empty name
    return tag.back() == ')';
}

int main() {
    // ---------- 1. human draftAbility names the Q ability ----------
    {
        EventBus bus; GameClock clock; RNG rng(501);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        uint64_t a = m.addBot(0); // humanCast/draftAbility work on any id
        std::vector<std::string> tags;
        bus.subscribe(EventType::AbilityUnlocked,
                      [&](const GameEvent& e) { tags.push_back(e.tag); });
        CHECK(m.draftAbility(a, 0)); // spend the level-1 pick on Q
        CHECK(tags.size() == 1);
        CHECK(wellFormed(tags[0], "Q"));
    }
    // ---------- 2. level-6 ult names the R ability ----------
    {
        EventBus bus; GameClock clock; RNG rng(502);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        uint64_t a = m.addBot(0);
        std::vector<std::string> tags;
        bus.subscribe(EventType::AbilityUnlocked,
                      [&](const GameEvent& e) { tags.push_back(e.tag); });
        // Level 6 needs 250 xp (level = 1 + xp/50).
        CHECK(m.grantXp(a, 250));
        const Match::Player* p = m.findPlayer(a);
        CHECK(p && p->level == 6);
        CHECK(p && p->rUnlocked);
        bool sawUlt = false;
        for (const auto& t : tags) {
            if (t.rfind("ULT", 0) == 0) {
                sawUlt = true;
                CHECK(wellFormed(t, "ULT"));
            }
        }
        CHECK(sawUlt);
    }
    // ---------- 3. bot auto-draft names abilities via update() ----------
    {
        EventBus bus; GameClock clock; RNG rng(503);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        m.botfill();
        std::vector<std::string> tags;
        bus.subscribe(EventType::AbilityUnlocked,
                      [&](const GameEvent& e) { tags.push_back(e.tag); });
        for (int t = 0; t < 30 && !m.isOver(); ++t) m.update(1.0);
        bool sawQ = false;
        for (const auto& t : tags) {
            if (t.rfind("Q", 0) == 0) {
                sawQ = true;
                CHECK(wellFormed(t, "Q"));
            }
        }
        CHECK(sawQ); // every bot spends its level-1 pick through the loop
    }
    // ---------- 5. failed draft publishes nothing ----------
    {
        EventBus bus; GameClock clock; RNG rng(504);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        uint64_t a = m.addBot(0);
        int count = 0;
        bus.subscribe(EventType::AbilityUnlocked,
                      [&](const GameEvent&) { ++count; });
        CHECK(m.draftAbility(a, 0));  // spends the pick, publishes once
        CHECK(count == 1);
        CHECK(!m.draftAbility(a, 0)); // no picks left: silent failure
        CHECK(count == 1);
        CHECK(!m.draftAbility(a, 1)); // still no picks: silent failure
        CHECK(count == 1);
    }

    // ---------- 6. unlock FX lands on the fighter, not the origin ----
    // Micro-dev (2026-10-13): AbilityUnlocked resolves to the buff preset
    // in fxForEvent, but the three publish sites never stamped pos — the
    // halo would have played at the world origin. Pin the position
    // contract on the human draft path.
    {
        EventBus bus; GameClock clock; RNG rng(505);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        uint64_t a = m.addBot(0);
        const Match::Player* pp = m.findPlayer(a);
        CHECK(pp != nullptr);
        std::vector<GameEvent> events;
        bus.subscribe(EventType::AbilityUnlocked,
                      [&](const GameEvent& e) { events.push_back(e); });
        CHECK(m.draftAbility(a, 0));
        CHECK(events.size() == 1);
        CHECK(events[0].pos.x == pp->pos.x);
        CHECK(events[0].pos.y == pp->pos.y);
        CHECK(events[0].pos.z == pp->pos.z);
    }

    if (failures == 0) std::cout << "ALL TESTS PASSED\n";
    return failures == 0 ? 0 : 1;
}
