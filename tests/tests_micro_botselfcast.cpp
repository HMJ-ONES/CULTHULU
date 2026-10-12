// CULT-ULHU micro-dev regression tests: bots cast self-targeted abilities
// proactively.
//
// Match bots used to hold summon/shield/buff/heal forever unless an enemy
// happened to be within 40m (the proximity gate in updateBotAbilities), so
// summoners never summoned their identity and supports never supported in
// bot hands. In Domination these now fire on cooldown without needing an
// enemy nearby; heals wait until the bot is actually hurt (< 80% HP).
// (DOTGOO is deliberately excluded — proactive self-casts cascade through
// the 20-minute MOBA pacing; see the seed-99 guardrail in tests_wave21.)
// These pin:
//
// 1. A mother_hydra bot (Q = summon) with no enemy near casts Q on the
//    first tick (qCd consumed) — previously it sat at 0 forever.
// 2. A hurt mother_hydra bot (F = heal) with no enemy near casts F
//    (fCd consumed, HP restored) — previously heal only fired mid-duel.
// 3. A full-HP mother_hydra bot does NOT burn its heal (fCd stays 0).
// 4. Control: a bast bot (Q = aoe_damage, enemy-targeted) with no enemy
//    near does not burn Q — the proximity gate still holds for damage.

#include "modes/Match.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"

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

// Two bots, ~700m apart — no enemy within the 40m proximity gate for the
// first several ticks (they walk toward the point at ~7 m/s).
static void far_apart(Match& m, uint64_t a, uint64_t b) {
    CHECK(m.movePlayer(a, Vec3(0, 0, 0)));
    CHECK(m.movePlayer(b, Vec3(500, 0, 500)));
}

int main() {
    // ---------- 1: summon Q fires with nobody near ----------
    {
        EventBus bus; GameClock clock; RNG rng(7);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t a = m.addBot(0);
        uint64_t b = m.addBot(1);
        CHECK(m.setPlayerKit(a, "mother_hydra")); // Q = summon
        far_apart(m, a, b);
        CHECK(m.player(0).qCd == 0.0);
        m.update(0.1);
        CHECK(m.player(0).qCd > 0.0); // summon spent on the first tick
    }

    // ---------- 2: heal F fires when hurt, restores HP ----------
    // Staged as a real duel (damagePlayer is private): b mauls a until a
    // drops under the 80% heal line, then they are separated — a has had
    // no tick since crossing the line, so no heal could have fired yet.
    {
        EventBus bus; GameClock clock; RNG rng(7);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t a = m.addBot(0);
        uint64_t b = m.addBot(1);
        CHECK(m.setPlayerKit(a, "mother_hydra")); // F = heal
        CHECK(m.setPlayerKit(b, "cthulhu_avatar"));
        CHECK(m.movePlayer(a, Vec3(0, 0, 0)));
        CHECK(m.movePlayer(b, Vec3(1, 0, 0)));
        int ticks = 0;
        while (m.player(0).alive &&
               m.player(0).hp >= m.player(0).maxHp * 0.8 && ticks < 30) {
            m.update(1.0);
            ++ticks;
        }
        CHECK(ticks < 30); // a got hurt (not a stalemate)
        CHECK(m.player(0).alive);
        const double hurtHp = m.player(0).hp;
        CHECK(hurtHp < m.player(0).maxHp * 0.8);
        far_apart(m, a, b); // no enemy near anymore
        m.update(0.1);
        m.update(0.1);
        CHECK(m.player(0).fCd > 0.0); // heal spent, not held
        CHECK(m.player(0).hp > hurtHp);
    }

    // ---------- 3: full-HP bot does not waste its heal ----------
    {
        EventBus bus; GameClock clock; RNG rng(7);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t a = m.addBot(0);
        uint64_t b = m.addBot(1);
        CHECK(m.setPlayerKit(a, "mother_hydra"));
        far_apart(m, a, b);
        for (int i = 0; i < 3; ++i) m.update(0.1);
        CHECK(m.player(0).fCd == 0.0); // heal held at full HP
        CHECK(m.player(0).hp == m.player(0).maxHp);
    }

    // ---------- 4: control — enemy-targeted Q still gated ----------
    {
        EventBus bus; GameClock clock; RNG rng(7);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t a = m.addBot(0);
        uint64_t b = m.addBot(1);
        CHECK(m.setPlayerKit(a, "bast")); // Q = aoe_damage (needs a target)
        far_apart(m, a, b);
        for (int i = 0; i < 3; ++i) m.update(0.1);
        CHECK(m.player(0).qCd == 0.0); // never burned with nobody near
    }

    if (failures == 0) std::cout << "ALL BOT SELF-CAST CHECKS PASSED\n";
    return failures == 0 ? 0 : 1;
}
