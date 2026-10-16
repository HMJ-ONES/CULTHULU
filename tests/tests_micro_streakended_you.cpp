// CULT-ULHU micro-dev regression tests: shutdown callouts in second person.
//
// The driver's StreakEnded callout reads "*** You ended <name>'s RAMPAGE! ***"
// when the joined human's fighter lands the shutdown, and "*** YOUR RAMPAGE
// was ended by <name>! ***" when the human's own streak is cut — both branch
// on `humanMatchId != 0` plus `e.sourceId == humanMatchId` (human ended it) or
// `e.targetId == humanMatchId` (human was the streak holder), with the killer
// name resolved by `findPlayer(e.sourceId)` falling back to `e.tag` and then
// the literal "the minions".
//
// The player-attributed side (sourceId/targetId are the killer's and holder's
// player ids, tag = killer name) is pinned by tests_micro_streakend. This pins
// what the "YOUR STREAK was ended" branch depends on for a NON-player kill
// (minions, hazards, debug damage): tag is empty and sourceId does NOT
// resolve to any player, so the driver must fall back to "the minions" and
// targetId is still the holder's player id so the personal branch can fire.
// If a future change starts naming the killer or reusing id 0 for a player,
// this breaks loudly instead of printing "*** YOUR RAMPAGE was ended by ! ***"
// or, worse, naming the wrong fighter.

#include "modes/Match.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"

#include <cstdint>
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

struct EndHit {
    uint64_t sourceId;
    uint64_t targetId;
    int amount;
    std::string tag;
};

static const Vec3 kFar(40.0f, 0, 40.0f);

// Put a victim in the killer's frontal cone, facing away so it never
// swings back (melee is a directional cone).
static void stageVictim(Match& m, uint64_t killer, uint64_t victim) {
    m.movePlayer(killer, Vec3(0, 0, 0));
    m.setFacingYaw(killer, 0.0f); // facing +x
    m.movePlayer(victim, Vec3(1.5f, 0, 0));
    m.setFacingYaw(victim, 0.0f); // facing +x: the killer is behind it
}

// Tick until `victim` dies.
static void killUntilDead(Match& m, uint64_t victim) {
    for (int i = 0; i < 40; ++i) {
        m.update(1.0);
        const auto* pv = m.findPlayer(victim);
        CHECK(pv != nullptr);
        if (pv && !pv->alive) return;
    }
    CHECK(false); // the victim never died — staging is broken
}

int main() {
    // ---------- 1. non-player shutdown: "the minions" fallback contract ---
    {
        EventBus bus; GameClock clock; RNG rng(431);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        std::vector<EndHit> hits;
        bus.subscribe(EventType::StreakEnded, [&](const GameEvent& e) {
            hits.push_back({e.sourceId, e.targetId,
                            static_cast<int>(e.amount), e.tag});
        });
        uint64_t h = m.addPlayer("H", 0);
        uint64_t v1 = m.addPlayer("V1", 1);
        uint64_t v2 = m.addPlayer("V2", 1);
        uint64_t v3 = m.addPlayer("V3", 1);
        m.movePlayer(v2, kFar);
        m.movePlayer(v3, kFar);
        CHECK(m.setPlayerKit(h, "cthulhu_avatar"));
        // Three player-attributed kills build the streak (wave-42 staging:
        // every kill lands on tick 7, inside the 8s streak window).
        stageVictim(m, h, v1);
        killUntilDead(m, v1);
        stageVictim(m, h, v2);
        killUntilDead(m, v2);
        stageVictim(m, h, v3);
        killUntilDead(m, v3);
        CHECK(m.findPlayer(h)->killStreak == 3);
        // The shutdown comes from a non-player source (minions / hazard /
        // debug path use killPlayer(victim, -1, <entity id>)).
        const auto* ph = m.findPlayer(h);
        CHECK(ph != nullptr && ph->alive);
        CHECK(m.debugDamagePlayer(h, ph->hp + 1000.0f));
        CHECK(hits.size() == 1);
        if (!hits.empty()) {
            // targetId is still the holder's player id — the driver's
            // personal branch (`e.targetId == humanMatchId`) needs this.
            CHECK(hits[0].targetId == h);
            CHECK(hits[0].amount == 3); // "TRIPLE KILL" label in the driver
            // sourceId does not resolve to any player and tag is empty —
            // the driver's name lookup `findPlayer(e.sourceId)` misses and
            // the tag is empty, so the callout falls back to "the minions".
            CHECK(m.findPlayer(hits[0].sourceId) == nullptr);
            CHECK(hits[0].tag.empty());
            CHECK(m.findPlayer(h)->killStreak == 0); // the streak is over
        }
        // No further 3+ streak dies in this match: extra ticks stay silent.
        for (int i = 0; i < 10; ++i) m.update(1.0);
        CHECK(hits.size() == 1);
    }

    // ---------- 2. player ids really do resolve to names -------------------
    // The "You ended <name>'s ..." branch needs targetId to name the victim
    // and the "YOUR ... was ended by <name>" branch needs sourceId to name
    // the killer — both through findPlayer. Spot-check the resolution for
    // the players in this file's staging so a renamed/rekeyed player id
    // scheme would fail here, not in the callout.
    {
        EventBus bus; GameClock clock; RNG rng(432);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t h = m.addPlayer("Holder", 0);
        uint64_t e = m.addPlayer("Ender", 1);
        const auto* ph = m.findPlayer(h);
        const auto* pe = m.findPlayer(e);
        CHECK(ph != nullptr && ph->name == "Holder");
        CHECK(pe != nullptr && pe->name == "Ender");
        // Player ids live at/above kPlayerIdBase, so a zero/non-player
        // sourceId (case 1) can never accidentally resolve to a player.
        CHECK(h >= 1000 && e >= 1000);
    }

    if (failures == 0)
        std::cout << "tests_micro_streakended_you: all green\n";
    return failures == 0 ? 0 : 1;
}
