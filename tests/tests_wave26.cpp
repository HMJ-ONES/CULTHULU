// CULT-ULHU wave 26 tests: the Discovery Codex — first finds are logged,
// named, rewarded, and persisted.

#include "achievements/AchievementSystem.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "discovery/DiscoveryCodex.h"
#include "discovery/RelicNames.h"
#include "modes/FreeRoamMode.h"
#include "save/SaveSystem.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace cultulhu;

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

struct EventTap {
    std::vector<GameEvent> events;
    void attach(EventBus& bus, EventType t) {
        bus.subscribe(t, [this](const GameEvent& e) { events.push_back(e); });
    }
};

static void testFirstDiscovery() {
    EventBus bus;
    EventTap tap;
    tap.attach(bus, EventType::DiscoveryMade);
    DiscoveryCodex codex{bus};

    const bool first = codex.discover(
        DiscoveryKind::Landmark, "The Shattered Court", "The Shattered Court",
        "Where the old court fell.", Vec3{1, 0, 2}, 100.0, false);
    CHECK(first);
    CHECK(codex.count() == 1);
    CHECK(codex.countKind(DiscoveryKind::Landmark) == 1);
    CHECK(codex.countKind(DiscoveryKind::Species) == 0);

    // Repeat discovery: no record, no event.
    const bool again = codex.discover(
        DiscoveryKind::Landmark, "The Shattered Court", "The Shattered Court",
        "Where the old court fell.", Vec3{1, 0, 2}, 200.0, false);
    CHECK(!again);
    CHECK(codex.count() == 1);
    CHECK(tap.events.size() == 1);

    const GameEvent& e = tap.events[0];
    CHECK(e.tag == "landmark:the_shattered_court");
    CHECK(e.amount == DiscoveryCodex::powerReward(false));
    CHECK(e.faction == 0);
    CHECK(e.pos.x == 1.0f && e.pos.z == 2.0f);

    const Discovery* d = codex.find("landmark:the_shattered_court");
    CHECK(d != nullptr);
    CHECK(d->name == "The Shattered Court");
    CHECK(!d->renamed);
    CHECK(codex.find("landmark:nope") == nullptr);
}

static void testNightDiscoveryPaysMore() {
    EventBus bus;
    EventTap tap;
    tap.attach(bus, EventType::DiscoveryMade);
    DiscoveryCodex codex{bus};
    CHECK(DiscoveryCodex::powerReward(false) == 15.0f);
    CHECK(DiscoveryCodex::powerReward(true) == 25.0f);
    CHECK(codex.discover(DiscoveryKind::Species, "dhole", "Dhole",
                         "All hunger, no eyes.", Vec3{}, 50.0, true));
    CHECK(tap.events.size() == 1);
    CHECK(tap.events[0].faction == 1); // night flag rides along
    CHECK(codex.find("species:dhole")->night);
}

static void testRename() {
    EventBus bus;
    DiscoveryCodex codex{bus};
    codex.discover(DiscoveryKind::Relic, "the Chalice of Embers",
                   "the Chalice of Embers", "Warm.", Vec3{}, 10.0, false);
    CHECK(codex.rename("relic:the_chalice_of_embers", "Bob's Cup"));
    const Discovery* d = codex.find("relic:the_chalice_of_embers");
    CHECK(d != nullptr && d->name == "Bob's Cup" && d->renamed);
    CHECK(!codex.rename("relic:missing", "X"));
    CHECK(!codex.rename("relic:the_chalice_of_embers", ""));
}

static void testCodexSaveLoad() {
    EventBus bus;
    DiscoveryCodex codex{bus};
    codex.discover(DiscoveryKind::Landmark, "Hollow", "Hollow of Whispers",
                   "Do not answer.", Vec3{3, 0, 4}, 77.0, true);
    codex.rename("landmark:hollow", "My Hollow");

    GameState s;
    codex.saveTo(s);
    CHECK(s.discoveries.size() == 1);

    DiscoveryCodex codex2{bus};
    codex2.loadFrom(s);
    CHECK(codex2.count() == 1);
    const Discovery* d = codex2.find("landmark:hollow");
    CHECK(d != nullptr);
    CHECK(d->name == "My Hollow" && d->renamed && d->night);
    CHECK(d->flavor == "Do not answer.");
    CHECK(d->pos.x == 3.0f && d->pos.z == 4.0f);
    CHECK(d->gameTime == 77.0);

    // Re-discovering after load is still a duplicate.
    EventTap tap;
    tap.attach(bus, EventType::DiscoveryMade);
    CHECK(!codex2.discover(DiscoveryKind::Landmark, "Hollow",
                           "Hollow of Whispers", "Do not answer.", Vec3{3, 0, 4},
                           99.0, false));
    CHECK(tap.events.empty());
}

