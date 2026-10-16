// CULT-ULHU wave 15 tests: five new ambient activities (DreamSharing,
// MendEffigy, WhisperCampaign, BloodRite, WildsHunt) and two new
// directives (OneiricHarvest, RebuildSanctum) — full lifecycles through
// the CommandSystem/DirectiveExecutor/exertion/power pipeline.
// Covers:
//   - new ambient action names, event payloads, exertion feeds,
//     BeliefSystem reactions, weight plausibility
//   - new directive names, obedience feeds, operation lifecycles
//     (tick events, completion events, power, devotion), partial +
//     refused paths, degrade-without-context paths
//   - legal-name scan of every new user-facing string
// Compile manually (CMakeLists.txt wires it as cultulhu_tests_wave15):
//   g++ -std=c++17 -Isrc tests/tests_wave15.cpp build/libcultulhu.a \
//       -o /tmp/wave15_test && /tmp/wave15_test

#include "ai/AmbientBehavior.h"
#include "beliefs/Belief.h"
#include "beliefs/BeliefSystem.h"
#include "commands/CommandSystem.h"
#include "commands/DirectiveExecutor.h"
#include "core/EventBus.h"
#include "core/Events.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "cult/CultManager.h"
#include "entities/Units.h"
#include "exertion/ExertionSystem.h"
#include "power/PowerSystem.h"
#include "world/WorldMap.h"
#include "world/Zone.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace cultulhu;

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

#define CHECK_CLOSE(a, b, eps) do { ++checks; if (std::fabs((a) - (b)) > (eps)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #a " (" << (a) \
    << ") != " #b " (" << (b) << ")\n"; } } while (0)

struct Recorder {
    std::vector<GameEvent> events;
    void attach(EventBus& bus, EventType t) {
        bus.subscribe(t, [this](const GameEvent& e) { events.push_back(e); });
    }
    size_t count(EventType t) const {
        size_t n = 0;
        for (const auto& e : events)
            if (e.type == t) ++n;
        return n;
    }
    const GameEvent& last(EventType t) const {
        for (size_t i = events.size(); i-- > 0;)
            if (events[i].type == t) return events[i];
        static GameEvent none;
        return none;
    }
};

static void adoptNow(BeliefSystem& bs, Belief b) {
    CHECK(bs.requestChange(b, Belief::Count));
    bs.update(BeliefSystem::ADOPTION_TIME + 1.0);
    CHECK(bs.isActive(b));
}

struct World {
    EventBus bus;
    GameClock clock;
    RNG rng{1234};
    PowerSystem power;
    BeliefSystem beliefs{bus, clock};
    CultManager cult{bus, clock, rng};
    ExertionSystem exertion{bus, beliefs, power, cult, rng};
    AmbientDirector ambient{bus, rng, beliefs, cult, 30.0};
    CommandSystem commands{bus, rng, beliefs, cult};
    DirectiveExecutor executor{bus, rng, cult};
    WorldMap map{"test"};
    Recorder rec;

    explicit World(uint64_t seed = 1234) : rng(seed) {
        ambient.setExertion(&exertion);
        DirectiveContext ctx;
        ctx.power = &power;
        ctx.exertion = &exertion;
        ctx.worldMap = &map;
        executor.setContext(ctx);
    }

    void attachAll() {
        rec.attach(bus, EventType::DreamShared);
        rec.attach(bus, EventType::DreamWhisper);
        rec.attach(bus, EventType::ConversionPerformed);
        rec.attach(bus, EventType::EffigyMended);
        rec.attach(bus, EventType::RumorSpread);
        rec.attach(bus, EventType::RiteOfFlesh);
        rec.attach(bus, EventType::WildsHunted);
        rec.attach(bus, EventType::SuppliesGathered);
        rec.attach(bus, EventType::OneiricHarvestCompleted);
        rec.attach(bus, EventType::SanctumRebuiltTick);
        rec.attach(bus, EventType::SanctumRebuilt);
        rec.attach(bus, EventType::BuildingRebuilt);
        rec.attach(bus, EventType::DirectiveCompleted);
    }
};

