// CULT-ULHU wave 29 tests: breeding rework (lower feral chance, skill
// mitigation, naming), lunatic Chaos visibility is driver-side.

#include "breeding/BreedingSystem.h"
#include "core/EventBus.h"
#include "core/RNG.h"

#include <cmath>
#include <iostream>
#include <string>

using namespace cultulhu;

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

static void testFeralChanceRework() {
    // Base rate dropped from 35% to 15%.
    CHECK(BreedingSystem::DEFAULT_FERAL_CHANCE == 0.15f);

    EventBus bus;
    RNG rng{9};
    BreedingSystem b{bus, rng};
    b.setBreedingActive(true);
    CHECK(b.feralChance() == 0.15f);

    // Skill halves the effective chance.
    b.setBreedingSkill(0.0f);
    CHECK(b.effectiveFeralChance() == 0.15f);
    b.setBreedingSkill(1.0f);
    CHECK(b.effectiveFeralChance() == 0.075f);
    b.setBreedingSkill(0.5f);
    CHECK(std::abs(b.effectiveFeralChance() - 0.1125f) < 1e-6f);
    // Clamped.
    b.setBreedingSkill(5.0f);
    CHECK(b.effectiveFeralChance() == 0.075f);
    b.setBreedingSkill(-1.0f);
    CHECK(b.effectiveFeralChance() == 0.15f);
}

static void testNamingAndEvents() {
    EventBus bus;
    RNG rng{21};
    BreedingSystem b{bus, rng};
    b.setBreedingActive(true);

    std::string bredTag;
    bus.subscribe(EventType::MonstrosityBred,
                  [&](const GameEvent& e) { bredTag = e.tag; });

    auto m = b.breed(Species::Human, Species::DeepOne, FACTION_CTHULHU,
                     Vec3{0, 0, 0}, "Innsmouth's Pride");
    CHECK(m != nullptr);
    CHECK(m->species() == "Innsmouth's Pride");
    CHECK(bredTag == "Innsmouth's Pride");

    // Unnamed births keep the classic hybrid label.
    auto m2 = b.breed(Species::Ghoul, Species::Beast, FACTION_CTHULHU,
                      Vec3{0, 0, 0});
    CHECK(m2 != nullptr);
    CHECK(m2->species() == "Ghoul-Beast hybrid");
    CHECK(bredTag == "Ghoul-Beast hybrid");

    // Inactive belief or incompatible species: no birth.
    b.setBreedingActive(false);
    CHECK(b.breed(Species::Human, Species::DeepOne, FACTION_CTHULHU,
                  Vec3{}) == nullptr);
    b.setBreedingActive(true);
    CHECK(b.breed(Species::Human, Species::Human, FACTION_CTHULHU,
                  Vec3{}) != nullptr); // same species always compatible
}

static void testFeralStatistical() {
    // Rough statistical check: with skill 1.0 the feral rate should be
    // well under the old 35% (effective 7.5%).
    EventBus bus;
    RNG rng{12345};
    BreedingSystem b{bus, rng};
    b.setBreedingActive(true);
    b.setBreedingSkill(1.0f);
    int feral = 0;
    const int N = 400;
    for (int i = 0; i < N; ++i) {
        auto m = b.breed(Species::Human, Species::Beast, FACTION_CTHULHU,
                         Vec3{0, 0, 0});
        if (m && m->feral()) ++feral;
    }
    const float rate = static_cast<float>(feral) / N;
    CHECK(rate < 0.20f); // effective 7.5% ± noise; old 35% would fail this
    CHECK(rate > 0.0f);  // feral births still happen
}

int main() {
    std::cout << "== wave29: breeding rework ==\n";
    testFeralChanceRework();
    testNamingAndEvents();
    testFeralStatistical();
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
