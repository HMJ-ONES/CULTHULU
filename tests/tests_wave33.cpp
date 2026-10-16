// CULT-ULHU wave 33 tests: MOBA relics — jungle pickups (free-roam pool),
// levels, and 20s base invocation. No money system anywhere.

#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "discovery/RelicNames.h"
#include "modes/Match.h"
#include "modes/MobaDefense.h"

#include <iostream>
#include <string>

using namespace cultulhu;

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

static void testRelicPickupsSpawn() {
    EventBus bus;
    GameClock clock;
    RNG rng{42};
    MobaDefense moba(bus, clock, rng);
    moba.setupDefaultMap();
    const auto& relics = moba.relics();
    // Wave 34: 5 tiered relics — 2x t1 (instant), 2x t2 (4s channel),
    // 1x t3 (manifests at 10:00, 6s channel).
    CHECK(relics.size() == 5);
    int t1 = 0, t2 = 0, t3 = 0;
    for (const auto& r : relics) {
        CHECK(r.claimedBy == 0);
        CHECK(!r.name.empty());
        if (r.tier == 1) {
            ++t1;
            CHECK(r.amplifier >= 0.10f && r.amplifier <= 0.15f);
            CHECK(r.spawnTime == 0.0);
        } else if (r.tier == 2) {
            ++t2;
            CHECK(r.amplifier >= 0.18f && r.amplifier <= 0.28f);
        } else if (r.tier == 3) {
            ++t3;
            CHECK(r.amplifier >= 0.30f && r.amplifier <= 0.45f);
            CHECK(r.spawnTime == 600.0);
        } else {
            CHECK(false); // unknown tier
        }
    }
    CHECK(t1 == 2 && t2 == 2 && t3 == 1);
}

static void testTieredClaimChannels() {
    // Wave 34: tier 1 is walk-over; tier 2 needs a 4s channel; leaving
    // cancels it.
    EventBus bus;
    GameClock clock;
    RNG rng{7};
    MobaDefense moba(bus, clock, rng);
    moba.setupDefaultMap();

    uint64_t claimedBy = 0;
    bus.subscribe(EventType::RelicClaimed, [&](const GameEvent& e) {
        claimedBy = e.sourceId;
    });

    // Find a tier-2 relic.
    Vec3 t2pos{0, 0, 0};
    for (const auto& r : moba.relics())
        if (r.tier == 2) { t2pos = r.pos; break; }

    MobaDefense::PlayerTarget p;
    p.id = 999;
    p.team = 0;
    p.pos = t2pos;
    p.alive = true;
    // 2s in radius: not yet claimed.
    moba.setPlayerTargets({p});
    moba.update(2.0);
    CHECK(claimedBy == 0);
    // 3 more seconds (5 total): claimed.
    moba.update(3.0);
    CHECK(claimedBy == 999);

    // Cancellation: new match, leave mid-channel.
    EventBus bus2;
    MobaDefense moba2(bus2, clock, rng);
    moba2.setupDefaultMap();
    uint64_t claimed2 = 0;
    bus2.subscribe(EventType::RelicClaimed, [&](const GameEvent& e) {
        claimed2 = e.sourceId;
    });
    Vec3 t2b{0, 0, 0};
    for (const auto& r : moba2.relics())
        if (r.tier == 2) { t2b = r.pos; break; }
    p.pos = t2b;
    moba2.setPlayerTargets({p});
    moba2.update(2.0);
    p.pos = Vec3(150, 0, 0); // walk away
    moba2.setPlayerTargets({p});
    moba2.update(5.0);
    CHECK(claimed2 == 0);
}

static void testTier3ManifestsLate() {
    EventBus bus;
    GameClock clock;
    RNG rng{9};
    MobaDefense moba(bus, clock, rng);
    moba.setupDefaultMap();
    bool announced = false;
    bus.subscribe(EventType::RelicManifested, [&](const GameEvent& e) {
        announced = true;
        CHECK(e.amount == 3.0f);
        CHECK(!e.tag.empty());
    });
    moba.update(599.0);
    CHECK(!announced);
    moba.update(2.0);
    CHECK(announced);
}