static GameEvent resolvedEvent(DirectiveType d, CommandOutcome o,
                               float chance = 0.8f,
                               Vec3 pos = Vec3{0, 0, 0}, int faction = 1) {
    GameEvent e(EventType::DirectiveResolved);
    e.tag = std::string(directiveName(d)) + "/" + commandOutcomeName(o);
    e.amount = chance;
    e.pos = pos;
    e.faction = faction;
    return e;
}

// ---- New ambient action names ----

static void test_ambient_action_names() {
    CHECK(std::string(ambientActionName(AmbientAction::DreamSharing)) ==
          "DreamSharing");
    CHECK(std::string(ambientActionName(AmbientAction::MendEffigy)) ==
          "MendEffigy");
    CHECK(std::string(ambientActionName(AmbientAction::WhisperCampaign)) ==
          "WhisperCampaign");
    CHECK(std::string(ambientActionName(AmbientAction::BloodRite)) ==
          "BloodRite");
    CHECK(std::string(ambientActionName(AmbientAction::WildsHunt)) ==
          "WildsHunt");
    // The new actions come before the Count sentinel.
    CHECK(static_cast<int>(AmbientAction::WildsHunt) <
          static_cast<int>(AmbientAction::Count));
    CHECK(static_cast<int>(AmbientAction::Count) == 16);
}

// ---- DreamSharing ----

static void test_dream_sharing_cold() {
    World w;
    w.attachAll();
    w.cult.recruit();
    const float d0 = w.exertion.exertion(Belief::Dreams);
    CHECK(w.ambient.forceAction(0, AmbientAction::DreamSharing));
    CHECK(w.rec.count(EventType::DreamShared) == 1);
    CHECK(w.rec.last(EventType::DreamShared).amount == 1.0f);
    // Dreams exertion feed: DreamShared -> Dreams +3 (inactive: half rate).
    CHECK(w.exertion.exertion(Belief::Dreams) > d0);
    // One cold share (Dreams exertion still < 50): no conversion.
    CHECK(w.rec.count(EventType::ConversionPerformed) == 0);
    CHECK(w.rec.count(EventType::DreamWhisper) == 0);
}

// Repeated sharing heats Dreams exertion past the 50 synergy gate, and
// conversions begin organically — the wave-10 design intent.
static void test_dream_sharing_heats_dreams() {
    World w(1234);
    w.attachAll();
    w.cult.recruit();
    bool converted = false;
    for (int i = 0; i < 120 && !converted; ++i) {
        w.ambient.forceAction(0, AmbientAction::DreamSharing);
        converted = w.rec.count(EventType::ConversionPerformed) > 0;
    }
    CHECK(converted);
    CHECK(w.exertion.exertion(Belief::Dreams) >= 50.0f);
}

static void test_dream_sharing_hot() {
    // Hot Dreams (>= 50 exertion): visions sharpen (amount 2) and a
    // distant civilian may convert via dreamshared whispers.
    World w(999);
    w.attachAll();
    w.cult.recruit();
    w.exertion.addExertion(Belief::Dreams, 200.0f); // top up well past 50
    bool converted = false;
    for (int i = 0; i < 80 && !converted; ++i) {
        w.ambient.forceAction(0, AmbientAction::DreamSharing);
        converted = w.rec.count(EventType::ConversionPerformed) > 0;
    }
    CHECK(converted); // 30% per share: 80 tries cannot all miss
    CHECK(w.rec.last(EventType::DreamWhisper).tag == "dreamshared");
    CHECK(w.rec.last(EventType::ConversionPerformed).tag == "dreamshared");
    CHECK(w.rec.last(EventType::DreamShared).amount == 2.0f);
}