static void testSaveFileRoundTrip() {
    EventBus bus;
    DiscoveryCodex codex{bus};
    // Names with tricky characters exercise the save escaping.
    codex.discover(DiscoveryKind::Relic, "weird", "The \"Odd\"\tRelic",
                   "Line one.\nLine two. \\ backslash", Vec3{1, 2, 3}, 5.0,
                   false);
    GameState s;
    codex.saveTo(s);
    const std::string path = "/tmp/w26_codex_save.txt";
    CHECK(SaveSystem::save(s, path));
    GameState s2;
    CHECK(SaveSystem::load(path, s2));
    CHECK(s2.discoveries.size() == 1);
    DiscoveryCodex codex2{bus};
    codex2.loadFrom(s2);
    const Discovery* d = codex2.find("relic:weird");
    CHECK(d != nullptr);
    CHECK(d->name == "The \"Odd\"\tRelic");
    CHECK(d->flavor == "Line one.\nLine two. \\ backslash");
    std::remove(path.c_str());
}

static void testRelicNames() {
    RNG a{42}, b{42}, c{7};
    CHECK(generateRelicName(a) == generateRelicName(b)); // deterministic
    const std::string n1 = generateRelicName(a);
    const std::string n2 = generateRelicName(c);
    CHECK(!n1.empty() && n1.rfind("the ", 0) == 0);
    // 50 draws should show variety (12 forms x 12 epithets).
    std::vector<std::string> seen;
    RNG r{1234};
    for (int i = 0; i < 50; ++i) seen.push_back(generateRelicName(r));
    size_t uniq = 0;
    for (size_t i = 0; i < seen.size(); ++i) {
        bool dup = false;
        for (size_t j = 0; j < i; ++j)
            if (seen[j] == seen[i]) { dup = true; break; }
        if (!dup) ++uniq;
    }
    CHECK(uniq > 20);
    CHECK(!speciesFlavor("dhole").empty());
    CHECK(prettySpecies("pale_wight") == "Pale Wight");
}

static void testLandmarksAndRelicClaim() {
    EventBus bus;
    GameClock clock;
    RNG rng{99};
    FreeRoamMode mode{bus, clock, rng};
    mode.addLandmark("Test Tor", Vec3{10, 0, 10}, 5.0f, "A testing place.");
    CHECK(mode.landmarks().size() == 1);
    CHECK(mode.landmarks()[0].name == "Test Tor");

    mode.addRelicSpawn(Vec3{20, 0, 20}, 0.25f, "the Test Chalice");
    CHECK(mode.relics().size() == 1);
    CHECK(mode.relics()[0]->name() == "the Test Chalice");

    float amp = 0.0f;
    std::string name;
    CHECK(!mode.claimRelicNear(Vec3{0, 0, 0}, 6.0f, amp, name)); // too far
    CHECK(mode.claimRelicNear(Vec3{20, 0, 20}, 6.0f, amp, name));
    CHECK(amp == 0.25f);
    CHECK(name == "the Test Chalice");
    CHECK(mode.relics().empty());
}

static void testDiscoveryAchievements() {
    EventBus bus;
    AchievementSystem ach{bus};
    DiscoveryCodex codex{bus};

    CHECK(!ach.isUnlocked("first_wonder"));
    codex.discover(DiscoveryKind::Zone, "graveyard", "Graveyard", "Quiet.",
                   Vec3{}, 1.0, false);
    CHECK(ach.isUnlocked("first_wonder"));
    CHECK(!ach.isUnlocked("night_pilgrim"));
    codex.discover(DiscoveryKind::Zone, "chapel", "Chapel", "Hushed.", Vec3{},
                   2.0, true);
    CHECK(ach.isUnlocked("night_pilgrim"));
    // Progress reporting reflects the discovery count.
    auto pr = ach.progress("cartographer"); // target 10 discoveries
    CHECK(pr.first == 2.0 && pr.second == 10.0);

    // Persistence of the new counters through save/load.
    GameState s;
    ach.saveTo(s);
    AchievementSystem ach2{bus};
    ach2.loadFrom(s);
    CHECK(ach2.isUnlocked("first_wonder"));
    auto pr2 = ach2.progress("cartographer");
    CHECK(pr2.first == 2.0 && pr2.second == 10.0);
}

int main() {
    std::cout << "== wave26: discovery codex ==\n";
    testFirstDiscovery();
    testNightDiscoveryPaysMore();
    testRename();
    testCodexSaveLoad();
    testSaveFileRoundTrip();
    testRelicNames();
    testLandmarksAndRelicClaim();
    testDiscoveryAchievements();
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