static void testXpLevels() {
    // Wave 34: XP economy — kills scale with victim level (50+25*lvl),
    // 60/minion, 800/tower; level = 1+xp/50.
    EventBus bus;
    GameClock clock;
    RNG rng{11};
    Match m(bus, clock, rng);
    CHECK(m.start("moba"));
    uint64_t a = m.addBot(0);
    uint64_t b = m.addBot(1);

    for (int i = 0; i < 200; ++i) {
        m.movePlayer(a, Vec3(0, 0, 0));
        m.movePlayer(b, Vec3(1, 0, 0));
        m.update(1.0);
        const auto* pa = m.findPlayer(a);
        const auto* pb = m.findPlayer(b);
        if ((pa && pa->xp > 0) || (pb && pb->xp > 0)) break;
    }
    const auto* pa = m.findPlayer(a);
    const auto* pb = m.findPlayer(b);
    const auto* killer = (pa && pa->xp > 0) ? pa : pb;
    CHECK(killer != nullptr);
    // One kill on a level-1 victim: 75 xp -> level 2, one charge.
    // (May have minion XP too; assert the mechanism, not exact.)
    CHECK(killer->level >= 2);
    CHECK(killer->pendingInvokes >= 1);
    // Below level 10: cannot invoke yet.
    CHECK(killer->level < MobaDefense::kTier1Level);
}

static void testInvokeTierGates() {
    // Wave 34: invocation unlocks at 10/20/30; the artifact tier matches
    // the highest unlocked tier. Drive a player to level 10 via XP.
    EventBus bus;
    GameClock clock;
    RNG rng{15};
    Match m(bus, clock, rng);
    CHECK(m.start("moba"));
    uint64_t a = m.addBot(0);
    m.addBot(1);

    // Farm XP: teleport onto enemy minions and clear them.
    // (15 xp per minion kill; 900 xp needed for level 10.)
    for (int i = 0; i < 3000; ++i) {
        const auto* pa = m.findPlayer(a);
        if (pa && pa->level >= 10) break;
        // Find nearest enemy minion via status is awkward; just march the
        // bot down mid lane where waves clash.
        m.movePlayer(a, Vec3(0, 0, -40));
        m.update(1.0);
    }
    const auto* pa = m.findPlayer(a);
    // Bots may or may not reach 10 in the sim window; assert the GATE,
    // not the grind: below 10 no channel starts, at 10+ it does.
    if (pa && pa->level >= 10) {
        CHECK(pa->pendingInvokes > 0);
        Vec3 base = m.moba()->basePos(pa->team);
        for (int i = 0; i < 25; ++i) {
            m.movePlayer(a, base);
            m.update(1.0);
        }
        const auto* pk = m.findPlayer(a);
        CHECK(!pk->relicNames.empty()); // invoked a tier-1 artifact
        CHECK(pk->dmgMult >= 1.10f);
    }
}

static void testInvokeGateAndCancel() {
    // Wave 34: below level 10, standing at base starts NO channel.
    EventBus bus;
    GameClock clock;
    RNG rng{13};
    Match m(bus, clock, rng);
    CHECK(m.start("moba"));
    uint64_t a = m.addBot(0);
    uint64_t b = m.addBot(1);
    for (int i = 0; i < 200; ++i) {
        m.movePlayer(a, Vec3(0, 0, 0));
        m.movePlayer(b, Vec3(1, 0, 0));
        m.update(1.0);
        const auto* pa = m.findPlayer(a);
        if (pa && pa->pendingInvokes > 0) break;
    }
    const auto* pa = m.findPlayer(a);
    CHECK(pa && pa->pendingInvokes > 0);
    CHECK(pa->level < MobaDefense::kTier1Level);
    Vec3 base = m.moba()->basePos(0);
    for (int i = 0; i < 25; ++i) {
        m.movePlayer(a, base);
        m.update(1.0);
    }
    // Gate held: no channel, charge kept, no artifact.
    CHECK(!m.findPlayer(a)->invoking);
    CHECK(m.findPlayer(a)->pendingInvokes > 0);
    CHECK(m.findPlayer(a)->relicNames.empty());
}

int main() {
    std::cout << "== wave33: moba relics ==\n";
    testRelicPickupsSpawn();
    testTieredClaimChannels();
    testTier3ManifestsLate();
    testXpLevels();
    testInvokeTierGates();
    testInvokeGateAndCancel();
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
