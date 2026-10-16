// CULT-ULHU wave 28 tests: obedience preview, revolt with teeth,
// named/capped relics, bulk rest.

#include "commands/CommandSystem.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "cult/CultManager.h"
#include "dreams/DreamSystem.h"
#include "power/PowerSystem.h"
#include "relics/RelicSystem.h"

#include <iostream>
#include <string>
#include <vector>

using namespace cultulhu;

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

static void testObediencePreview() {
    EventBus bus;
    GameClock clock;
    RNG rng{3};
    BeliefSystem beliefs{bus, clock};
    CultManager cult{bus, clock, rng};
    CommandSystem cmds{bus, rng, beliefs, cult};

    for (int i = 0; i < 4; ++i) cult.recruit();

    ObediencePreview pv =
        cmds.previewObedience(DirectiveType::RaidCity, Vec3{10, 0, 10});
    CHECK(pv.chance >= 0.05f && pv.chance <= 0.95f);
    CHECK(!pv.reasons.empty());

    // Crushed devotion -> low chance + a reason saying so.
    for (size_t i = 0; i < cult.size(); ++i) cult.at(i).setDevotion(5.0f);
    ObediencePreview low =
        cmds.previewObedience(DirectiveType::RaidCity, Vec3{10, 0, 10});
    CHECK(low.chance < pv.chance);
    bool found = false;
    for (const auto& r : low.reasons)
        if (r == "devotion is low") found = true;
    CHECK(found);

    // No cultists: chance 0 with an explanatory reason.
    cult.clear();
    ObediencePreview none =
        cmds.previewObedience(DirectiveType::RaidCity, Vec3{0, 0, 0});
    CHECK(none.chance == 0.0f);
    CHECK(!none.reasons.empty());
}

static void testRevoltHasTeeth() {
    EventBus bus;
    GameClock clock;
    RNG rng{11};
    BeliefSystem beliefs{bus, clock};
    CultManager cult{bus, clock, rng};

    int revolts = 0;
    int lost = 0;
    bus.subscribe(EventType::Revolt,
                  [&](const GameEvent&) { ++revolts; });
    bus.subscribe(EventType::CultistLost, [&](const GameEvent&) { ++lost; });

    for (int i = 0; i < 8; ++i) {
        Cultist& c = cult.recruit();
        c.setDevotion(10.0f + i * 10.0f); // varied devotion
    }
    cult.addRisk(100.0f);
    const size_t before = cult.size();
    cult.update(1.0);
    CHECK(revolts == 1);
    CHECK(cult.size() < before);      // deserters are gone
    CHECK(lost > 0);                  // each published CultistLost
    CHECK(cult.insurrectionRisk() < 80.0f); // risk reset as before
}

static void testNamedCappedRelics() {
    EventBus bus;
    RelicSystem relics{bus};

    relics.seizeRelic(0.25f, 1, "the Chalice of Embers");
    relics.seizeRelic(0.50f, 1, "the Drowned Bell");
    CHECK(relics.heldRelics().size() == 2);
    CHECK(relics.heldRelics()[0].name == "the Chalice of Embers");
    CHECK(relics.powerMultiplier() == 1.75f);

    // Uncapped stacking is gone: 10 more big relics still cap at 3x.
    for (int i = 0; i < 10; ++i) relics.addRelic(1.0f, "relic");
    CHECK(relics.powerMultiplier() == RelicSystem::MAX_MULTIPLIER);
    CHECK(relics.powerMultiplier() == 3.0f);

    relics.removeRelic(0.25f);
    CHECK(relics.heldRelics().size() == 11);

    // Unnamed relics get a fallback name, never an empty string.
    relics.addRelic(0.1f);
    CHECK(!relics.heldRelics().back().name.empty());
}

static void testBulkRest() {
    EventBus bus;
    GameClock clock;
    RNG rng{5};
    BeliefSystem beliefs{bus, clock};
    CultManager cult{bus, clock, rng};
    DreamSystem dreams{bus, rng, beliefs, cult};

    for (int i = 0; i < 4; ++i) cult.recruit();
    CHECK(dreams.restingCount() == 0);

    // Bulk rest: all four dream at once.
    for (size_t i = 0; i < cult.size(); ++i)
        dreams.startRest(cult.at(i).id());
    CHECK(dreams.restingCount() == 4);
    CHECK(dreams.powerPerSec() > 0.0f);

    // Bulk wake.
    for (size_t i = 0; i < cult.size(); ++i)
        dreams.endRest(cult.at(i).id());
    CHECK(dreams.restingCount() == 0);
}

int main() {
    std::cout << "== wave28: obedience, revolt, relics, rest ==\n";
    testObediencePreview();
    testRevoltHasTeeth();
    testNamedCappedRelics();
    testBulkRest();
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
