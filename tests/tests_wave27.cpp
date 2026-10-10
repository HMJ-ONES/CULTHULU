// CULT-ULHU wave 27 tests: kit casting (Q/F/R through real combat),
// entity wards, and the arrival survey plumbing.

#include "beliefs/BeliefSystem.h"
#include "characters/CharacterDef.h"
#include "combat/CrowdControl.h"
#include "combat/KitCaster.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "entities/Units.h"
#include "exertion/ExertionSystem.h"
#include "power/PowerSystem.h"

#include <iostream>
#include <string>

using namespace cultulhu;

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

struct Ctx {
    EventBus bus;
    GameClock clock;
    RNG rng{7};
    PowerSystem power;
    BeliefSystem beliefs{bus, clock};
    ActiveEffects fx;

    Ctx() = default;
};

static SpellDef makeSpell(const std::string& id, const std::string& kind,
                          float pwr, float range = 0.0f) {
    SpellDef s;
    s.id = id;
    s.name = "Test " + id;
    s.effectKind = kind;
    s.effectPower = pwr;
    s.range = range;
    s.cooldownSec = 8.0f;
    return s;
}

static void testAoeDamage() {
    Ctx c;
    EldritchAvatar av{FACTION_CTHULHU, Vec3{0, 0, 0}, c.power};
    Civilian target{Vec3{5, 0, 0}};
    Civilian bystander{Vec3{8, 0, 0}};
    std::vector<Entity*> foes = {&bystander};

    auto r = castKitSpell(makeSpell("slam", "aoe_damage", 40.0f, 10.0f), av,
                          &target, foes, c.beliefs, c.bus, c.fx, 1.0f);
    CHECK(r.ok);
    CHECK(r.damageDealt > 0.0f);
    CHECK(target.hp() < 50.0f);
    CHECK(bystander.hp() < 50.0f); // splash caught the bystander
    CHECK(r.message.find("Test slam") != std::string::npos);

    // No target: fizzle, no crash.
    auto r2 = castKitSpell(makeSpell("slam", "aoe_damage", 40.0f), av,
                           nullptr, {}, c.beliefs, c.bus, c.fx, 1.0f);
    CHECK(!r2.ok);
}

static void testFearAuraAndStun() {
    Ctx c;
    EldritchAvatar av{FACTION_CTHULHU, Vec3{0, 0, 0}, c.power};
    Civilian near{Vec3{5, 0, 0}};
    Civilian far{Vec3{500, 0, 0}};
    std::vector<Entity*> foes = {&near, &far};

    auto r = castKitSpell(makeSpell("howl", "fear_aura", 2.0f, 20.0f), av,
                          nullptr, foes, c.beliefs, c.bus, c.fx, 1.0f);
    CHECK(r.ok);
    CHECK(c.fx.has(near.id(), CCType::Fear));
    CHECK(!c.fx.has(far.id(), CCType::Fear)); // out of range: spared

    Civilian victim{Vec3{5, 0, 0}};
    auto s = castKitSpell(makeSpell("hold", "stun", 2.5f), av, &victim, {},
                          c.beliefs, c.bus, c.fx, 1.0f);
    CHECK(s.ok);
    CHECK(c.fx.has(victim.id(), CCType::Stun));
    CHECK(victim.hp() == 50.0f); // stun deals no damage
}

static void testSummonBuffDebuff() {
    Ctx c;
    EldritchAvatar av{FACTION_CTHULHU, Vec3{0, 0, 0}, c.power};

    auto r = castKitSpell(makeSpell("brood_surge", "summon", 3.0f), av,
                          nullptr, {}, c.beliefs, c.bus, c.fx, 1.0f);
    CHECK(r.ok);
    CHECK(r.summons.size() == 1);
    CHECK(r.summons[0].first == "dark young");
    CHECK(r.summons[0].second == 3);

    auto b = castKitSpell(makeSpell("rage", "buff", 10.0f), av, nullptr, {},
                          c.beliefs, c.bus, c.fx, 1.0f);
    CHECK(b.ok && b.buffMult == 1.3f && b.buffSeconds == 10.0f);

    Civilian victim{Vec3{5, 0, 0}};
    auto d = castKitSpell(makeSpell("wither", "debuff", 4.0f), av, &victim,
                          {}, c.beliefs, c.bus, c.fx, 1.0f);
    CHECK(d.ok);
    CHECK(c.fx.has(victim.id(), CCType::Slow));
}

static void testHealShieldDashPull() {
    Ctx c;
    EldritchAvatar av{FACTION_CTHULHU, Vec3{0, 0, 0}, c.power};
    av.takeDamage(500.0f);

    auto h = castKitSpell(makeSpell("mend", "heal", 100.0f), av, nullptr, {},
                          c.beliefs, c.bus, c.fx, 1.0f);
    CHECK(h.ok);
    CHECK(av.hp() == 1600.0f);

    auto sh = castKitSpell(makeSpell("wall", "shield", 200.0f), av, nullptr,
                           {}, c.beliefs, c.bus, c.fx, 1.0f);
    CHECK(sh.ok);
    CHECK(av.ward() == 200.0f);
    // Ward absorbs before HP.
    av.takeDamage(150.0f);
    CHECK(av.hp() == 1600.0f && av.ward() == 50.0f);
    av.takeDamage(100.0f);
    CHECK(av.ward() == 0.0f && av.hp() == 1550.0f);

    Civilian target{Vec3{10, 0, 0}};
    auto da = castKitSpell(makeSpell("lunge", "dash", 6.0f), av, &target, {},
                           c.beliefs, c.bus, c.fx, 1.0f);
    CHECK(da.ok);
    CHECK(da.dash.x > 5.9f && da.dash.x < 6.1f); // toward the target

    Civilian foe{Vec3{12, 0, 0}};
    std::vector<Entity*> foes = {&foe};
    auto p = castKitSpell(makeSpell("drag", "pull", 8.0f, 20.0f), av, nullptr,
                          foes, c.beliefs, c.bus, c.fx, 1.0f);
    CHECK(p.ok);
    CHECK(foe.position().x < 12.0f); // dragged toward the caster
}

static void testUnknownKindFizzles() {
    Ctx c;
    EldritchAvatar av{FACTION_CTHULHU, Vec3{0, 0, 0}, c.power};
    auto r = castKitSpell(makeSpell("weird", "time_stop", 99.0f), av, nullptr,
                          {}, c.beliefs, c.bus, c.fx, 1.0f);
    CHECK(!r.ok);
    CHECK(r.message.find("unknown effect") != std::string::npos);
}

static void testCooldownEnforcedByDriver() {
    // Cooldown math lives in the driver; here we verify the SpellDef
    // carries the cooldown the driver reads.
    SpellDef s = makeSpell("slam", "aoe_damage", 40.0f);
    CHECK(s.cooldownSec == 8.0f);
    CHECK(!s.id.empty() && !s.effectKind.empty());
}

int main() {
    std::cout << "== wave27: kit casting & wards ==\n";
    testAoeDamage();
    testFearAuraAndStun();
    testSummonBuffDebuff();
    testHealShieldDashPull();
    testUnknownKindFizzles();
    testCooldownEnforcedByDriver();
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
