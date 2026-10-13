// CULT-ULHU micro-dev regression tests: DOTGOO "Great Old One under
// attack" warning.
//
// DOTGOO teams learned their Great Old One was taking damage only when it
// died — there was no "your god is under attack" callout at all. MobaDefense
// now publishes EventType::GreatOldOneUnderAttack on the first scratch and
// each 75/50/25% HP crossing (one warning per hit; the killing blow stays
// silent because GreatOldOneSlain is its announcement), which gets a console
// callout (main.cpp, urgent read for the defending player's team), a
// GOOATTACK line in the exported match log (MatchLogger), and the stun FX
// preset. These pin:
//
// 1. First scratch: exactly one event with faction = defending team,
//    sourceId = attacker id, amount = hp fraction after the hit,
//    pos = base position.
// 2. Chip damage that crosses no threshold stays silent.
// 3. Crossing 75% publishes once with the post-hit fraction; one hit that
//    spans several thresholds warns only once (breaks on first crossing).
// 4. The killing blow publishes NO warning — GreatOldOneSlain fires instead.
// 5. The untouched team's GOO produces no warnings.
// 6. MatchLogger: a GreatOldOneUnderAttack on the bus lands a GOOATTACK line
//    carrying the team, the HP %, and the attacker attribution.
// 7. fxForEvent(GreatOldOneUnderAttack) resolves to the existing "stun"
//    preset (no new FX asset; within the particle budget).

#include "modes/MobaDefense.h"
#include "modes/Match.h"
#include "modes/MatchLogger.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "core/Events.h"
#include "characters/AbilityFx.h"

#include <cmath>
#include <fstream>
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

static bool near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

// damageBase is protected; the tests need the production path, not a copy.
class TestMoba : public MobaDefense {
  public:
    using MobaDefense::MobaDefense;
    using MobaDefense::damageBase;
};

static std::string repoPrefix() {
    std::ifstream p("assets/fx/ability_fx.def");
    if (p) return "";
    return "../";
}

int main() {
    // ---------- 1-5: production loop contract via the real damageBase ----------
    {
        EventBus bus; GameClock clock; RNG rng(77);
        TestMoba moba(bus, clock, rng);
        // No lanes built: laneOpenFor() is true, so the GOO takes damage.
        moba.setBase(0, Vec3(-150, 0, 0), 1500.0f);
        moba.setBase(1, Vec3(150, 0, 0), 1500.0f);

        int warns = 0, slain = 0;
        GameEvent last;
        bus.subscribe(EventType::GreatOldOneUnderAttack,
                      [&](const GameEvent& e) { ++warns; last = e; });
        bus.subscribe(EventType::GreatOldOneSlain,
                      [&](const GameEvent&) { ++slain; });

        // 1. First scratch: one warning with full payload.
        moba.damageBase(1, 10.0f, 4242);
        CHECK(warns == 1);
        CHECK(last.faction == 1);
        CHECK(last.sourceId == 4242);
        CHECK(near(static_cast<float>(last.amount), 1490.0f / 1500.0f));
        CHECK(last.pos.x == 150.0f && last.pos.y == 0.0f && last.pos.z == 0.0f);

        // 2. Chip damage crossing no threshold: silent.
        moba.damageBase(1, 100.0f, 4242); // 1390/1500 = 0.9267
        CHECK(warns == 1);
        CHECK(slain == 0);

        // 3. Crossing 75%: one warning, post-hit fraction.
        moba.damageBase(1, 400.0f, 4242); // 990/1500 = 0.66
        CHECK(warns == 2);
        CHECK(last.faction == 1);
        CHECK(near(static_cast<float>(last.amount), 990.0f / 1500.0f));

        // One hit spanning 50% and 25% warns only once (first crossing).
        moba.damageBase(1, 500.0f, 4242); // 490/1500 = 0.3267
        CHECK(warns == 3);
        CHECK(near(static_cast<float>(last.amount), 490.0f / 1500.0f));

        // 4. Killing blow: no warning — GreatOldOneSlain is the announcement.
        moba.damageBase(1, 5000.0f, 4242);
        CHECK(warns == 3);
        CHECK(slain == 1);

        // Hitting the corpse: nothing at all.
        moba.damageBase(1, 10.0f, 4242);
        CHECK(warns == 3);
        CHECK(slain == 1);

        // 5. Team 0's GOO was never touched: no warnings for it.
        // (All three warnings above carry faction == 1.)
        CHECK(last.faction == 1);
    }

    // ---------- 6: MatchLogger line ----------
    {
        EventBus bus; GameClock clock; RNG rng(78);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        MatchLogger logger;
        logger.attach(m);

        GameEvent e(EventType::GreatOldOneUnderAttack);
        e.faction = 1;
        e.amount = 0.62;
        e.sourceId = 0; // minions did it — no player to attribute
        bus.publish(e);

        int lines = 0;
        for (const auto& entry : logger.events()) {
            if (entry.kind == "GOOATTACK") {
                ++lines;
                CHECK(entry.text.find("t1") != std::string::npos);
                CHECK(entry.text.find("62%") != std::string::npos);
                CHECK(entry.text.find("minions") != std::string::npos);
            }
        }
        CHECK(lines == 1);
    }

    // ---------- 7: FX resolves to the stun preset ----------
    {
        FxLoadResult r =
            loadFxLibrary(repoPrefix() + "assets/fx/ability_fx.def");
        CHECK(r.ok);
        if (r.ok) {
            const FxPreset* viaEvent =
                fxForEvent(EventType::GreatOldOneUnderAttack, r.library);
            const FxPreset* stun = r.library.find("stun");
            CHECK(stun != nullptr);
            CHECK(viaEvent == stun);
        }
    }

    if (failures == 0) std::cout << "ALL GOOUNDERATTACK CHECKS PASSED\n";
    return failures ? 1 : 0;
}
