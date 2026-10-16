// CULT-ULHU micro-dev regression tests: DOTGOO level-6 ult callout.
//
// The level-6 ult is DOTGOO's biggest early spike, but the human only saw
// a small "[unlock] You unlocked ULT (<name>)" line — easy to miss in a
// fight. The driver now prints an arena callout ("*** ULT UNLOCKED — <name>
// is ready! (mcast r) ***") gated on the human's own unlock with a tag
// starting "ULT". These pin the production contract that subscriber
// depends on:
//
// 1. Level-6 crossing publishes exactly one AbilityUnlocked whose tag
//    starts "ULT" (well-formed "ULT (<name>)") with sourceId = the player.
// 2. An XP jump from 1 to 7 still publishes exactly one ult unlock (the
//    oldLevel+1..level loop must not double-fire).
// 3. Domination: leveling publishes no AbilityUnlocked at all (moba-only).
// 4. Draft picks publish "Q ("/"F (" tags, never "ULT" — the callout
//    predicate is ult-only, so the level-1/3 prompts stay single-voice.

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

struct UnlockHit {
    uint64_t id;
    std::string tag;
};

// Tag contract: "<SLOT> (<name>)" with a non-empty name.
static bool wellFormed(const std::string& tag, const std::string& slot) {
    const std::string prefix = slot + " (";
    if (tag.rfind(prefix, 0) != 0) return false;
    if (tag.size() <= prefix.size() + 1) return false; // empty name
    return tag.back() == ')';
}

int main() {
    // ---------- 1. level-6 crossing: exactly one "ULT (...)" unlock ----------
    {
        EventBus bus; GameClock clock; RNG rng(601);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        uint64_t a = m.addBot(0);
        std::vector<UnlockHit> hits;
        bus.subscribe(EventType::AbilityUnlocked,
                      [&](const GameEvent& e) {
                          hits.push_back({e.sourceId, e.tag});
                      });
        // Level 6 needs 250 xp (level = 1 + xp/50).
        CHECK(m.grantXp(a, 250));
        const Match::Player* p = m.findPlayer(a);
        CHECK(p && p->level == 6);
        CHECK(p && p->rUnlocked);
        CHECK(hits.size() == 1);
        if (!hits.empty()) {
            CHECK(hits[0].id == a);
            CHECK(wellFormed(hits[0].tag, "ULT"));
        }
    }
    // ---------- 2. XP jump 1 -> 7: still exactly one ult unlock ----------
    {
        EventBus bus; GameClock clock; RNG rng(602);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        uint64_t a = m.addBot(0);
        int ultCount = 0;
        bus.subscribe(EventType::AbilityUnlocked,
                      [&](const GameEvent& e) {
                          if (e.tag.rfind("ULT", 0) == 0) ++ultCount;
                      });
        CHECK(m.grantXp(a, 300)); // lands on level 7
        const Match::Player* p = m.findPlayer(a);
        CHECK(p && p->level == 7);
        CHECK(p && p->rUnlocked);
        CHECK(ultCount == 1);
    }
    // ---------- 3. Domination: no AbilityUnlocked on level-ups ----------
    {
        EventBus bus; GameClock clock; RNG rng(603);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t a = m.addBot(0);
        int count = 0;
        bus.subscribe(EventType::AbilityUnlocked,
                      [&](const GameEvent&) { ++count; });
        CHECK(m.grantXp(a, 300));
        CHECK(count == 0);
    }
    // ---------- 4. draft picks are not "ULT" — callout stays ult-only ----------
    {
        EventBus bus; GameClock clock; RNG rng(604);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        uint64_t a = m.addBot(0);
        std::vector<std::string> tags;
        bus.subscribe(EventType::AbilityUnlocked,
                      [&](const GameEvent& e) { tags.push_back(e.tag); });
        CHECK(m.draftAbility(a, 0)); // spend the level-1 pick on Q
        CHECK(tags.size() == 1);
        CHECK(tags[0].rfind("ULT", 0) != 0);
        CHECK(wellFormed(tags[0], "Q"));
    }

    if (failures == 0) std::cout << "ALL TESTS PASSED\n";
    return failures == 0 ? 0 : 1;
}
