// CULT-ULHU micro-dev regression tests: free-roam kill events stamp the
// corpse's position.
//
// Combat::dealDamage publishes the kill event plus MonstrositySlain (for
// monstrosity kills) and PlayerKilled (avatar-on-avatar PvP). All three
// are death beats in the FX pipeline (CivilianSlain / MonstrositySlain /
// PlayerKilled all resolve to the aoe_damage impact burst in fxForEvent),
// but the free-roam combat path never stamped pos — so a Dhole slain by a
// kit spell in the Vale of Pnath, or an avatar felled in free-roam PvP,
// burst with impact smoke at the world origin instead of on the corpse.
// The match-pipeline kill sites (Match::killPlayer) already stamp pos;
// this pins the free-roam path to the same contract:
//
// 1. Actual civilian deaths and species-tagged monster deaths carry the
//    corpse position, without awarding civilian kills for other entity types.
// 2. Avatar-vs-avatar kill: PlayerKilled carries the victim's position.
// 3. Full pipeline: both events resolve through fxForEvent to aoe_damage
//    (the visual the position stamp feeds).
// 4. Survivor (non-lethal hit): no kill events at all.

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
    std::string tag;
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
        seen.push_back({e.type, e.pos, e.targetId, e.tag});
    };

    {
        EventBus bus;
        seen.clear();
        bus.subscribe(EventType::CivilianSlain,capture);
        const Vec3 corpse{19.0f,0.0f,-5.0f};
        Entity civilian(EntityType::Civilian,FACTION_NEUTRAL,corpse,10.0f);
        CHECK(combat::dealDamage(civilian,9999.0f,bus,EventType::CivilianSlain,false,7,EntityType::EldritchAvatar));
        CHECK(countType(seen,EventType::CivilianSlain)==1);
        const Seen* slain=firstOf(seen,EventType::CivilianSlain);
        CHECK(slain&&slain->targetId==civilian.id()&&samePos(slain->pos,corpse));
    }

    // ---------- 1. Monster death: species event, never civilian rewards
    {
        EventBus bus;
        seen.clear();
        bus.subscribe(EventType::CivilianSlain, capture);
        bus.subscribe(EventType::MonstrositySlain, capture);
        bus.subscribe(EventType::PlayerKilled, capture);

        const Vec3 corpse{12.0f, 0.0f, -7.0f};
        Entity dhole(EntityType::Monstrosity, FACTION_NEUTRAL, corpse, 10.0f);
        const uint64_t id = dhole.id();
        const bool killed = combat::dealDamage(dhole, 9999.0f, bus,
                                              EventType::CivilianSlain, false,
                                              7, EntityType::EldritchAvatar);
        CHECK(killed);
        CHECK(countType(seen, EventType::CivilianSlain) == 0);
        CHECK(countType(seen, EventType::MonstrositySlain) == 1);
        CHECK(countType(seen, EventType::PlayerKilled) == 0);
        const Seen* mslain = firstOf(seen, EventType::MonstrositySlain);
        CHECK(mslain && mslain->targetId == id);
        CHECK(mslain && samePos(mslain->pos, corpse));
    }

    // ---------- 2. Avatar-vs-avatar: PlayerKilled stamped ----------
    {
        EventBus bus;
        seen.clear();
        bus.subscribe(EventType::CivilianSlain, capture);
        bus.subscribe(EventType::PlayerKilled, capture);

        const Vec3 corpse{3.0f, 0.0f, 5.0f};
        Entity victim(EntityType::EldritchAvatar, 1, corpse, 50.0f);
        const uint64_t vid = victim.id();
        const bool killed =
            combat::dealDamage(victim, 9999.0f, bus, EventType::CivilianSlain,
                               false, 4242, EntityType::EldritchAvatar);
        CHECK(killed);
        CHECK(countType(seen, EventType::PlayerKilled) == 1);
        const Seen* pk = firstOf(seen, EventType::PlayerKilled);
        CHECK(pk && pk->targetId == vid);
        CHECK(pk && samePos(pk->pos, corpse));
        CHECK(countType(seen,EventType::CivilianSlain)==0); // no false civilian reward
    }

    // ---------- 3. Full pipeline: stamped events resolve to aoe_damage --
    {
        const FxLoadResult fr =
            loadFxLibrary(repoPrefix() + "assets/fx/ability_fx.def");
        CHECK(fr.ok);
        if (fr.ok) {
            EventBus bus;
            seen.clear();
            bus.subscribe(EventType::MonstrositySlain, capture);
            bus.subscribe(EventType::PlayerKilled, capture);
            Entity dhole(EntityType::Monstrosity, FACTION_NEUTRAL,
                         Vec3{1.0f, 0.0f, 2.0f}, 10.0f);
            combat::dealDamage(dhole, 9999.0f, bus, EventType::CivilianSlain);
            Entity victim(EntityType::EldritchAvatar, 1, Vec3{4.0f, 0.0f, 6.0f},
                          50.0f);
            combat::dealDamage(victim, 9999.0f, bus, EventType::CivilianSlain,
                               false, 9, EntityType::EldritchAvatar);
            for (const auto& s : seen) {
                GameEvent e(s.type);
                e.pos = s.pos;
                e.tag = s.tag;
                const FxPreset* p = fxForEvent(e, fr.library);
                CHECK(p != nullptr);
                if (p) CHECK(p->id == "aoe_damage");
                CHECK(!samePos(s.pos, Vec3{0.0f, 0.0f, 0.0f}));
            }
            CHECK(seen.size() == 2);
        }
    }

    // ---------- 4. Survivor: no kill events ----------
    {
        EventBus bus;
        seen.clear();
        bus.subscribe(EventType::CivilianSlain, capture);
        bus.subscribe(EventType::MonstrositySlain, capture);
        bus.subscribe(EventType::PlayerKilled, capture);
        Entity tough(EntityType::Monstrosity, FACTION_NEUTRAL,
                     Vec3{9.0f, 0.0f, 9.0f}, 1000.0f);
        const bool killed =
            combat::dealDamage(tough, 1.0f, bus, EventType::CivilianSlain);
        CHECK(!killed);
        CHECK(seen.empty());
    }

    if (failures == 0) std::cout << "slaypos: all green\n";
    return failures == 0 ? 0 : 1;
}