// ---- MendEffigy ----

static void test_mend_effigy() {
    World w;
    w.attachAll();
    adoptNow(w.beliefs, Belief::Reconstruction);
    Cultist& c = w.cult.recruit();
    const float devotionBefore = c.devotion();
    const float r0 = w.exertion.exertion(Belief::Reconstruction);
    CHECK(w.ambient.forceAction(0, AmbientAction::MendEffigy));
    CHECK(w.rec.count(EventType::EffigyMended) == 1);
    CHECK(w.rec.last(EventType::EffigyMended).sourceId == c.id());
    CHECK_CLOSE(c.devotion(), devotionBefore + 2.0f, 1e-4f);
    // Reconstruction exertion feed: EffigyMended -> Reconstruction +2.
    CHECK(w.exertion.exertion(Belief::Reconstruction) > r0);
}

// ---- WhisperCampaign ----

static void test_whisper_campaign() {
    World w;
    w.attachAll();
    Cultist& c = w.cult.recruit();
    c.setPosition(Vec3{1.0f, 0.0f, 1.0f});
    ZoneDef def;
    def.name = "hamlet";
    def.min = Vec3{-10.0f, -10.0f, -10.0f};
    def.max = Vec3{10.0f, 10.0f, 10.0f};
    w.map.addZone(std::move(def));
    w.ambient.setWorldMap(&w.map);

    const float t0 = w.exertion.exertion(Belief::Trickery);
    CHECK(w.ambient.forceAction(0, AmbientAction::WhisperCampaign));
    CHECK(w.rec.count(EventType::RumorSpread) == 1);
    CHECK(w.rec.last(EventType::RumorSpread).sourceId == c.id());
    CHECK_CLOSE(w.rec.last(EventType::RumorSpread).pos.x, 1.0f, 1e-4f);
    // The hamlet's fear rises through the planted rumors.
    CHECK_CLOSE(w.map.zone(0).ambient().ambientFear, 4.0f, 1e-4f);
    // Trickery exertion feed: RumorSpread -> Trickery +3 (half rate
    // while Trickery is inactive).
    CHECK(w.exertion.exertion(Belief::Trickery) > t0);

    // A rumor that lands converts like a sermon (20% per campaign).
    bool converted = false;
    for (int i = 0; i < 80 && !converted; ++i) {
        w.ambient.forceAction(0, AmbientAction::WhisperCampaign);
        for (const auto& e : w.rec.events)
            if (e.type == EventType::ConversionPerformed &&
                e.tag == "rumor") {
                converted = true;
                break;
            }
    }
    CHECK(converted);
}

// ---- BloodRite ----

static void test_blood_rite() {
    World w(31337);
    w.attachAll();
    Cultist& c = w.cult.recruit();
    const float devotionBefore = c.devotion();
    const float t0 = w.exertion.exertion(Belief::Torture);
    CHECK(w.ambient.forceAction(0, AmbientAction::BloodRite));
    CHECK(w.rec.count(EventType::RiteOfFlesh) == 1);
    CHECK(w.rec.last(EventType::RiteOfFlesh).sourceId == c.id());
    CHECK_CLOSE(c.devotion(), devotionBefore + 2.0f, 1e-4f);
    // Torture exertion feed: RiteOfFlesh -> Torture +3 (half rate
    // while Torture is inactive).
    CHECK(w.exertion.exertion(Belief::Torture) > t0);

    // The rite sometimes draws more blood than intended (10% per rite):
    // enough rites must eventually wound the cultist.
    const float hpBefore = c.hp();
    for (int i = 0; i < 100 && c.hp() >= hpBefore; ++i)
        w.ambient.forceAction(0, AmbientAction::BloodRite);
    CHECK(c.hp() < hpBefore);
}

// ---- WildsHunt ----

