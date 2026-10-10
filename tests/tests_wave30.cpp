// CULT-ULHU wave 30 tests: civilian-rumor plumbing (codex idFor),
// player-directed rituals.

#include "ai/RitualCaster.h"
#include "beliefs/BeliefSystem.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "cult/CultManager.h"
#include "discovery/DiscoveryCodex.h"
#include "entities/Units.h"
#include "exertion/ExertionSystem.h"
#include "power/PowerSystem.h"

#include <iostream>
#include <string>

using namespace cultulhu;

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

static void testCodexIdFor() {
    // The rumor system checks "already discovered?" via stable ids.
    CHECK(DiscoveryCodex::idFor(DiscoveryKind::Landmark, "The Shattered Court") ==
          "landmark:the_shattered_court");
    CHECK(DiscoveryCodex::idFor(DiscoveryKind::Species, "pale_wight") ==
          "species:pale_wight");

    EventBus bus;
    DiscoveryCodex codex{bus};
    const std::string id = DiscoveryCodex::idFor(DiscoveryKind::Landmark,
                                                 "The Shattered Court");
    CHECK(codex.find(id) == nullptr);
    codex.discover(DiscoveryKind::Landmark, "The Shattered Court",
                   "The Shattered Court", "Stones kneel.", Vec3{}, 1.0, false);
    CHECK(codex.find(id) != nullptr);
}

static void testDirectedRitual() {
    EventBus bus;
    GameClock clock;
    RNG rng{77};
    BeliefSystem beliefs{bus, clock};
    PowerSystem power;
    CultManager cult{bus, clock, rng};
    ExertionSystem exertion{bus, beliefs, power, cult, rng};
    RitualCaster rc{bus, rng, beliefs, exertion, 1.0}; // fast interval

    CHECK(!rc.hasDirectedPoint());
    rc.setDirectedPoint(Vec3{100, 0, 100});
    CHECK(rc.hasDirectedPoint());
    rc.clearDirectedPoint();
    CHECK(!rc.hasDirectedPoint());

    // Directed point expires after 120s without firing.
    rc.setDirectedPoint(Vec3{0, 0, 0});
    rc.update(121.0);
    CHECK(!rc.hasDirectedPoint());
}

static void testDirectedRitualAims() {
    EventBus bus;
    GameClock clock;
    RNG rng{78};
    BeliefSystem beliefs{bus, clock};
    PowerSystem power;
    CultManager cult{bus, clock, rng};
    ExertionSystem exertion{bus, beliefs, power, cult, rng};
    // Long interval; we drive single ticks manually.
    RitualCaster rc{bus, rng, beliefs, exertion, 1000.0};

    Sorcerer sorc{FACTION_CTHULHU, Vec3{0, 0, 0}};
    Civilian near{Vec3{5, 0, 0}};
    Civilian far{Vec3{40, 0, 0}};
    // Sorcerer needs mana; check the API exists and the directed point
    // is accepted without crashing when no ritual fires.
    rc.setSorcerers({&sorc});
    rc.setCivilians({&near, &far});
    rc.setDirectedPoint(Vec3{40, 0, 0}); // aim at the far civilian
    rc.update(0.5); // timer < interval: no ritual, point persists
    CHECK(rc.hasDirectedPoint());
}

int main() {
    std::cout << "== wave30: rumors & directed rituals ==\n";
    testCodexIdFor();
    testDirectedRitual();
    testDirectedRitualAims();
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
