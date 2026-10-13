// CULT-ULHU micro-dev regression tests: the rock swing publishes an FX
// event.
//
// Cthulhu's Domination rock kit publishes AbilityCast events for the rip
// ("Sunder the Earth"), the hurl ("Hurl the Mountain"), and the break
// ("the rock shatters" / "the rock crumbles to dust") — but a landed
// rock swing that left the rock intact published nothing, so the FX
// pipeline had no hook for the kit's signature melee attack while every
// sibling action did. swingRock now publishes AbilityCast (slot 3 = RMB)
// with kRockSwingTag when the rock survives the swing; the breaking swing
// is covered by breakRock's event instead, so one swing never fires two
// visuals. These pin:
//
// 1. Landed swing, rock intact: exactly one AbilityCast with the swing
//    tag, amount == 3, sourceId == the attacker's player id — and it
//    resolves through fxForEvent to the MeleeHeavy preset (a heavy
//    frontal cleave; the exact tag is checked before the break-prose
//    substring test, so it must not land on rock_shatter).
// 2. Whiffed swing (no enemy in the cone): silent — no swing event.
// 3. Breaking swing (durability spent): publishes the break tag, and no
//    swing event — one swing, one visual.
//
// MatchLogger deliberately keeps non-landmark RMB casts out of the
// exported story, so the swing event stays log-quiet by the existing
// filter (verified by construction: the logger's isRock check is an
// exact match on the four landmark tags).

#include "modes/Match.h"
#include "characters/AbilityFx.h"
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

struct CastEvent {
    uint64_t sourceId;
    std::string tag;
    float amount;
};

// Domination match, Cthulhu (team 0) with a ripped rock, one human enemy.
// Humans get no bot AI, so the event stream is fully deterministic.
struct RockDuel {
    EventBus bus;
    GameClock clock;
    RNG rng{11};
    Match m;
    uint64_t cthulhu = 0;
    uint64_t foe = 0;
    std::vector<CastEvent> casts;

    RockDuel() : m(bus, clock, rng) {
        CHECK(m.start("capture"));
        cthulhu = m.addPlayer("Cthulhu", 0);
        foe = m.addPlayer("Foe", 1);
        CHECK(m.setPlayerKit(cthulhu, "cthulhu_avatar"));
        bus.subscribe(EventType::AbilityCast, [&](const GameEvent& e) {
            casts.push_back({e.sourceId, e.tag, e.amount});
        });
        // Rip the rock (RMB aimed down).
        CHECK(m.humanCast(cthulhu, 3, 0.0f, Match::RmbAim::Down));
        casts.clear(); // the rip event is not under test
    }

    int countTag(const char* tag) const {
        int n = 0;
        for (const auto& c : casts)
            if (c.tag == tag) ++n;
        return n;
    }
};

int main() {
    // ---------- 1: landed swing publishes the swing event ----------
    {
        RockDuel d;
        // Foe 3m ahead in the frontal cone (yaw 0 faces +x).
        CHECK(d.m.movePlayer(d.cthulhu, Vec3(0, 0, 0)));
        CHECK(d.m.movePlayer(d.foe, Vec3(3, 0, 0)));
        CHECK(d.m.humanCast(d.cthulhu, 3, 0.0f, Match::RmbAim::Normal));
        CHECK(d.countTag(kRockSwingTag) == 1);
        for (const auto& c : d.casts) {
            if (c.tag != kRockSwingTag) continue;
            CHECK(c.sourceId == d.cthulhu);
            CHECK(c.amount == 3.0f);
        }
        // The published event resolves to the MeleeHeavy preset through
        // the production fxForEvent overload — the whole chain is live.
        FxLoadResult fr = loadFxLibrary("assets/fx/ability_fx.def");
        CHECK(fr.ok);
        GameEvent e(EventType::AbilityCast);
        e.tag = kRockSwingTag;
        e.amount = 3;
        const FxPreset* p = fxForEvent(e, fr.library);
        CHECK(p != nullptr);
        if (p) CHECK(p->id == "MeleeHeavy");
    }

    // ---------- 2: whiffed swing is silent ----------
    {
        RockDuel d;
        CHECK(d.m.movePlayer(d.cthulhu, Vec3(0, 0, 0)));
        CHECK(d.m.movePlayer(d.foe, Vec3(500, 0, 500))); // nobody near
        CHECK(!d.m.humanCast(d.cthulhu, 3, 0.0f, Match::RmbAim::Normal));
        CHECK(d.countTag(kRockSwingTag) == 0);
        CHECK(d.casts.empty());
    }

    // ---------- 3: breaking swing publishes the break, not the swing ---
    {
        RockDuel d;
        CHECK(d.m.movePlayer(d.cthulhu, Vec3(0, 0, 0)));
        CHECK(d.m.movePlayer(d.foe, Vec3(3, 0, 0)));
        // Fresh rock: 3.0 durability, one swing = one unit of wear. The
        // foe has 100 HP; three swings deal 3 x 30 = 90, so it survives
        // all three and every swing lands. swingCd ticks down between.
        CHECK(d.m.humanCast(d.cthulhu, 3, 0.0f, Match::RmbAim::Normal));
        d.m.update(1.1);
        CHECK(d.m.humanCast(d.cthulhu, 3, 0.0f, Match::RmbAim::Normal));
        d.m.update(1.1);
        CHECK(d.m.humanCast(d.cthulhu, 3, 0.0f, Match::RmbAim::Normal));
        CHECK(d.countTag(kRockSwingTag) == 2); // swings 1 and 2
        // The third swing spent the rock: breakRock's event instead.
        CHECK(d.countTag("the rock crumbles to dust") == 1);
        CHECK(d.countTag(kRockSwingTag) == 2); // still 2 — no double fire
    }

    if (failures == 0) std::cout << "rockswing: all green\n";
    return failures == 0 ? 0 : 1;
}