static void test_wilds_hunt() {
    World w(424242);
    w.attachAll();
    Cultist& c = w.cult.recruit();
    const float o0 = w.exertion.exertion(Belief::Onslaught);
    CHECK(w.ambient.forceAction(0, AmbientAction::WildsHunt));
    CHECK(w.rec.count(EventType::WildsHunted) == 1);
    CHECK(w.rec.count(EventType::SuppliesGathered) == 1);
    CHECK(w.rec.last(EventType::SuppliesGathered).amount >= 1.0f);
    CHECK(w.rec.last(EventType::SuppliesGathered).amount <= 2.0f);
    // Onslaught exertion feed: WildsHunted -> Onslaught +2 (half rate
    // while Onslaught is inactive).
    CHECK(w.exertion.exertion(Belief::Onslaught) > o0);

    // The quarry fights back sometimes (15% per hunt).
    bool mauled = false;
    for (int i = 0; i < 100 && !mauled; ++i) {
        const float h = c.hp();
        w.ambient.forceAction(0, AmbientAction::WildsHunt);
        mauled = c.hp() < h;
    }
    CHECK(mauled);
}

// ---- Weighting plausibility for the new actions ----

static void test_new_action_weights() {
    // Dreams + Reconstruction + Trickery active (MAX_ACTIVE is 3), night:
    // the new belief-aligned actions must show up in weighted draws.
    World w(777);
    adoptNow(w.beliefs, Belief::Dreams);
    adoptNow(w.beliefs, Belief::Reconstruction);
    adoptNow(w.beliefs, Belief::Trickery);
    w.ambient.setHourOfDay(23.0); // night
    w.cult.recruit();
    Cultist& c = w.cult.at(0);

    int newActionPicks = 0;
    for (int i = 0; i < 2000; ++i) {
        AmbientAction a = chooseAmbientAction(c, w.beliefs, 0.9f, 23.0, w.rng);
        CHECK(a != AmbientAction::Count);
        if (static_cast<int>(a) >=
            static_cast<int>(AmbientAction::DreamSharing))
            ++newActionPicks;
    }
    // Base new-action weight ~23/141 with +33 belief/night bonus:
    // 2000 draws should see them hundreds of times.
    CHECK(newActionPicks > 100);
}

// ---- New directive names ----

static void test_new_directive_names() {
    CHECK(std::string(directiveName(DirectiveType::OneiricHarvest)) ==
          "OneiricHarvest");
    CHECK(std::string(directiveName(DirectiveType::RebuildSanctum)) ==
          "RebuildSanctum");
}

// ---- Directive exertion feeds ----

static void test_directive_exertion_feeds() {
    World w(99);
    w.attachAll();
    const float d0 = w.exertion.exertion(Belief::Dreams);
    const float r0 = w.exertion.exertion(Belief::Reconstruction);
    const float c0 = w.exertion.exertion(Belief::Chaos);
    w.bus.publish(
        resolvedEvent(DirectiveType::OneiricHarvest, CommandOutcome::Obeyed));
    w.bus.publish(
        resolvedEvent(DirectiveType::RebuildSanctum, CommandOutcome::Obeyed));
    w.bus.publish(
        resolvedEvent(DirectiveType::OneiricHarvest, CommandOutcome::Refused));
    CHECK(w.exertion.exertion(Belief::Dreams) > d0);
    CHECK(w.exertion.exertion(Belief::Reconstruction) > r0);
    CHECK(w.exertion.exertion(Belief::Chaos) > c0);
}

// ---- OneiricHarvest lifecycle ----

static void runDreamHarvest(World& w, CommandOutcome o) {
    w.bus.publish(resolvedEvent(DirectiveType::OneiricHarvest, o, 0.8f,
                                Vec3{5, 0, 5}, 1));
    for (int i = 0; i < 20 && w.executor.activeCount() > 0; ++i)
        w.executor.update(5.0);
}

