// CULT-ULHU micro-dev regression tests: MaulStruck gets FX, and the Vale
// relic claim gets the triumph halo.
//
// Combat::dealDamage publishes MaulStruck whenever a monstrosity/feral beast
// mauls a human (civilian, cultist, adventurer) — consumed by the
// AnimationDirector — but fxForEvent() returned nullptr for it, so the
// engine played no visual for the maul while every other violence beat
// (kills, ambushes, feral rampages) bursts with impact smoke. This pins:
//   1. MaulStruck carries the victim's position (same pos-stamp contract
//      as the kill beats).
//   2. Full pipeline: MaulStruck resolves through fxForEvent() to the
//      aoe_damage preset against the real ability_fx.def.
//
// ValeRelicClaimed (ValeOfPnath::claimRelic) likewise returned nullptr —
// the dungeon's signature treasure moment had no visual while its cousins
// (DungeonCompleted, DiscoveryMade, AchievementUnlocked) all get the
// golden triumph halo. The publish site already stamps pos at the relic
// spot, so this pins:
//   3. ValeRelicClaimed resolves through fxForEvent() to the buff preset.

#include "combat/Combat.h"
#include "characters/AbilityFx.h"
#include "core/EventBus.h"
#include "entities/Entity.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace cultulhu;

// The test runs with WORKING_DIRECTORY = repo root (or build/); probe.
static std::string repoPrefix() {
    std::ifstream p("assets/fx/ability_fx.def");
    if (p) return "";
    return "../";
}

static int failures = 0;
#define CHECK(cond)                                        \
    do {                                                   \
        if (!(cond)) {                                     \
            std::cout << "FAIL line " << __LINE__ << ": "  \
                      << #cond << "\n";                     \
            ++failures;                                    \
        }                                                  \
    } while (0)

struct Seen {
    EventType type;
    Vec3 pos;
    uint64_t targetId;
};

static bool samePos(const Vec3& a, const Vec3& b) {
    return std::fabs(a.x - b.x) < 1e-6f && std::fabs(a.y - b.y) < 1e-6f &&
           std::fabs(a.z - b.z) < 1e-6f;
}

static int countType(const std::vector<Seen>& v, EventType t) {
    int n = 0;
    for (const auto& s : v)
        if (s.type == t) ++n;
    return n;
}

static const Seen* firstOf(const std::vector<Seen>& v, EventType t) {
    for (const auto& s : v)
        if (s.type == t) return &s;
    return nullptr;
}

int main() {
    std::vector<Seen> seen;
    auto capture = [&](const GameEvent& e) {
        seen.push_back({e.type, e.pos, e.targetId});
    };

    // ---------- 1. MaulStruck: beast mauls a civilian, pos stamped ----
    {
        EventBus bus;
        seen.clear();
        bus.subscribe(EventType::MaulStruck, capture);

        const Vec3 prey{9.0f, 0.0f, -4.0f};
        Entity civ(EntityType::Civilian, FACTION_NEUTRAL, prey, 30.0f);
        const uint64_t cid = civ.id();
        // Non-lethal maul: attacker is a Monstrosity, target a Civilian —
        // the maul branch fires regardless of the kill.
        combat::dealDamage(civ, 5.0f, bus, EventType::CivilianSlain, false,
                           99, EntityType::Monstrosity);
        CHECK(countType(seen, EventType::MaulStruck) == 1);
        const Seen* maul = firstOf(seen, EventType::MaulStruck);
        CHECK(maul && maul->targetId == cid);
        CHECK(maul && samePos(maul->pos, prey));
    }

    // ---------- 2 & 3. Full pipeline against the real fx library ------
    {
        const FxLoadResult fr =
            loadFxLibrary(repoPrefix() + "assets/fx/ability_fx.def");
        CHECK(fr.ok);
        if (fr.ok) {
            const FxPreset* maulFx = fxForEvent(EventType::MaulStruck, fr.library);
            CHECK(maulFx && maulFx->id == "aoe_damage");
            const FxPreset* relicFx = fxForEvent(EventType::ValeRelicClaimed, fr.library);
            CHECK(relicFx && relicFx->id == "buff");
        }
    }

    if (failures == 0) std::cout << "ALL OK\n";
    return failures == 0 ? 0 : 1;
}
