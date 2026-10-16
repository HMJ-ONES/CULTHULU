// CULT-ULHU wave-2 tests: save/load, breeding, traps, AI, war, relics,
// expanded MOBA. Run via ctest or directly.

#include "ai/AI.h"
#include "beliefs/BeliefSystem.h"
#include "breeding/BreedingSystem.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "entities/Structures.h"
#include "entities/Units.h"
#include "modes/MobaDefense.h"
#include "relics/RelicSystem.h"
#include "save/SaveSystem.h"
#include "traps/TrapSystem.h"
#include "war/WarSystem.h"

#include <cmath>
#include <cstdio>
#include <iostream>

using namespace cultulhu;

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

#define CHECK_CLOSE(a, b, eps) do { ++checks; if (std::fabs((a) - (b)) > (eps)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #a " (" << (a) \
    << ") != " #b " (" << (b) << ")\n"; } } while (0)

static void adoptNow(BeliefSystem& bs, Belief b) {
    CHECK(bs.requestChange(b, Belief::Count));
    bs.update(BeliefSystem::ADOPTION_TIME + 1.0);
    CHECK(bs.isActive(b));
}

static void test_save_load() {
    GameState s;
    s.clockTime = 123.5;
    s.power = 250.0f;
    s.activeBeliefs = {Belief::Torture, Belief::War};
    s.insurrectionRisk = 12.5f;
    GameState::EntityRec e1;
    e1.id = 7; e1.type = 1; e1.faction = 0;
    e1.pos = Vec3(10, 0, 5); e1.hp = 80; e1.maxHp = 100;
    GameState::EntityRec e2;
    e2.id = 9; e2.type = 8; e2.faction = 1;
    e2.pos = Vec3(-3, 1, 2); e2.hp = 1000; e2.maxHp = 1000;
    s.entities = {e1, e2};

    const char* path = "/tmp/cultulhu_wave2_save.txt";
    CHECK(SaveSystem::save(s, path));

    GameState out;
    CHECK(SaveSystem::load(path, out));
    CHECK_CLOSE(out.clockTime, 123.5, 0.001);
    CHECK_CLOSE(out.power, 250.0f, 0.001f);
    CHECK(out.activeBeliefs.size() == 2);
    CHECK(out.activeBeliefs[0] == Belief::Torture);
    CHECK(out.activeBeliefs[1] == Belief::War);
    CHECK_CLOSE(out.insurrectionRisk, 12.5f, 0.001f);
    CHECK(out.entities.size() == 2);
    CHECK(out.entities[0].id == 7);
    CHECK_CLOSE(out.entities[0].pos.x, 10.0f, 0.001f);
    CHECK_CLOSE(out.entities[1].pos.z, 2.0f, 0.001f);

    // Missing file -> false, no crash.
    GameState junk;
    CHECK(!SaveSystem::load("/tmp/cultulhu_nope_missing.txt", junk));
    std::remove(path);
}

static void test_breeding() {
    EventBus bus; RNG rng(42);
    BreedingSystem br(bus, rng);

    int bred = 0, rampages = 0;
    bus.subscribe(EventType::MonstrosityBred, [&](const GameEvent&) { ++bred; });
    bus.subscribe(EventType::FeralRampage, [&](const GameEvent&) { ++rampages; });

    // Inactive belief: no breeding.
    CHECK(br.breed(Species::Human, Species::DeepOne, FACTION_CTHULHU, Vec3()) == nullptr);

    br.setBreedingActive(true);
    // Incompatible pair: no breeding.
    CHECK(br.breed(Species::Human, Species::Ghoul, FACTION_CTHULHU, Vec3()) == nullptr);
    CHECK(bred == 0);

    // Compatible pair: monstrosity born.
    auto m = br.breed(Species::Human, Species::DeepOne, FACTION_CTHULHU, Vec3(1, 0, 0));
    CHECK(m != nullptr);
    CHECK(bred == 1);
    CHECK(m->faction() == FACTION_CTHULHU);
    CHECK(m->type() == EntityType::Monstrosity);

    // Forced feral / forced tame.
    br.setFeralChance(1.0f);
    auto f = br.breed(Species::Beast, Species::Horror, FACTION_CTHULHU, Vec3());
    CHECK(f && f->feral());
    CHECK(rampages == 1);
    br.setFeralChance(0.0f);
    auto t = br.breed(Species::Beast, Species::Horror, FACTION_CTHULHU, Vec3());
    CHECK(t && !t->feral());
    CHECK(rampages == 1); // no new rampage

    // Compatibility matrix spot checks.
    CHECK(BreedingSystem::compatible(Species::Ghoul, Species::Ghoul));
    CHECK(BreedingSystem::compatible(Species::DeepOne, Species::Horror));
    CHECK(!BreedingSystem::compatible(Species::Human, Species::Horror));
}