static void test_oneiric_harvest() {
    World w(11);
    w.attachAll();
    for (int i = 0; i < 3; ++i) {
        Cultist& c = w.cult.recruit();
        c.setDevotion(90.0f);
    }
    const float powerBefore = w.power.value();
    runDreamHarvest(w, CommandOutcome::Obeyed);
    CHECK(w.executor.activeCount() == 0);
    // 60s / 10s ticks: six channeled visions.
    CHECK(w.rec.count(EventType::DreamShared) == 6);
    CHECK(w.rec.last(EventType::DreamShared).tag == "directive_dream");
    CHECK(w.rec.count(EventType::OneiricHarvestCompleted) == 1);
    CHECK(w.rec.last(EventType::OneiricHarvestCompleted).amount == 3.0f);
    CHECK(w.rec.last(EventType::OneiricHarvestCompleted).tag ==
          "OneiricHarvest");
    // Harvested dreams distilled into power: 3 dreamers x 3 power.
    CHECK_CLOSE(w.power.value(), powerBefore + 9.0f, 1e-4f);
}

static void test_oneiric_harvest_partial() {
    World w(12);
    w.attachAll();
    for (int i = 0; i < 4; ++i) w.cult.recruit();
    const float powerBefore = w.power.value();
    runDreamHarvest(w, CommandOutcome::PartiallyObeyed);
    CHECK(w.executor.activeCount() == 0);
    // Half duration: three ticks; half-magnitude power distillation.
    CHECK(w.rec.count(EventType::DreamShared) == 3);
    CHECK(w.rec.count(EventType::OneiricHarvestCompleted) == 1);
    CHECK_CLOSE(w.power.value(), powerBefore + 4.0f * 3.0f * 0.5f, 1e-4f);
}

static void test_oneiric_harvest_refused() {
    World w(13);
    w.attachAll();
    w.cult.recruit();
    runDreamHarvest(w, CommandOutcome::Refused);
    CHECK(w.rec.count(EventType::DreamShared) == 0);
    CHECK(w.rec.count(EventType::OneiricHarvestCompleted) == 0);
}

static void test_oneiric_harvest_no_power_ctx() {
    // Graceful degradation: without a PowerSystem the rite still
    // completes; the distillation step is skipped.
    World w(14);
    w.attachAll();
    DirectiveContext bare; // all null
    w.executor.setContext(bare);
    for (int i = 0; i < 3; ++i) w.cult.recruit();
    const float powerBefore = w.power.value();
    runDreamHarvest(w, CommandOutcome::Obeyed);
    CHECK(w.rec.count(EventType::OneiricHarvestCompleted) == 1);
    CHECK_CLOSE(w.power.value(), powerBefore, 1e-4f);
}

// ---- RebuildSanctum lifecycle ----

static void runRebuild(World& w, CommandOutcome o) {
    w.bus.publish(resolvedEvent(DirectiveType::RebuildSanctum, o, 0.8f,
                                Vec3{7, 0, 7}, 1));
    for (int i = 0; i < 20 && w.executor.activeCount() > 0; ++i)
        w.executor.update(5.0);
}

static void test_rebuild_sanctum() {
    World w(21);
    w.attachAll();
    for (int i = 0; i < 3; ++i) {
        Cultist& c = w.cult.recruit();
        c.setDevotion(80.0f);
    }
    const float r0 = w.exertion.exertion(Belief::Reconstruction);
    runRebuild(w, CommandOutcome::Obeyed);
    CHECK(w.executor.activeCount() == 0);
    // 60s / 5s ticks: twelve progress ticks.
    CHECK(w.rec.count(EventType::SanctumRebuiltTick) == 12);
    CHECK(w.rec.count(EventType::BuildingRebuilt) == 1);
    CHECK(w.rec.count(EventType::SanctumRebuilt) == 1);
    CHECK(w.rec.last(EventType::SanctumRebuilt).tag == "RebuildSanctum");
    // The shared labor steadies the cult: +5 devotion each.
    for (size_t i = 0; i < w.cult.size(); ++i)
        CHECK_CLOSE(w.cult.at(i).devotion(), 85.0f, 1e-4f);
    // Reconstruction exertion feeds: 12 ticks x 1.0 + completion 8.0
    // (inactive: half rate).
    CHECK(w.exertion.exertion(Belief::Reconstruction) > r0);
}

