// CULT-ULHU micro-dev regression tests: end-of-match personal stat line.
//
// The driver's MatchEnded read now prints "*** Your line: K/D/A, level N
// ***" for the joined human. The driver resolves the human's stats by
// scanning player(i).id == humanMatchId and reading stats().rows()[i]
// (player-index order), plus the Player's level / relicNames / dmgMult.
// These pin that data contract through the production path:
//
// 1. The joined human lands a real melee kill through attackNearestEnemy
//    (production combat: recordDamage -> damagePlayer -> killPlayer ->
//    recordKill), so the human's stats row carries kills == 1 and the
//    victim's row carries deaths == 1.
// 2. rows().size() == playerCount() — every player has a row, in
//    player-index order, so the driver's id-scan index is always valid.
// 3. The human's Player still resolves after the kill with readable
//    level, relicNames, and dmgMult — the three fields the line prints.
// 4. A second human-free pass: no joined human means no resolvable row
//    index for the personal line (the driver gates on humanMatchId).
//
// Staging: capture mode, 5v5 botfill, human joins team 0. Everyone except
// the human and one team-1 victim is stunned for 900 s; the victim is
// softened to 1 HP and parked inside the human's +x melee cone
// (facingYaw defaults to 0 = +x, melee range is 3.0). A couple of ticks:
// the human swings (1 s cooldown), the kill lands through the real
// recordKill path.

#include "modes/Match.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"

#include <cstdint>
#include <iostream>
#include <string>

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

// The driver's exact index resolution: player(i).id == humanMatchId.
static int humanRowIndex(const Match& m, uint64_t humanMatchId) {
    const auto rows = m.stats().rows();
    for (size_t i = 0; i < m.playerCount() && i < rows.size(); ++i)
        if (m.player(i).id == humanMatchId) return static_cast<int>(i);
    return -1;
}

int main() {
    // ---------- 1-3. Human kill through production combat ----------
    {
        EventBus bus; GameClock clock; RNG rng(9131);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        m.botfill();
        const uint64_t human = m.joinAsHuman(0);
        CHECK(human != 0);

        // Pick the human and one team-1 victim; stun everyone else.
        size_t victimIdx = m.playerCount();
        for (size_t i = 0; i < m.playerCount(); ++i) {
            const auto& p = m.player(i);
            if (p.id == human) continue;
            if (p.team == 1 && victimIdx == m.playerCount())
                victimIdx = i; // first team-1 bot is the victim
            else
                CHECK(m.debugStunPlayer(p.id, 900.0));
        }
        CHECK(victimIdx < m.playerCount());
        const uint64_t victimId = m.player(victimIdx).id;

        CHECK(m.movePlayer(human, Vec3(0.0f, 0.0f, 0.0f)));
        CHECK(m.movePlayer(victimId, Vec3(2.5f, 0.0f, 0.0f)));
        // Soften the victim to 1 HP through the production damage path.
        const float vMax = m.player(victimIdx).maxHp;
        CHECK(m.debugDamagePlayer(victimId, vMax - 1.0f));
        const Match::Player* pv0 = m.findPlayer(victimId);
        CHECK(pv0 != nullptr && pv0->alive);

        for (int t = 0; t < 3; ++t) m.update(1.0); // swing kills (1 s cd)

        const int hi = humanRowIndex(m, human);
        CHECK(hi >= 0); // contract 2: the human always resolves to a row
        CHECK(m.stats().rows().size() == m.playerCount());
        CHECK(m.stats().rows()[static_cast<size_t>(hi)].kills == 1);

        // The victim's row carries the death, at its own player index.
        int vi = -1;
        for (size_t i = 0; i < m.playerCount(); ++i)
            if (m.player(i).id == victimId) vi = static_cast<int>(i);
        CHECK(vi >= 0);
        CHECK(m.stats().rows()[static_cast<size_t>(vi)].deaths == 1);

        // Contract 3: the three printed fields resolve on the Player.
        const Match::Player* hp = m.findPlayer(human);
        CHECK(hp != nullptr);
        CHECK(hp->level >= 1);
        CHECK(hp->dmgMult > 0.0f);
        (void)hp->relicNames; // readable; empty at this point
    }

    // ---------- 4. No human: nothing for the personal line to resolve --
    {
        EventBus bus; GameClock clock; RNG rng(9132);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        m.botfill();
        CHECK(humanRowIndex(m, 0) < 0); // humanMatchId == 0 resolves nowhere
    }

    if (failures == 0) std::cout << "ALL YOURLINE OK\n";
    return failures ? 1 : 0;
}