static void test_traps() {
    EventBus bus;
    TrapSystem traps(bus);
    int sprung = 0;
    bus.subscribe(EventType::TrapSprung, [&](const GameEvent&) { ++sprung; });

    uint64_t id = traps.placeTrap(Vec3(5, 0, 5));
    CHECK(traps.get(id) != nullptr);
    CHECK(traps.get(id)->armed);

    Civilian civ(Vec3(5, 0, 5));
    CHECK(traps.springTrap(id, civ));   // civilian captured
    CHECK(sprung == 1);
    CHECK(!traps.get(id)->armed);      // single-use
    CHECK(traps.get(id)->captured == 1);
    CHECK(!traps.springTrap(id, civ)); // disarmed: no double-trigger

    uint64_t id2 = traps.placeTrap(Vec3());
    Cultist cult(FACTION_CTHULHU, Vec3()); // cultists can't be trap victims
    CHECK(!traps.springTrap(id2, cult));
    CHECK(sprung == 1);
}

static void test_ai() {
    Cultist c(FACTION_CTHULHU, Vec3(0, 0, 0));
    Vec3 leader(10, 0, 0);
    float d0 = c.position().distance(leader);
    ai::cultistFollow(c, leader, 1.0); // speed 4
    CHECK(c.position().distance(leader) < d0);

    // Imprisoned cultists ignore orders.
    c.setPosition(Vec3(0, 0, 0));
    c.setState(CultistState::Imprisoned);
    ai::cultistFollow(c, leader, 1.0);
    CHECK_CLOSE(c.position().x, 0.0f, 0.001f);

    Civilian civ(Vec3(0, 0, 0));
    Vec3 threat(1, 0, 0);
    ai::civilianFlee(civ, threat, 1.0); // speed 5
    CHECK(civ.position().distance(threat) > 1.0f);

    // Rampage targeting: nearest Cthulhu-faction entity.
    Monstrosity m(FACTION_CTHULHU, Vec3(0, 0, 0), "hybrid", true);
    Cultist near(FACTION_CTHULHU, Vec3(5, 0, 0));
    Cultist far(FACTION_CTHULHU, Vec3(50, 0, 0));
    Civilian neutral(Vec3(2, 0, 0)); // not Cthulhu faction: ignored
    std::vector<const Entity*> ents = {&near, &far, &neutral};
    const Entity* tgt = ai::selectRampageTarget(m, ents);
    CHECK(tgt == &near);

    float md0 = m.position().distance(near.position());
    ai::rampageStep(m, ents, 1.0);
    CHECK(m.position().distance(near.position()) < md0);
}

static void test_war() {
    EventBus bus; GameClock clock; RNG rng(21);
    WarSystem war(bus);
    BeliefSystem bs(bus, clock);
    adoptNow(bs, Belief::War);

    int slainEvents = 0;
    bus.subscribe(EventType::EnemyCultistSlain, [&](const GameEvent&) { ++slainEvents; });

    war.declareWar(1);
    war.declareWar(2);
    CHECK(war.warCount() == 2);
    CHECK(war.atWar(1));
    war.syncBeliefs(bs);
    CHECK(bs.activeWars() == 2);

    war.recordKill(1);
    CHECK(slainEvents == 1);
    CHECK(war.killsAgainst(1) == 1);
    war.recordKill(3); // not at war: ignored
    CHECK(slainEvents == 1);

    // Power scaling through the belief system: single war pays more.
    GameEvent kill(EventType::EnemyCultistSlain);
    war.makePeace(2);
    war.syncBeliefs(bs);
    float single = bs.onEvent(kill);
    war.declareWar(2);
    war.syncBeliefs(bs);
    float multi = bs.onEvent(kill);
    CHECK(single > multi);
    CHECK(multi > 0.0f);
}

