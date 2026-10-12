// CULT-ULHU wave 42 regression tests: 8 PM bug-fix run (2026-10-11).
// 1. Cthulhu's rock (frontal shield) now blocks melee auto-attacks —
//    they used to bypass the shield entirely (damagePlayer had no
//    attacker position for the frontal-arc check).
// 2. Blocked damage drains durability at the documented rate: a fresh
//    rock soaks ~6 blocked level-appropriate melee hits (the old math
//    soaked ~24, disagreeing with every comment).
// 3. DOTGOO: a banked level-1 draft pick no longer eats the level-3
//    pick (picks accumulate; both Q and F stay draftable).
// 4. Wave 42 fun — kill streaks: the 2nd and 3rd kill inside the 8s
//    window publish KillStreak (amount 2 / 3), the first kill is silent,
//    death resets the streak, and a lapsed window restarts it at 1.
//
// Staging: the attacker is added FIRST so its auto-swing lands before
// the victim's retaliation each update tick.

#include "modes/Match.h"
#include "modes/MatchKits.h"
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

static const float kPi = 3.14159265f;

// A 1v1 Domination duel: attacker at +x of the victim, both facing each
// other (attacker looks -x, victim looks +x), in melee range.
static void stageDuel(Match& m, uint64_t attacker, uint64_t victim) {
    m.movePlayer(attacker, Vec3(1.5f, 0, 0));
    m.movePlayer(victim, Vec3(0, 0, 0));
    m.setFacingYaw(attacker, kPi); // facing -x: toward the victim
    m.setFacingYaw(victim, 0.0f);  // facing +x: toward the attacker
}

static void rock_checks();
static void draft_checks();
static void streak_checks();

int main() {
    rock_checks();
    draft_checks();
    streak_checks();
    if (failures == 0) std::cout << "wave42: all checks passed\n";
    else std::cout << "wave42: " << failures << " checks FAILED\n";
    return failures == 0 ? 0 : 1;
}

void rock_checks() {
    // ---------- frontal melee is halved by the rock ----------
    {
        EventBus bus; GameClock clock; RNG rng(420);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t a = m.addPlayer("Attacker", 0);
        uint64_t v = m.addPlayer("Victim", 1);
        CHECK(m.setPlayerKit(v, "cthulhu_avatar"));
        CHECK(m.humanCast(v, 3, 0.0f, Match::RmbAim::Down)); // rip the rock
        CHECK(m.findPlayer(v)->hasRock);
        CHECK(m.findPlayer(v)->rockDurability == 3.0f);
        stageDuel(m, a, v);
        m.update(1.0); // both swing once
        const auto* pv = m.findPlayer(v);
        // Melee is 15 dmg; the rock halves it to 7.5 from the front.
        CHECK(std::fabs(pv->hp - 92.5f) < 0.05f);
        CHECK(pv->hasRock);
        // One blocked level-1 melee hit drains (7.5/30.6)*2 = 0.49.
        CHECK(pv->rockDurability < 3.0f);
        CHECK(pv->rockDurability > 2.4f);
    }
    // ---------- rear melee ignores the rock ----------
    {
        EventBus bus; GameClock clock; RNG rng(421);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t a = m.addPlayer("Attacker", 0);
        uint64_t v = m.addPlayer("Victim", 1);
        CHECK(m.setPlayerKit(v, "cthulhu_avatar"));
        CHECK(m.humanCast(v, 3, 0.0f, Match::RmbAim::Down));
        // Attacker behind the victim (victim faces +x, attacker at -x).
        m.movePlayer(a, Vec3(-1.5f, 0, 0));
        m.movePlayer(v, Vec3(0, 0, 0));
        m.setFacingYaw(a, 0.0f); // facing +x: toward the victim
        m.setFacingYaw(v, 0.0f); // facing +x: attacker is behind
        m.update(1.0);
        const auto* pv = m.findPlayer(v);
        CHECK(std::fabs(pv->hp - 85.0f) < 0.05f); // full 15 dmg
        CHECK(pv->rockDurability == 3.0f);       // untouched
        CHECK(pv->hasRock);
    }
    // ---------- no rock: full melee damage (control) ----------
    {
        EventBus bus; GameClock clock; RNG rng(422);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t a = m.addPlayer("Attacker", 0);
        uint64_t v = m.addPlayer("Victim", 1);
        stageDuel(m, a, v);
        m.update(1.0);
        CHECK(std::fabs(m.findPlayer(v)->hp - 85.0f) < 0.05f);
    }
    // ---------- ~6 blocked hits break a fresh rock ----------
    {
        EventBus bus; GameClock clock; RNG rng(423);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t a = m.addPlayer("Attacker", 0);
        uint64_t v = m.addPlayer("Victim", 1);
        CHECK(m.setPlayerKit(v, "cthulhu_avatar"));
        CHECK(m.humanCast(v, 3, 0.0f, Match::RmbAim::Down));
        stageDuel(m, a, v);
        int swings = 0;
        for (; swings < 10 && m.findPlayer(v)->hasRock; ++swings)
            m.update(1.0);
        CHECK(!m.findPlayer(v)->hasRock); // broke within 10 blocked swings
        CHECK(swings <= 8);               // ~6-hit design soak, not ~24
        // Every blocked swing was halved: 100 - 7 * 7.5 = 47.5.
        CHECK(std::fabs(m.findPlayer(v)->hp - 47.5f) < 0.1f);
    }
}

