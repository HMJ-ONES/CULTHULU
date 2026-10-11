// CULT-ULHU wave 40 tests: directional kit combat in 5v5 matches.
// Every fighter wields a full character kit (Q/F/R + RMB); all attacks
// are directional (cones), and combat plays the same in Domination,
// DOTGOO, and free-roam.

#include "modes/Match.h"
#include "modes/MatchKits.h"
#include "characters/CharacterRegistry.h"
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

static void wave41_checks();

int main() {
    // ---------- inAttackCone: directionality ----------
    {
        Vec3 c{0, 0, 0};
        // Facing +x (yaw 0): target at +x is inside the cone.
        CHECK(inAttackCone(c, 0.0f, Vec3(5, 0, 0), 8.0f, 60.0f));
        // Target behind (-x) is outside a 60° frontal cone.
        CHECK(!inAttackCone(c, 0.0f, Vec3(-5, 0, 0), 8.0f, 60.0f));
        // Out of range, even dead ahead.
        CHECK(!inAttackCone(c, 0.0f, Vec3(50, 0, 0), 8.0f, 60.0f));
        // 90° to the side: outside 60° half-angle, inside 100°.
        CHECK(!inAttackCone(c, 0.0f, Vec3(0, 0, 5), 8.0f, 60.0f));
        CHECK(inAttackCone(c, 0.0f, Vec3(0, 0, 5), 8.0f, 100.0f));
        // Facing -x (yaw π): the cone follows the facing.
        const float pi = 3.14159265f;
        CHECK(inAttackCone(c, pi, Vec3(-5, 0, 0), 8.0f, 60.0f));
        CHECK(!inAttackCone(c, pi, Vec3(5, 0, 0), 8.0f, 60.0f));
        // yawTo points from caster toward target.
        CHECK(std::fabs(yawTo(c, Vec3(1, 0, 0)) - 0.0f) < 0.01f);
        CHECK(std::fabs(yawTo(c, Vec3(-1, 0, 0)) - pi) < 0.01f);
    }
    // ---------- kits load: all 20 character kits available ----------
    {
        CharacterRegistry reg;
        const int n = loadMatchKits(reg, "assets");
        CHECK(n >= 20);
        CHECK(reg.get("cthulhu_avatar") != nullptr);
        const CharacterDef* cth = reg.get("cthulhu_avatar");
        CHECK(!cth->qAbility.id.empty());
        CHECK(!cth->fAbility.id.empty());
        CHECK(!cth->rAbility.id.empty());
        CHECK(!cth->rightClick.name.empty());
    }
    // ---------- directional melee: facing matters ----------
    // Both duelists are human (no bot AI re-facing them).
    {
        EventBus bus; GameClock clock; RNG rng(40);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t a = m.addPlayer("DuellistA", 0);
        uint64_t b = m.addPlayer("DuellistB", 1);
        // A faces away from B: A's swings whiff. B faces away too: no
        // counterhits (B at +x facing +x is looking away from A at origin).
        m.movePlayer(a, Vec3(0, 0, 0));
        m.movePlayer(b, Vec3(1, 0, 0));
        CHECK(m.setFacingYaw(a, 3.14159265f)); // facing -x, B at +x: away
        CHECK(m.setFacingYaw(b, 0.0f));        // facing +x: away from A
        const float hpBefore = m.findPlayer(b)->hp;
        for (int i = 0; i < 10; ++i) m.update(1.0);
        CHECK(m.findPlayer(b)->hp == hpBefore); // nobody hit anybody
        // A turns to face B: melee connects.
        CHECK(m.setFacingYaw(a, 0.0f));
        for (int i = 0; i < 10; ++i) m.update(1.0);
        CHECK(m.findPlayer(b)->hp < hpBefore);
    }
    // ---------- kit abilities: Q cast is directional ----------
    {
        EventBus bus; GameClock clock; RNG rng(41);
        struct CastRec { int n = 0; };
        CastRec rec;
        bus.subscribe(EventType::AbilityCast,
                      [&](const GameEvent&) { ++rec.n; });
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t a = m.addBot(0);
        uint64_t b = m.addBot(1);
        CHECK(m.setPlayerKit(a, "cthulhu_avatar")); // Q = Tentacle Slam (aoe)
        CHECK(m.setPlayerKit(b, ""));
        m.movePlayer(a, Vec3(0, 0, 0));
        m.movePlayer(b, Vec3(5, 0, 0)); // within Q range, ahead
        CHECK(m.setFacingYaw(a, 0.0f));
        CHECK(m.setFacingYaw(b, 0.0f)); // B faces away, won't fight back
        const float hpBefore = m.findPlayer(b)->hp;
        for (int i = 0; i < 5; ++i) m.update(1.0);
        // A (bot) should have cast something at B by now.
        CHECK(rec.n > 0);
        CHECK(m.findPlayer(b)->hp < hpBefore);
    }
    // ---------- RMB: heavy attack is directional ----------
    {
        EventBus bus; GameClock clock; RNG rng(42);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t a = m.addBot(0);
        uint64_t b = m.addBot(1);
        CHECK(m.setPlayerKit(a, "cthulhu_avatar"));
        CHECK(m.setPlayerKit(b, ""));
        // Human-style RMB via humanCast. joinAsHuman respawns at base, so
        // reposition next to the target first.
        uint64_t hid = m.joinAsHuman(0);
        CHECK(hid != 0);
        m.movePlayer(hid, Vec3(0, 0, 0));
        m.movePlayer(b, Vec3(2, 0, 0));
        const float hpBefore = m.findPlayer(b)->hp;
        // Face the human at B and fire the RMB (MindControl: narrow cone).
        CHECK(m.humanCast(hid, 3, 0.0f));
        CHECK(m.findPlayer(b)->hp < hpBefore);
    }
    // ---------- full Domination match with kits completes ----------
    {
        EventBus bus; GameClock clock; RNG rng(43);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        m.botfill();
        // Every fighter wields a kit.
        for (size_t i = 0; i < m.playerCount(); ++i)
            CHECK(!m.player(i).kitId.empty());
        int t = 0;
        for (; t < 1200 && !m.isOver(); ++t) m.update(1.0);
        CHECK(m.isOver());
        CHECK(m.winner() == 0 || m.winner() == 1);
    }
    // ---------- full DOTGOO match with kits completes ----------
    {
        EventBus bus; GameClock clock; RNG rng(44);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        m.botfill();
        for (size_t i = 0; i < m.playerCount(); ++i)
            CHECK(!m.player(i).kitId.empty());
        int t = 0;
        for (; t < 1500 && !m.isOver(); ++t) m.update(1.0);
        CHECK(m.isOver());
        CHECK(m.winner() == 0 || m.winner() == 1);
    }

    wave41_checks();

    if (failures == 0) std::cout << "wave40: all checks passed\n";
    else std::cout << "wave40: " << failures << " checks FAILED\n";
    return failures == 0 ? 0 : 1;
}

