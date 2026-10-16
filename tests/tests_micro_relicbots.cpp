// CULT-ULHU 2 PM build regression tests (2026-10-14): DOTGOO bots
// contest jungle relics.
//
// Tier-2/3 jungle relics were effectively human-only — updateMobaObjective
// had no relic steering, so a human who detoured always out-scaled bot
// teams. These pin, through the production match loop:
//
// 1. A bot near an unclaimed tier-1 relic steers to it and claims it
//    (walk-over) with no driver input — RelicClaimed fires with the
//    bot's id. Without steering the bot marches its lane away from the
//    relic, so the claim proves the new behavior.
// 2. A bot already in a tower fight (enemy tower inside the existing 12m
//    lane-threat check) does NOT abandon the siege for a nearby relic —
//    the relic stays unclaimed and the bot stays on the tower.
// 3. Chaser cap: with two teammates already committed to a tier-2 relic,
//    a third in-range teammate keeps its lane march instead of dogpiling
//    (it marches away from the relic in x, never toward it), while one
//    of the committed pair completes the 4s channel and claims.

#include "modes/Match.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"

#include <cmath>
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

static bool findRelic(const MobaDefense* moba, int tier, float zSign,
                      Vec3& outPos, std::string& outName) {
    for (const auto& r : moba->relics()) {
        if (r.tier == tier && r.claimedBy == 0 &&
            (zSign < 0 ? r.pos.z < 0 : r.pos.z > 0)) {
            outPos = r.pos;
            outName = r.name;
            return true;
        }
    }
    return false;
}

static bool relicClaimedBy(const MobaDefense* moba, const Vec3& pos,
                           uint64_t& by) {
    for (const auto& r : moba->relics()) {
        if (r.pos.distance(pos) < 1.0f) {
            by = r.claimedBy;
            return r.claimedBy != 0;
        }
    }
    return false;
}

int main() {
    // 1: bot steers to a nearby tier-1 relic and walk-over claims it.
    {
        EventBus bus;
        GameClock clock;
        RNG rng(21);
        std::vector<GameEvent> claims;
        bus.subscribe(EventType::RelicClaimed,
                      [&claims](const GameEvent& e) { claims.push_back(e); });

        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        const uint64_t p = m.addBot(0);
        CHECK(m.setPlayerKit(p, "cthulhu_avatar"));

        Vec3 relicPos;
        std::string relicName;
        CHECK(findRelic(m.moba(), 1, -1.0f, relicPos, relicName));
        // Stage 15 m off the relic: outside the 6 m claim circle, inside
        // chase range. The lane march from here heads AWAY from the relic,
        // so a claim proves steering, not the march.
        m.movePlayer(p, relicPos + Vec3(15, 0, 0));

        int ticks = 0;
        while (claims.empty() && ticks < 60) {
            m.update(0.25);
            ++ticks;
        }
        CHECK(claims.size() == 1);
        if (!claims.empty()) CHECK(claims[0].sourceId == p);
        uint64_t by = 0;
        CHECK(relicClaimedBy(m.moba(), relicPos, by));
        CHECK(by == p);
    }

    // 2: a bot mid-siege does not abandon its tower for a nearby relic.
    {
        EventBus bus;
        GameClock clock;
        RNG rng(22);

        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        const uint64_t p = m.addBot(0);
        CHECK(m.setPlayerKit(p, "cthulhu_avatar"));

        // Enemy tower on lane 0; stand 8 m off it — inside the 12 m
        // lane-threat check, outside melee.
        const Vec3 tower = m.moba()->towerPos(1, 0, 0);
        CHECK(tower.length() > 1.0f);
        Vec3 relicPos;
        std::string relicName;
        CHECK(findRelic(m.moba(), 2, -1.0f, relicPos, relicName));
        const Vec3 botStart = tower + Vec3(0, 0, 8);
        // Precondition: the gate is actually exercised — a relic IS in
        // chase range of this spot, so "no steering" is the gate's doing.
        CHECK(botStart.distance(relicPos) < Match::kRelicChaseRange);
        m.movePlayer(p, botStart);

        for (int i = 0; i < 12; ++i) m.update(0.25);

        uint64_t by = 0;
        CHECK(!relicClaimedBy(m.moba(), relicPos, by));
        const Match::Player* bp = m.findPlayer(p);
        CHECK(bp != nullptr);
        if (bp != nullptr) {
            CHECK(bp->alive); // tower shoots back; 3 s must not kill it
            CHECK(bp->pos.distance(tower) < 12.0f); // still on the tower
        }
    }

    // 3: chaser cap — two committed teammates claim the tier-2 while the
    // third keeps its lane march instead of dogpiling.
    {
        EventBus bus;
        GameClock clock;
        RNG rng(23);
        std::vector<GameEvent> claims;
        bus.subscribe(EventType::RelicClaimed,
                      [&claims](const GameEvent& e) { claims.push_back(e); });

        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        const uint64_t a = m.addBot(0);
        const uint64_t b = m.addBot(0);
        const uint64_t c = m.addBot(0);
        CHECK(m.setPlayerKit(a, "cthulhu_avatar"));
        CHECK(m.setPlayerKit(b, "cthulhu_avatar"));
        CHECK(m.setPlayerKit(c, "cthulhu_avatar"));

        // Tier-2 (4 s channel) at (0,0,-27)-ish.
        Vec3 relicPos;
        std::string relicName;
        CHECK(findRelic(m.moba(), 2, -1.0f, relicPos, relicName));
        // A and B committed (20 m < 40 m commit range); C in chase range
        // (60 m) but outside commit range. C is the third bot added, so it
        // marches lane 2 — its march heads -x, toward-relic is +x. C is
        // staged clear of the lane-0 enemy towers (a bot that wanders
        // within 12 m of one gets captured by the siege gate instead).
        m.movePlayer(a, relicPos + Vec3(20, 0, 0));
        m.movePlayer(b, relicPos + Vec3(-20, 0, 0));
        const Vec3 cStart(-40, 0, -60);
        CHECK(cStart.distance(relicPos) < Match::kRelicChaseRange);
        CHECK(cStart.distance(relicPos) > Match::kRelicCommitRange);
        m.movePlayer(c, cStart);

        for (int i = 0; i < 40; ++i) m.update(0.25);

        // One of the committed pair completes the 4 s channel and claims
        // the tier-2. (C may walk over a different relic on its march —
        // legitimate walk-over, not dogpiling — so filter by relic name.)
        int tier2Claims = 0;
        for (const auto& e : claims) {
            if (e.tag == relicName) {
                ++tier2Claims;
                CHECK(e.sourceId == a); // first in roster order holds
                                        // the channel
            }
        }
        CHECK(tier2Claims == 1);
        // C kept its lane march (-x) instead of dogpiling (+x toward the
        // relic).
        const Match::Player* cp = m.findPlayer(c);
        CHECK(cp != nullptr);
        if (cp != nullptr) CHECK(cp->pos.x < cStart.x);
    }

    if (failures == 0) std::cout << "tests_micro_relicbots: all green\n";
    return failures == 0 ? 0 : 1;
}