// ---------- wave 41 fix: DOTGOO draft picks accumulate ----------
// Banking the level-1 pick and crossing level 3 in one XP jump used to
// eat the level-3 pick (the old `pendingPicks == 0` guard skipped the
// grant), permanently locking Q or F for the whole match.
void draft_checks() {
    // Banked level-1 pick + one jump to level 3: two picks, both draftable.
    {
        EventBus bus; GameClock clock; RNG rng(424);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        uint64_t h = m.addPlayer("Human", 0);
        const auto* ph = m.findPlayer(h);
        CHECK(ph->pendingPicks == 1);
        CHECK(!ph->qUnlocked && !ph->fUnlocked);
        // One big XP jump to level 3 (a tower + kill landing in one tick
        // does this in a real match). The level-1 pick stays banked.
        CHECK(m.grantXp(h, 100)); // xp = 100 -> level 3
        CHECK(m.findPlayer(h)->level == 3);
        CHECK(m.findPlayer(h)->pendingPicks == 2); // level-3 pick arrived
        CHECK(m.draftAbility(h, 0));               // Q with the banked pick
        CHECK(m.draftAbility(h, 1));               // F with the level-3 pick
        CHECK(m.findPlayer(h)->qUnlocked && m.findPlayer(h)->fUnlocked);
        CHECK(m.findPlayer(h)->pendingPicks == 0);
    }
    // Spent level-1 pick + jump to 3: exactly one pick (no double grant).
    {
        EventBus bus; GameClock clock; RNG rng(425);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        uint64_t h = m.addPlayer("Human", 0);
        CHECK(m.draftAbility(h, 0)); // spend the level-1 pick on Q
        CHECK(m.findPlayer(h)->pendingPicks == 0);
        CHECK(m.grantXp(h, 100)); // -> level 3
        CHECK(m.findPlayer(h)->pendingPicks == 1);
        CHECK(m.draftAbility(h, 1)); // F with the level-3 pick
        CHECK(m.findPlayer(h)->fUnlocked);
        CHECK(m.findPlayer(h)->pendingPicks == 0);
    }
}

// ---------- wave 42 fun: kill streaks ----------
// Kill streaks announce at 2+ kills inside the 8s window, stay silent
// on the first kill, reset on death, and restart at 1 when the window
// lapses. Staged with facing-away victims so the killer takes no return
// damage (melee is a directional 120° cone); XP levels don't change
// melee damage in capture mode, so every kill lands on tick 7.
static const Vec3 kStreakFar(40.0f, 0, 40.0f);

// Put a victim in the killer's frontal cone, facing away so it never
// swings back.
static void stageStreakVictim(Match& m, uint64_t killer, uint64_t victim) {
    m.movePlayer(killer, Vec3(0, 0, 0));
    m.setFacingYaw(killer, 0.0f); // facing +x
    m.movePlayer(victim, Vec3(1.5f, 0, 0));
    m.setFacingYaw(victim, 0.0f); // facing +x: the killer is behind it
}

