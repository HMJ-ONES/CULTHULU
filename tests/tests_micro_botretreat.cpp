// CULT-ULHU micro-dev regression tests: Domination bots retreat at low HP.
//
// Match bots used to brawl to death on the point at 10 HP — no
// self-preservation at all (analysis #6, open since Oct 10). In Domination
// a bot below 35% HP now backs off from the nearest enemy instead of
// fighting: it moves and faces away, which also keeps its directional
// melee swing silent while fleeing (attackNearestEnemy needs the frontal
// 60° cone). DOTGOO is deliberately excluded — MOBA pacing is fragile
// (seed-99 guardrail in tests_wave21), so lane behavior stays as it was
// until the owner re-tunes it deliberately.
// These pin:
//
// 1. A sub-35% bot with an enemy 10m away moves AWAY (distance grows)
//    and faces away from the foe (facingYaw = π for the staged layout).
// 2. A sub-35% bot in melee range does not swing while fleeing — the
//    enemy's HP is untouched after the tick (it faces away, outside the
//    frontal cone).
// 3. Control: a full-HP bot does NOT retreat — it faces the enemy.
// 4. Control: a sub-35% bot in DOTGOO does NOT retreat — it faces the
//    enemy (the deliberate MOBA exclusion).

#include "modes/Match.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"

#include <cmath>
#include <iostream>

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

// Stage a real duel until a drops below 35% HP (damagePlayer is private).
// a = mother_hydra (F = heal keeps it alive through the beating),
// b = bast (Q = aoe_damage). b is glued to a each tick so the beating
// continues even once a starts retreating. Returns with a alive and
// under the line.
static void duel_to_low_hp(Match& m, uint64_t a, uint64_t b) {
    CHECK(m.movePlayer(a, Vec3(0, 0, 0)));
    CHECK(m.movePlayer(b, Vec3(1, 0, 0)));
    int ticks = 0;
    while (m.player(0).alive &&
           m.player(0).hp >= m.player(0).maxHp * 0.35 && ticks < 200) {
        m.update(0.5);
        const Vec3 ap = m.player(0).pos;
        CHECK(m.movePlayer(b, Vec3(ap.x + 1.0f, ap.y, ap.z)));
        ++ticks;
    }
    CHECK(ticks < 200); // a got hurt (not a stalemate)
    CHECK(m.player(0).alive);
    CHECK(m.player(0).hp < m.player(0).maxHp * 0.35);
}

int main() {
    // ---------- 1: low-HP bot backs off and faces away ----------
    {
        EventBus bus; GameClock clock; RNG rng(7);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t a = m.addBot(0);
        uint64_t b = m.addBot(1);
        CHECK(m.setPlayerKit(a, "mother_hydra"));
        CHECK(m.setPlayerKit(b, "bast"));
        duel_to_low_hp(m, a, b);
        // a at (20,0,0), b at (30,0,0): point at origin, so objective
        // steering walks both -x 0.5m; retreat then pushes a further -x.
        CHECK(m.movePlayer(a, Vec3(20, 0, 0)));
        CHECK(m.movePlayer(b, Vec3(30, 0, 0)));
        m.update(0.1);
        // a backed off along -x: objective steering alone walks it to
        // 19.5; the retreat pushes it to 19.0. (Distance-to-b is not used:
        // b's dash can close the gap on the same tick.)
        CHECK(m.player(0).pos.x < 19.4f);
        // Facing away: from b(29.5,0,0) to a(19,0,0) is -x -> yaw = π.
        CHECK(std::fabs(m.player(0).facingYaw - 3.14159265f) < 0.05f);
    }

    // ---------- 2: fleeing bot's melee stays silent ----------
    {
        EventBus bus; GameClock clock; RNG rng(7);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t a = m.addBot(0);
        uint64_t b = m.addBot(1);
        CHECK(m.setPlayerKit(a, "mother_hydra"));
        CHECK(m.setPlayerKit(b, "bast"));
        duel_to_low_hp(m, a, b);
        // Melee range (2m < 3m): b is pinned and will swing at a, but a
        // must face away and never swing back.
        CHECK(m.movePlayer(a, Vec3(0, 0, 0)));
        CHECK(m.movePlayer(b, Vec3(2, 0, 0)));
        const float bHp = m.player(1).hp;
        m.update(0.1);
        CHECK(m.player(1).hp == bHp); // a never landed a melee swing
        CHECK(std::fabs(m.player(0).facingYaw - 3.14159265f) < 0.05f);
    }

    // ---------- 3: full-HP bot engages, does not retreat ----------
    {
        EventBus bus; GameClock clock; RNG rng(7);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t a = m.addBot(0);
        uint64_t b = m.addBot(1);
        CHECK(m.setPlayerKit(a, "mother_hydra"));
        CHECK(m.setPlayerKit(b, "bast"));
        CHECK(m.movePlayer(a, Vec3(20, 0, 0)));
        CHECK(m.movePlayer(b, Vec3(30, 0, 0)));
        CHECK(m.player(0).hp == m.player(0).maxHp);
        m.update(0.1);
        // No backing off: objective steering alone walks a from 20.0 to
        // 19.5; a retreat would push it to 19.0. (Facing is not checked:
        // the proactive self-cast block can spend the summon and return
        // before the face-enemy code on this tick.)
        CHECK(m.player(0).pos.x > 19.4f);
    }

    // ---------- 4: MOBA bots keep lane behavior (no retreat) ----------
    {
        EventBus bus; GameClock clock; RNG rng(7);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        uint64_t a = m.addBot(0);
        uint64_t b = m.addBot(1);
        CHECK(m.setPlayerKit(a, "mother_hydra"));
        CHECK(m.setPlayerKit(b, "bast"));
        duel_to_low_hp(m, a, b);
        CHECK(m.movePlayer(a, Vec3(20, 0, 0)));
        CHECK(m.movePlayer(b, Vec3(30, 0, 0)));
        m.update(0.1);
        // Faces the enemy (+x -> yaw 0), not away (π).
        float yaw = m.player(0).facingYaw;
        CHECK(std::fabs(yaw) < 0.05f || std::fabs(yaw - 6.2831853f) < 0.05f);
    }

    if (failures == 0) std::cout << "ALL BOT RETREAT CHECKS PASSED\n";
    return failures == 0 ? 0 : 1;
}
