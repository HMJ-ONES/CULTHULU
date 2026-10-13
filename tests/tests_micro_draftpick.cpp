// CULT-ULHU micro-dev (2026-10-13): DOTGOO draft-pick prompt regression
// tests.
//
// DOTGOO hands the player one draft pick at level 1 and a second at
// level 3, but the driver never said the pick exists: `match status`
// showed a cryptic "(+1 pick)" and the level-3 crossing printed only
// "[level] X reached level 3". The driver now prompts the joined human
// at join time ("you have a draft pick — mpick <q|f>") and once when
// they cross level 3 ("you earned a draft pick — ..."). These pin the
// production contracts the prompts rely on (the driver strings are
// thin wrappers over exactly this state):
//
// 1. Join-time predicate: at joinAsHuman in moba the player holds
//    pendingPicks >= 1 with Q and F still locked (prompt fires); in
//    capture (Domination) pendingPicks == 0 (prompt stays silent).
// 2. Level-3 grant: production grantXp to level 3 publishes one
//    PlayerLeveledUp (amount == 3) and grants exactly one pending pick.
// 3. Multi-level jump (1 -> 7): one event (amount == 7) but the level-3
//    grant loop still fires exactly once — the driver's per-level
//    dedupe sees the crossing inside the jump.
// 4. Domination control: crossing level 3 grants no draft pick at all.
// 5. mpick path: draftAbility consumes the pick and unlocks the slot
//    (the command the prompt points at actually works).

#include "modes/Match.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "core/Events.h"

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

int main() {
    // ---------- 1: join-time predicate ----------
    {
        EventBus bus; GameClock clock; RNG rng(202613);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        m.addBot(0);
        const uint64_t human = m.joinAsHuman(0);
        CHECK(human != 0);
        const Match::Player* p = m.findPlayer(human);
        CHECK(p != nullptr);
        // The exact state the driver checks before printing the
        // join-time prompt: moba mode, pick banked, Q/F still locked.
        CHECK(m.modeName() == "moba");
        CHECK(p->pendingPicks >= 1);
        CHECK(!p->qUnlocked && !p->fUnlocked);
    }
    // ---------- 1b: Domination join predicate stays silent ----------
    {
        EventBus bus; GameClock clock; RNG rng(202614);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        m.addBot(0);
        const uint64_t human = m.joinAsHuman(0);
        CHECK(human != 0);
        const Match::Player* p = m.findPlayer(human);
        CHECK(p != nullptr);
        CHECK(m.modeName() == "capture");
        CHECK(p->pendingPicks == 0);
    }
    // ---------- 2: level-3 grant via production grantXp ----------
    {
        EventBus bus; GameClock clock; RNG rng(202615);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        m.addBot(0);
        const uint64_t human = m.joinAsHuman(0);
        int leveledUps = 0;
        float lastAmount = 0.0f;
        bus.subscribe(EventType::PlayerLeveledUp, [&](const GameEvent& e) {
            if (e.sourceId == human) {
                ++leveledUps;
                lastAmount = e.amount;
            }
        });
        CHECK(m.grantXp(human, 100)); // 1 + 100/50 = 3
        const Match::Player* p = m.findPlayer(human);
        CHECK(p->level == 3);
        CHECK(leveledUps == 1);
        CHECK(static_cast<int>(lastAmount) == 3);
        // Level-1 pick still banked (no draft yet) + the level-3 grant.
        CHECK(p->pendingPicks == 2);
    }
    // ---------- 3: multi-level jump still grants the level-3 pick ----------
    {
        EventBus bus; GameClock clock; RNG rng(202616);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        m.addBot(0);
        const uint64_t human = m.joinAsHuman(0);
        int leveledUps = 0;
        float lastAmount = 0.0f;
        bus.subscribe(EventType::PlayerLeveledUp, [&](const GameEvent& e) {
            if (e.sourceId == human) {
                ++leveledUps;
                lastAmount = e.amount;
            }
        });
        CHECK(m.grantXp(human, 300)); // 1 + 300/50 = 7
        const Match::Player* p = m.findPlayer(human);
        CHECK(p->level == 7);
        CHECK(leveledUps == 1); // one event per grantXp call
        CHECK(static_cast<int>(lastAmount) == 7);
        // The internal lv loop still crossed 3 -> exactly one extra pick.
        CHECK(p->pendingPicks == 2);
    }
    // ---------- 4: Domination control — no draft grants ----------
    {
        EventBus bus; GameClock clock; RNG rng(202617);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        m.addBot(0);
        const uint64_t human = m.joinAsHuman(0);
        CHECK(m.grantXp(human, 100)); // level 3
        const Match::Player* p = m.findPlayer(human);
        CHECK(p->level == 3);
        CHECK(p->pendingPicks == 0);
    }
    // ---------- 5: mpick consumes the pick and unlocks the slot ----------
    {
        EventBus bus; GameClock clock; RNG rng(202618);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        m.addBot(0);
        const uint64_t human = m.joinAsHuman(0);
        CHECK(m.draftAbility(human, 0)); // mpick q
        const Match::Player* p = m.findPlayer(human);
        CHECK(p->qUnlocked);
        CHECK(!p->fUnlocked);
        CHECK(p->pendingPicks == 0);
        CHECK(!m.draftAbility(human, 1)); // mpick f with no pick fails
    }

    if (failures == 0) std::cout << "draftpick: all green\n";
    return failures == 0 ? 0 : 1;
}