static void test_rebuild_sanctum_partial() {
    World w(22);
    w.attachAll();
    Cultist& c = w.cult.recruit();
    c.setDevotion(80.0f);
    runRebuild(w, CommandOutcome::PartiallyObeyed);
    CHECK(w.executor.activeCount() == 0);
    CHECK(w.rec.count(EventType::SanctumRebuiltTick) == 6);
    CHECK(w.rec.count(EventType::BuildingRebuilt) == 1);
    CHECK(w.rec.count(EventType::SanctumRebuilt) == 1);
    CHECK_CLOSE(w.cult.at(0).devotion(), 82.5f, 1e-4f);
}

static void test_rebuild_sanctum_refused() {
    World w(23);
    w.attachAll();
    w.cult.recruit();
    runRebuild(w, CommandOutcome::Refused);
    CHECK(w.rec.count(EventType::SanctumRebuiltTick) == 0);
    CHECK(w.rec.count(EventType::SanctumRebuilt) == 0);
    CHECK(w.rec.count(EventType::BuildingRebuilt) == 0);
}

// ---- End-to-end through issueCommand ----

static void test_issue_dream_end_to_end() {
    // A loyal cult obeys the dream-rite; the operation runs to a
    // DirectiveCompleted.
    int obeySeed = -1;
    for (int s = 1; s <= 40 && obeySeed < 0; ++s) {
        World w(static_cast<uint64_t>(s));
        for (int i = 0; i < 3; ++i) {
            Cultist& c = w.cult.recruit();
            c.setDevotion(95.0f);
        }
        CommandResult r =
            w.commands.issueCommand(DirectiveType::OneiricHarvest, Vec3{0, 0, 0});
        if (r.outcome == CommandOutcome::Obeyed) obeySeed = s;
    }
    CHECK(obeySeed > 0);
    if (obeySeed <= 0) return;

    World w(static_cast<uint64_t>(obeySeed));
    w.attachAll();
    for (int i = 0; i < 3; ++i) {
        Cultist& c = w.cult.recruit();
        c.setDevotion(95.0f);
    }
    Recorder done, issued;
    done.attach(w.bus, EventType::DirectiveCompleted);
    issued.attach(w.bus, EventType::DirectiveIssued);
    CommandResult r =
        w.commands.issueCommand(DirectiveType::OneiricHarvest, Vec3{0, 0, 0});
    CHECK(r.outcome == CommandOutcome::Obeyed);
    CHECK(issued.events.size() == 1);
    CHECK(std::string(issued.events[0].tag) == "OneiricHarvest");
    CHECK(w.executor.activeCount() == 1);
    for (int i = 0; i < 20 && w.executor.activeCount() > 0; ++i)
        w.executor.update(5.0);
    CHECK(done.events.size() == 1);
    CHECK(done.events[0].tag == "OneiricHarvest");
    CHECK(w.rec.count(EventType::OneiricHarvestCompleted) == 1);
}

static void test_issue_rebuild_end_to_end() {
    // A resolution the executor does not carry out (SparksInsurrection)
    // spawns no operation.
    World w(555);
    w.attachAll();
    w.cult.recruit();
    w.bus.publish(
        resolvedEvent(DirectiveType::RebuildSanctum,
                      CommandOutcome::SparksInsurrection, 0.05f));
    w.executor.update(60.0);
    CHECK(w.rec.count(EventType::SanctumRebuiltTick) == 0);
    CHECK(w.rec.count(EventType::SanctumRebuilt) == 0);
}

// ---- BeliefSystem power reactions ----