// Tick until `victim` dies; returns the elapsed match time of the kill.
static double killUntilDead(Match& m, uint64_t victim) {
    for (int i = 0; i < 40; ++i) {
        m.update(1.0);
        const auto* pv = m.findPlayer(victim);
        CHECK(pv != nullptr);
        if (pv && !pv->alive) return m.elapsed();
    }
    CHECK(false); // the victim never died — staging is broken
    return m.elapsed();
}

struct StreakHit {
    uint64_t src;
    int amount;
    std::string tag;
};

void streak_checks() {
    // ---------- rampage: 2nd kill announces, 3rd announces, 1st silent
    {
        EventBus bus; GameClock clock; RNG rng(426);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        std::vector<StreakHit> hits;
        bus.subscribe(EventType::KillStreak, [&](const GameEvent& e) {
            hits.push_back(
                {e.sourceId, static_cast<int>(e.amount), e.tag});
        });
        uint64_t k = m.addPlayer("K", 0);
        uint64_t v1 = m.addPlayer("V1", 1);
        uint64_t v2 = m.addPlayer("V2", 1);
        uint64_t v3 = m.addPlayer("V3", 1);
        m.movePlayer(v2, kStreakFar);
        m.movePlayer(v3, kStreakFar);
        stageStreakVictim(m, k, v1);
        const double t1 = killUntilDead(m, v1);
        CHECK(hits.empty()); // first blood stays silent
        CHECK(m.findPlayer(k)->killStreak == 1);
        stageStreakVictim(m, k, v2);
        const double t2 = killUntilDead(m, v2);
        CHECK(t2 - t1 < 8.0);
        CHECK(hits.size() == 1); // DOUBLE KILL
        CHECK(hits[0].src == k);
        CHECK(hits[0].amount == 2);
        CHECK(hits[0].tag == "K");
        stageStreakVictim(m, k, v3);
        const double t3 = killUntilDead(m, v3);
        CHECK(t3 - t2 < 8.0);
        CHECK(hits.size() == 2); // TRIPLE KILL
        CHECK(hits[1].src == k);
        CHECK(hits[1].amount == 3);
        CHECK(m.findPlayer(k)->killStreak == 3);
    }
    // ---------- death ends the streak ----------
    {
        EventBus bus; GameClock clock; RNG rng(427);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        std::vector<StreakHit> hits;
        bus.subscribe(EventType::KillStreak, [&](const GameEvent& e) {
            hits.push_back(
                {e.sourceId, static_cast<int>(e.amount), e.tag});
        });
        uint64_t k = m.addPlayer("K", 0);
        uint64_t v1 = m.addPlayer("V1", 1);
        uint64_t v2 = m.addPlayer("V2", 1);
        m.movePlayer(v2, kStreakFar);
        stageStreakVictim(m, k, v1);
        killUntilDead(m, v1);
        CHECK(m.findPlayer(k)->killStreak == 1);
        // V2 faces the killer while the killer looks away: only V2 swings.
        m.movePlayer(k, Vec3(0, 0, 0));
        m.setFacingYaw(k, 0.0f); // facing +x: V2 is behind
        m.movePlayer(v2, Vec3(-1.5f, 0, 0));
        m.setFacingYaw(v2, 0.0f); // facing +x: toward the killer
        killUntilDead(m, k);
        CHECK(m.findPlayer(k)->killStreak == 0);
        CHECK(hits.empty()); // V2's lone kill announces nothing
    }
    // ---------- a lapsed 8s window restarts the streak at 1 ----------
    {
        EventBus bus; GameClock clock; RNG rng(428);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        std::vector<StreakHit> hits;
        bus.subscribe(EventType::KillStreak, [&](const GameEvent& e) {
            hits.push_back(
                {e.sourceId, static_cast<int>(e.amount), e.tag});
        });
        uint64_t k = m.addPlayer("K", 0);
        uint64_t v1 = m.addPlayer("V1", 1);
        uint64_t v2 = m.addPlayer("V2", 1);
        m.movePlayer(v2, kStreakFar);
        stageStreakVictim(m, k, v1);
        killUntilDead(m, v1);
        m.update(9.0); // more than the 8s streak window passes
        stageStreakVictim(m, k, v2);
        killUntilDead(m, v2);
        CHECK(hits.empty()); // stale streak: back to a lone kill
        CHECK(m.findPlayer(k)->killStreak == 1);
    }
}