// ---------- wave 41: ability unlocks ----------
// Domination: full kit unlocked, ult starts on full cooldown (0% charged).
// DOTGOO: draft at levels 1/3, ult unlocks at 6.
void wave41_checks() {
    // Domination: R starts on cooldown.
    {
        EventBus bus; GameClock clock; RNG rng(100);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t a = m.addBot(0);
        const auto* pa = m.findPlayer(a);
        CHECK(pa->qUnlocked && pa->fUnlocked && pa->rUnlocked);
        CHECK(pa->rCd > 0.0); // ult at 0%: full cooldown ticking
        CHECK(pa->qCd == 0.0 && pa->fCd == 0.0);
    }
    // DOTGOO: everything locked at start, one pending pick.
    {
        EventBus bus; GameClock clock; RNG rng(101);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        uint64_t a = m.addBot(0);
        const auto* pa = m.findPlayer(a);
        CHECK(!pa->qUnlocked && !pa->fUnlocked && !pa->rUnlocked);
        CHECK(pa->pendingPicks == 1);
        // Locked Q can't be cast.
        m.movePlayer(a, Vec3(0, 0, 0));
        uint64_t b = m.addBot(1);
        m.movePlayer(b, Vec3(2, 0, 0));
        CHECK(!m.humanCast(a, 0, 0.0f)); // humanCast on a bot id works for test
    }
    // DOTGOO: draftAbility spends picks; bots auto-draft.
    {
        EventBus bus; GameClock clock; RNG rng(102);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        uint64_t a = m.addBot(0);
        CHECK(m.draftAbility(a, 0)); // pick Q
        CHECK(m.findPlayer(a)->qUnlocked);
        CHECK(m.findPlayer(a)->pendingPicks == 0);
        CHECK(!m.draftAbility(a, 0)); // no picks left
        CHECK(!m.draftAbility(a, 1)); // no picks left
    }
    // DOTGOO: level 6 unlocks the ult.
    {
        EventBus bus; GameClock clock; RNG rng(103);
        int ults = 0;
        bus.subscribe(EventType::AbilityUnlocked, [&](const GameEvent& e) {
            if (e.tag == "ULT") ++ults;
        });
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        m.botfill();
        for (int t = 0; t < 600 && !m.isOver(); ++t) m.update(1.0);
        CHECK(ults > 0); // someone hit level 6 and got their ult
    }
}