static void test_belief_power_hooks() {
    World w;
    // MAX_ACTIVE is 3: Dreams, Reconstruction, Onslaught.
    adoptNow(w.beliefs, Belief::Dreams);
    adoptNow(w.beliefs, Belief::Reconstruction);
    adoptNow(w.beliefs, Belief::Onslaught);

    // The new ambient events convert into power deltas when their creed
    // is active (the driver adds these to the power pool).
    GameEvent dream(EventType::DreamShared);
    dream.amount = 2.0f;
    CHECK_CLOSE(w.beliefs.onEvent(dream), 2.0f, 1e-4f); // DREAMSHARE_POWER*2
    GameEvent mend(EventType::EffigyMended);
    mend.amount = 1.0f;
    CHECK_CLOSE(w.beliefs.onEvent(mend), 1.0f, 1e-4f);  // EFFIGY_POWER*1
    GameEvent hunt(EventType::WildsHunted);
    hunt.amount = 2.0f;
    CHECK_CLOSE(w.beliefs.onEvent(hunt), 1.0f, 1e-4f);  // HUNT_POWER*2
    // No belief hook for rumors: no power.
    GameEvent rumor(EventType::RumorSpread);
    rumor.amount = 1.0f;
    CHECK_CLOSE(w.beliefs.onEvent(rumor), 0.0f, 1e-4f);
    // The blood rite raises dread only when Fear is active; Fear is
    // inactive here.
    const float fearBefore = w.beliefs.fearLevel();
    GameEvent rite(EventType::RiteOfFlesh);
    w.beliefs.onEvent(rite);
    CHECK_CLOSE(w.beliefs.fearLevel(), fearBefore, 1e-4f);
}

// ---- Legal-name scan ----

static std::string lowerStr(std::string s) {
    for (char& ch : s)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

static void test_legal_names() {
    // assets/creatures/LEGAL_NAMES.md: only Lovecraft-original PD terms or
    // plain generic English. These are the new wave-15 user-facing
    // strings; none may contain a forbidden term.
    const std::vector<std::string> newStrings = {
        "OneiricHarvest", "RebuildSanctum", "DreamSharing", "MendEffigy",
        "WhisperCampaign", "BloodRite", "WildsHunt", "dreamshared",
        "rumor", "directive_dream", "dream", "mend", "rumor", "rite",
        "hunt", "rebuild", "OneiricHarvestCompleted", "SanctumRebuiltTick",
        "SanctumRebuilt",
    };
    const std::vector<std::string> forbidden = {
        "tindalos", "derleth", "call of cthulhu",
    };
    for (const auto& s : newStrings)
        for (const auto& f : forbidden)
            CHECK(lowerStr(s).find(f) == std::string::npos);
    // The wave-9b chanting lures are allowlisted generic terms.
    for (const char* species : {"ghoul", "deep one", "night-gaunt"}) {
        const std::string ls = lowerStr(species);
        for (const auto& f : forbidden) CHECK(ls.find(f) == std::string::npos);
    }
}

int main() {
    std::cout << "CULT-ULHU wave 15 content tests\n";
    test_ambient_action_names();
    test_dream_sharing_cold();
    test_dream_sharing_heats_dreams();
    test_dream_sharing_hot();
    test_mend_effigy();
    test_whisper_campaign();
    test_blood_rite();
    test_wilds_hunt();
    test_new_action_weights();
    test_new_directive_names();
    test_directive_exertion_feeds();
    test_oneiric_harvest();
    test_oneiric_harvest_partial();
    test_oneiric_harvest_refused();
    test_oneiric_harvest_no_power_ctx();
    test_rebuild_sanctum();
    test_rebuild_sanctum_partial();
    test_rebuild_sanctum_refused();
    test_issue_dream_end_to_end();
    test_issue_rebuild_end_to_end();
    test_belief_power_hooks();
    test_legal_names();
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
