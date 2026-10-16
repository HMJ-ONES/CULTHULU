// CULT-ULHU micro-dev regression tests: dismissed cultists are untracked
// from the AnimationDirector.
//
// The director tracks live entities by RAW pointer (AnimationDirector::track).
// Cultists live in CultManager's vector<unique_ptr<Cultist>> — heap-stable,
// but dismissDead() DESTROYS the heap object on erase. A revolt kills and
// dismisses deserters inside CultManager::update() — which the driver calls
// AFTER animDirector.tick() in the same tick — so without an untrack the
// director's next tick() dereferences a destroyed cultist (use-after-free
// read in the !e->alive() prune check).
//
// dismissDead() now takes an optional per-erased-id callback; the driver
// passes one that untracks the id from the director. Ownership is explicit:
// a caller that dismisses without the callback keeps the stale entry (the
// SimWorld path has no director, so it needs none).
//
// Pinned behavior:
// 1. Production order (revolt): track -> kill + dismiss with the driver's
//    untrack callback between director ticks -> director holds nothing and
//    the next tick() is safe.
// 2. The callback fires once per erased cultist with the right id; live
//    cultists never fire it.
// 3. dismissDead() with no callback still compiles/works (SimWorld path) but
//    leaves tracking to the caller — the stale entry is observable.
// 4. untrack() of an unknown id is harmless.

#include "animation/AnimationDirector.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "cult/CultManager.h"

#include <cstdint>
#include <iostream>
#include <vector>

using namespace cultulhu;

static int failures = 0;
#define CHECK(cond)                                        \
    do {                                                   \
        if (!(cond)) {                                     \
            std::cout << "FAIL line " << __LINE__ << ": "  \
                      << #cond << "\n";                    \
            ++failures;                                    \
        }                                                  \
    } while (0)

int main() {
    // 1: the production order — a revolt kills + dismisses between ticks.
    {
        EventBus bus;
        GameClock clock;
        RNG rng{7};
        CultManager cult{bus, clock, rng};
        AnimationDirector director{bus};

        Cultist& c = cult.recruit();
        const uint64_t id = c.id();
        director.track(c);
        CHECK(director.tracked() == 1);

        // Revolt-style: dead and dismissed before the next director tick.
        c.takeDamage(c.hp() + 1.0f);
        std::vector<uint64_t> erased;
        cult.dismissDead([&](uint64_t e) {
            erased.push_back(e);
            director.untrack(e); // what the driver passes
        });
        CHECK(cult.size() == 0);
        CHECK(erased.size() == 1);
        CHECK(erased[0] == id);
        CHECK(director.tracked() == 0);
        director.tick(1.0); // must not touch the destroyed cultist
        CHECK(director.tracked() == 0);
    }

    // 2: only the erased fire the callback; the living stay tracked.
    {
        EventBus bus;
        GameClock clock;
        RNG rng{7};
        CultManager cult{bus, clock, rng};
        AnimationDirector director{bus};

        Cultist& a = cult.recruit();
        Cultist& b = cult.recruit();
        Cultist& live = cult.recruit();
        director.track(a);
        director.track(b);
        director.track(live);
        const uint64_t liveId = live.id();
        a.takeDamage(a.hp() + 1.0f);
        b.takeDamage(b.hp() + 1.0f);

        std::vector<uint64_t> erased;
        cult.dismissDead([&](uint64_t e) {
            erased.push_back(e);
            director.untrack(e);
        });
        CHECK(erased.size() == 2);
        CHECK(cult.size() == 1);
        CHECK(director.tracked() == 1);
        // The survivor is still the tracked one.
        CHECK(director.requestState(liveId, AnimationState::Idle, 0.0));
        director.tick(1.0);
        CHECK(director.tracked() == 1);
    }

    // 3: no-callback dismiss (SimWorld shape) keeps the entry — the caller
    // owns untracking. Do NOT tick the director here: the entry dangles,
    // which is exactly why the driver always passes the callback.
    {
        EventBus bus;
        GameClock clock;
        RNG rng{7};
        CultManager cult{bus, clock, rng};
        AnimationDirector director{bus};

        Cultist& c = cult.recruit();
        director.track(c);
        c.takeDamage(c.hp() + 1.0f);
        cult.dismissDead(); // no callback: entry stays, caller must untrack
        CHECK(cult.size() == 0);
        CHECK(director.tracked() == 1);
    }

    // 4: untracking an unknown id is a no-op.
    {
        EventBus bus;
        AnimationDirector director{bus};
        director.untrack(0xDEADBEEFull);
        CHECK(director.tracked() == 0);
    }

    if (failures == 0) std::cout << "tests_micro_animuntrack: all green\n";
    return failures == 0 ? 0 : 1;
}