static void test_relics() {
    EventBus bus;
    RelicSystem relics(bus);
    CHECK_CLOSE(relics.powerMultiplier(), 1.0f, 0.001f);
    relics.addRelic(0.25f);
    relics.addRelic(0.10f);
    CHECK_CLOSE(relics.powerMultiplier(), 1.35f, 0.001f);
    CHECK_CLOSE(relics.applyAmplifier(100.0f), 135.0f, 0.01f);
    relics.removeRelic(0.25f);
    CHECK_CLOSE(relics.powerMultiplier(), 1.10f, 0.001f);

    int triggered = 0;
    bus.subscribe(EventType::ArtifactTriggered, [&](const GameEvent&) { ++triggered; });
    uint64_t id = relics.placeCursedArtifact(Vec3(0, 0, 0), FACTION_CTHULHU);
    Civilian enemy(Vec3(3, 0, 0)); // neutral counts as "other faction"
    Cultist friend_(FACTION_CTHULHU, Vec3(3, 0, 0));
    std::vector<const Entity*> ents = {&friend_};
    CHECK(!relics.checkTrigger(id, ents)); // friendlies don't trigger
    ents = {&enemy};
    CHECK(relics.checkTrigger(id, ents));  // enemy in radius: sprung
    CHECK(triggered == 1);
    CHECK(!relics.checkTrigger(id, ents)); // single-use
}

static void test_trickery_trap_power() {
    EventBus bus; GameClock clock; RNG rng(22);
    BeliefSystem bs(bus, clock);
    adoptNow(bs, Belief::Trickery);
    GameEvent e(EventType::TrapSprung);
    CHECK_CLOSE(bs.onEvent(e), 6.0f, 0.001f);
}

static void test_moba_expanded() {
    EventBus bus; GameClock clock; RNG rng(23);
    MobaDefense moba(bus, clock, rng);
    moba.addLane({Vec3(0, 0, 0), Vec3(50, 0, 0), Vec3(100, 0, 0)});
    moba.setBase(0, Vec3(0, 0, 0), 2000.0f);
    moba.setBase(1, Vec3(100, 0, 0), 2000.0f);
    moba.update(0.1); // builds towers
    CHECK(moba.towerCount() == 4); // 2 teams x 2 towers x 1 lane

    // Wave composition: 3rd wave is a siege wave.
    moba.spawnWave(0, 0); // wave 1: 4
    moba.spawnWave(0, 0); // wave 2: 4
    moba.spawnWave(0, 0); // wave 3: 4 + siege
    CHECK(moba.minionCount() == 13);
    int siege = 0, melee = 0, ranged = 0;
    for (const auto& m : moba.minions()) {
        if (m.kind == "siege") ++siege;
        else if (m.kind == "melee") ++melee;
        else if (m.kind == "ranged") ++ranged;
    }
    CHECK(siege == 1 && melee == 9 && ranged == 3);

    // Unopposed minions march down the lane and damage enemy towers.
    // (Stepped in small ticks: the sim resolves one action per update.)
    float towerBefore = moba.towerHp(1, 0, 0);
    CHECK(towerBefore > 0.0f);
    for (int i = 0; i < 50; ++i) moba.update(0.5); // 25s, under the 30s wave timer
    CHECK(moba.towerHp(1, 0, 0) < towerBefore);
    CHECK(!moba.isOver());
}

int main() {
    test_save_load();
    test_breeding();
    test_traps();
    test_ai();
    test_war();
    test_relics();
    test_trickery_trap_power();
    test_moba_expanded();

    std::cout << "checks: " << checks << ", failures: " << failures << "\n";
    if (failures == 0) std::cout << "ALL WAVE-2 TESTS PASSED\n";
    return failures == 0 ? 0 : 1;
}
