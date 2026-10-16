// CULT-ULHU micro-dev regression tests: DOTGOO "tower under attack" warning.
//
// DOTGOO towers started taking damage silently — the console jumped from
// lane pushing straight to TOWER DOWN, so nobody had a "defend your tower"
// beat before a lane defense crumbled. MobaDefense now publishes
// EventType::TowerUnderAttack on the FIRST scratch only (towers never heal,
// so pre-hit hp == maxHp identifies the first hit by construction; the
// killing blow skips the warning — TowerDestroyed is its announcement),
// which gets a console callout (main.cpp, urgent read for the defending
// team's human), a TOWERATTACK line in the exported match log
// (MatchLogger), and the stun FX preset. Both damage paths — the player
// swing path and the minion grind path — funnel through damageTower so the
// warning can't be skipped by one path or double-fire across paths.
// These pin:
//
// 1. Player path: first scratch publishes exactly one TowerUnderAttack with
//    the tower's team, lane name ("mid" for lane 1), the post-hit hp
//    fraction (240/250), the attacker's id, and the tower's position.
// 2. Chip damage after the first scratch stays silent (once per tower).
// 3. The killing blow publishes NO extra warning — TowerDestroyed fires
//    instead (and the playerHitStructuresCredit destruction contract from
//    the wave-34 XP work still holds).
// 4. Minion path: a minion grinding a tower down in the update loop warns
//    once on the first tick, then stays silent on later ticks.
// 5. MatchLogger: a TowerUnderAttack on the bus lands a TOWERATTACK line
//    carrying the team, lane name, HP %, and the attacker attribution.
// 6. fxForEvent(TowerUnderAttack) resolves to the existing "stun" preset
//    (no new FX asset; within the particle budget).

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

static std::string repoPrefix() {
    std::ifstream p("assets/fx/ability_fx.def");
    if (p) return "";
    return "../";
}

int main() {
    // ---------- 1-3: player swing path ----------
    {
        EventBus bus; GameClock clock; RNG rng(31);
        MobaDefense moba(bus, clock, rng);
        moba.setupDefaultMap();
        moba.update(0.1); // towers auto-build: 3 lanes x 2 teams x 2

        int warns = 0, downs = 0;
        GameEvent last;
        bus.subscribe(EventType::TowerUnderAttack,
                      [&](const GameEvent& e) { ++warns; last = e; });
        bus.subscribe(EventType::TowerDestroyed,
                      [&](const GameEvent&) { ++downs; });

        // Team-1 mid tower (lane 1), point-blank swing from player 4242.
        Vec3 tp = moba.towerPos(1, 1, 0);
        const float maxHp = moba.towerHp(1, 1, 0);
        CHECK(!moba.playerHitStructuresCredit(0, tp, 20.0f, 10.0f, 4242));

        // 1. First scratch: one warning with the full payload.
        CHECK(warns == 1);
        CHECK(downs == 0);
        CHECK(last.sourceId == 4242);
        CHECK(last.faction == 1);          // the defending tower's team
        CHECK(last.tag == "mid");          // lane name for lane 1
        CHECK(near(static_cast<float>(last.amount), (maxHp - 10.0f) / maxHp));
        CHECK(last.pos.x == tp.x && last.pos.y == tp.y && last.pos.z == tp.z);

        // 2. Chip damage that isn't the first scratch: silent.
        CHECK(!moba.playerHitStructuresCredit(0, tp, 20.0f, 10.0f, 4242));
        CHECK(warns == 1);
        CHECK(downs == 0);

        // 3. The killing blow: no extra warning — TowerDestroyed is the
        // announcement (and the destruction return contract still holds).
        CHECK(moba.playerHitStructuresCredit(0, tp, 20.0f, 99999.0f, 4242));
        CHECK(warns == 1);
        CHECK(downs == 1);

        // Hitting the corpse: nothing at all (range 5m so the pristine
        // lane-mate tower ~8m away is untouched — the old towerdown test
        // pins the same boundary).
        CHECK(!moba.playerHitStructuresCredit(0, tp, 5.0f, 10.0f, 4242));
        CHECK(warns == 1);
        CHECK(downs == 1);
    }

    // ---------- 4: minion grind path ----------
    {
        EventBus bus; GameClock clock; RNG rng(32);
        MobaDefense moba(bus, clock, rng);
        moba.setupDefaultMap();
        moba.update(0.1);

        int warns = 0, downs = 0;
        GameEvent last;
        bus.subscribe(EventType::TowerUnderAttack,
                      [&](const GameEvent& e) { ++warns; last = e; });
        bus.subscribe(EventType::TowerDestroyed,
                      [&](const GameEvent&) { ++downs; });

        // Team-0 minion parked on team-1's top-lane tower, gentle nibbling
        // (small dps so the first scratch and the kill land on different
        // ticks and the crossing contract is visible).
        moba.spawnWave(0, 0);
        CHECK(!moba.minions().empty());
        MobaDefense::Minion& m = moba.minions().front();
        m.pos = moba.towerPos(1, 0, 0);
        m.hp = m.maxHp = 1e9f; // towers shoot back; stay standing
        m.dps = 1.0f;
        m.range = 5.0f; // reach only the tower at this spot, not its lane-mate

        // First tick: first scratch warns once (faction = defending team,
        // lane name "top", attacker = the minion).
        moba.update(1.0);
        CHECK(warns == 1);
        CHECK(downs == 0);
        CHECK(last.faction == 1);
        CHECK(last.tag == "top");
        CHECK(last.sourceId == m.id);

        // Second tick: chip damage, no new warning.
        moba.update(1.0);
        CHECK(warns == 1);
        CHECK(downs == 0);
    }

    // ---------- 5: MatchLogger line ----------
    {
        EventBus bus; GameClock clock; RNG rng(33);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        MatchLogger logger;
        logger.attach(m);

        GameEvent e(EventType::TowerUnderAttack);
        e.faction = 1;
        e.amount = 0.96;
        e.tag = "mid";
        e.sourceId = 0; // minions did it — no player to attribute
        bus.publish(e);

        int lines = 0;
        for (const auto& entry : logger.events()) {
            if (entry.kind == "TOWERATTACK") {
                ++lines;
                CHECK(entry.text.find("t1") != std::string::npos);
                CHECK(entry.text.find("mid") != std::string::npos);
                CHECK(entry.text.find("96%") != std::string::npos);
                CHECK(entry.text.find("minions") != std::string::npos);
            }
        }
        CHECK(lines == 1);
    }

    // ---------- 6: FX resolves to the stun preset ----------
    {
        FxLoadResult r =
            loadFxLibrary(repoPrefix() + "assets/fx/ability_fx.def");
        CHECK(r.ok);
        if (r.ok) {
            const FxPreset* viaEvent =
                fxForEvent(EventType::TowerUnderAttack, r.library);
            const FxPreset* stun = r.library.find("stun");
            CHECK(stun != nullptr);
            CHECK(viaEvent == stun);
        }
    }

    if (failures == 0) std::cout << "ALL TOWERUNDERATTACK CHECKS PASSED\n";
    return failures ? 1 : 0;
}
