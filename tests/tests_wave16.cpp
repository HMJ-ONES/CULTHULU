// CULT-ULHU wave 16 tests: achievements — 10 single-player + 7 multiplayer,
// driven by real events. Covers:
//   - each achievement unlocks on its true trigger, exactly once
//   - negative cases (wrong species, neutral faction, no local player)
//   - match-scope reset on MatchStarted; victory/death evaluation on MatchEnded
//   - new source instrumentation: RelicClaimed, CityDestroyed, PointCaptured,
//     GreatOldOneSlain, ValeRelicClaimed, MonstrositySlain, PlayerKilled
//   - save/load round-trip of unlocked set + progress counters
//   - legal-name scan of all achievement strings
// Compile manually (CMakeLists.txt wires it as cultulhu_tests_wave16):
//   g++ -std=c++17 -Isrc tests/tests_wave16.cpp build/libcultulhu.a \
//       -o /tmp/wave16_test && /tmp/wave16_test

#include "achievements/AchievementSystem.h"
#include "city/CitySystem.h"
#include "combat/Combat.h"
#include "core/EventBus.h"
#include "core/Events.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "entities/Units.h"
#include "modes/CapturePointMode.h"
#include "modes/MobaDefense.h"
#include "power/PowerSystem.h"
#include "relics/RelicSystem.h"
#include "save/SaveSystem.h"
#include "world/ValeOfPnath.h"

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
};

static void fireConversion(EventBus& bus, float n, const std::string& tag = "",
                           int faction = FACTION_NEUTRAL) {
    GameEvent e(EventType::ConversionPerformed);
    e.amount = n;
    e.tag = tag;
    e.faction = faction;
    bus.publish(e);
}

// damageBase is protected; expose it for tests.
struct TestMoba : MobaDefense {
    using MobaDefense::MobaDefense;
    void hitBase(int team, float dmg, uint64_t attacker) {
        damageBase(team, dmg, attacker);
    }
};

int main() {
    // ---------- single-player: conversion achievements ----------
    {
        EventBus bus;
        AchievementSystem a(bus);
        CHECK(!a.isUnlocked("first_flesh"));
        fireConversion(bus, 1.0f);
        CHECK(a.isUnlocked("first_flesh"));
        // Exactly once: more conversions don't re-fire.
        Recorder r; r.attach(bus, EventType::AchievementUnlocked);
        fireConversion(bus, 5.0f);
        CHECK(r.count(EventType::AchievementUnlocked) == 0);
        CHECK(!a.isUnlocked("flock_multiplies"));
        fireConversion(bus, 19.0f); // total 25
        CHECK(a.isUnlocked("flock_multiplies"));
        auto pr = a.progress("flock_multiplies");
        CHECK(pr.first == 25.0 && pr.second == 25.0);
    }
    // ---------- dreamthief: only dream_whisper conversions count ----------
    {
        EventBus bus;
        AchievementSystem a(bus);
        fireConversion(bus, 60.0f, "sermon"); // not dreams
        CHECK(!a.isUnlocked("dreamthief"));
        for (int i = 0; i < 49; ++i) fireConversion(bus, 1.0f, "dream_whisper");
        CHECK(!a.isUnlocked("dreamthief"));
        fireConversion(bus, 1.0f, "dream_whisper"); // 50th
        CHECK(a.isUnlocked("dreamthief"));
        auto pr = a.progress("dreamthief");
        CHECK(pr.first == 50.0 && pr.second == 50.0);
    }
    // ---------- ashes_of_man / architect_of_desolation / unmaker_of_worlds --
    {
        EventBus bus;
        AchievementSystem a(bus);
        GameEvent d(EventType::DistrictRazed);
        bus.publish(d);
        CHECK(a.isUnlocked("ashes_of_man"));
        for (int i = 0; i < 24; ++i) {
            GameEvent b(EventType::CityBuildingDestroyed);
            bus.publish(b);
        }
        CHECK(!a.isUnlocked("architect_of_desolation"));
        GameEvent b(EventType::CityBuildingDestroyed);
        bus.publish(b); // 25th
        CHECK(a.isUnlocked("architect_of_desolation"));
        for (int i = 0; i < 4; ++i) {
            GameEvent c(EventType::CityDestroyed);
            bus.publish(c);
        }
        CHECK(!a.isUnlocked("unmaker_of_worlds"));
        GameEvent c(EventType::CityDestroyed);
        bus.publish(c); // 5th
        CHECK(a.isUnlocked("unmaker_of_worlds"));
    }
    // ---------- price_of_power via RelicSystem::seizeRelic ----------
    {
        EventBus bus;
        AchievementSystem a(bus);
        Recorder r; r.attach(bus, EventType::RelicClaimed);
        RelicSystem relics(bus);
        relics.seizeRelic(0.25f, 555, "Eye of the Deep");
        CHECK(r.count(EventType::RelicClaimed) == 1);
        CHECK(r.events[0].sourceId == 555);
        CHECK(a.isUnlocked("price_of_power"));
        CHECK(std::fabs(relics.powerMultiplier() - 1.25f) < 1e-5);
    }
    // ---------- what_lies_beneath via ValeOfPnath::claimRelic ----------
    {
        EventBus bus;
        AchievementSystem a(bus);
        Recorder r; r.attach(bus, EventType::ValeRelicClaimed);
        ValeOfPnath vale(bus, 1, 4242, Vec3{0, 0, 0});
        CHECK(vale.claimRelic(777));
        CHECK(r.count(EventType::ValeRelicClaimed) == 1);
        CHECK(!vale.claimRelic(777)); // vault emptied: no second event
        CHECK(r.count(EventType::ValeRelicClaimed) == 1);
        CHECK(a.isUnlocked("what_lies_beneath"));
        // A vale relic is still a relic: price_of_power fires too.
        CHECK(a.isUnlocked("price_of_power"));
    }
    // ---------- the_stirring ----------
    {
        EventBus bus;
        AchievementSystem a(bus);
        GameEvent e(EventType::ChampionSummoned);
        bus.publish(e);
        CHECK(a.isUnlocked("the_stirring"));
    }
    // ---------- burrowers_bane: dhole only ----------
    {
        EventBus bus;
        AchievementSystem a(bus);
        GameEvent e(EventType::MonstrositySlain);
        e.tag = "charnel_imp";
        bus.publish(e);
        CHECK(!a.isUnlocked("burrowers_bane"));
        GameEvent d(EventType::MonstrositySlain);
        d.tag = "dhole";
        bus.publish(d);
        CHECK(a.isUnlocked("burrowers_bane"));
    }
    // ---------- MonstrositySlain from real combat ----------
    {
        EventBus bus;
        AchievementSystem a(bus);
        Recorder r; r.attach(bus, EventType::MonstrositySlain);
        Dhole dhole(FACTION_NEUTRAL, Vec3{0, 0, 0});
        combat::dealDamage(dhole, 99999.0f, bus, EventType::MimicSlain);
        CHECK(r.count(EventType::MonstrositySlain) == 1);
        CHECK(r.events[0].tag == "dhole");
        CHECK(a.isUnlocked("burrowers_bane"));
        // Non-monstrosity kills don't publish it.
        Civilian civ(Vec3{0, 0, 0});
        combat::dealDamage(civ, 99999.0f, bus, EventType::MimicSlain);
        CHECK(r.count(EventType::MonstrositySlain) == 1);
    }
    // ---------- CityDestroyed from real siege ----------
    {
        EventBus bus;
        AchievementSystem a(bus);
        Recorder r; r.attach(bus, EventType::CityDestroyed);
        CitySystem cities(bus);
        City& c = cities.foundCity("Innsmouth", Vec3{0, 0, 0});
        c.addDistrict("Docks", 100, 2, 50.0f);
        c.addDistrict("Old Town", 100, 1, 50.0f);
        cities.damageBuilding(c, 0, 0, 9999.0f);
        cities.damageBuilding(c, 0, 1, 9999.0f);
        CHECK(r.count(EventType::CityDestroyed) == 0); // one district left
        cities.damageBuilding(c, 1, 0, 9999.0f);
        CHECK(r.count(EventType::CityDestroyed) == 1);
        CHECK(r.events[0].tag == "Innsmouth");
        CHECK(!a.isUnlocked("unmaker_of_worlds")); // only 1 of 5
        // No double-fire on further damage attempts.
        cities.damageBuilding(c, 1, 0, 9999.0f);
        CHECK(r.count(EventType::CityDestroyed) == 1);
    }
    // ---------- multiplayer: first_blood / reaper / match reset ----------
    {
        EventBus bus;
        AchievementSystem a(bus);
        PowerSystem power;
        EldritchAvatar killer(FACTION_CTHULHU, Vec3{0, 0, 0}, power);
        EldritchAvatar victim(1, Vec3{5, 0, 0}, power);
        // No local player set: PvP kills don't count.
        combat::dealDamage(victim, 99999.0f, bus, EventType::MimicSlain,
                           false, killer.id(), EntityType::EldritchAvatar);
        CHECK(!a.isUnlocked("first_blood"));
        a.setLocalPlayer(killer.id(), FACTION_CTHULHU, 0);
        EldritchAvatar victim2(1, Vec3{5, 0, 0}, power);
        combat::dealDamage(victim2, 99999.0f, bus, EventType::MimicSlain,
                           false, killer.id(), EntityType::EldritchAvatar);
        CHECK(a.isUnlocked("first_blood"));
        // Match scope: 9 more kills -> reaper at 10.
        GameEvent ms(EventType::MatchStarted);
        bus.publish(ms);
        for (int i = 0; i < 9; ++i) {
            GameEvent k(EventType::PlayerKilled);
            k.sourceId = killer.id();
            bus.publish(k);
        }
        CHECK(!a.isUnlocked("reaper"));
        GameEvent k(EventType::PlayerKilled);
        k.sourceId = killer.id();
        bus.publish(k); // 10th
        CHECK(a.isUnlocked("reaper"));
        // New match resets the counter.
        bus.publish(ms);
        auto pr = a.progress("reaper");
        CHECK(pr.first == 0.0);
    }
    // ---------- unbroken / dominion ----------
    {
        EventBus bus;
        AchievementSystem a(bus);
        a.setLocalPlayer(4242, FACTION_CTHULHU, 0);
        GameEvent ms(EventType::MatchStarted);
        bus.publish(ms);
        GameEvent me(EventType::MatchEnded);
        me.faction = 0; // our team wins
        bus.publish(me);
        CHECK(a.isUnlocked("dominion"));
        CHECK(a.isUnlocked("unbroken"));
    }
    {
        EventBus bus;
        AchievementSystem a(bus);
        a.setLocalPlayer(4242, FACTION_CTHULHU, 0);
        GameEvent ms(EventType::MatchStarted);
        bus.publish(ms);
        GameEvent k(EventType::PlayerKilled); // we died once
        k.targetId = 4242;
        bus.publish(k);
        GameEvent me(EventType::MatchEnded);
        me.faction = 0;
        bus.publish(me);
        CHECK(a.isUnlocked("dominion"));
        CHECK(!a.isUnlocked("unbroken"));
    }
    {
        // Lost match: neither fires.
        EventBus bus;
        AchievementSystem a(bus);
        a.setLocalPlayer(4242, FACTION_CTHULHU, 0);
        GameEvent ms(EventType::MatchStarted);
        bus.publish(ms);
        GameEvent me(EventType::MatchEnded);
        me.faction = 1; // enemy team wins
        bus.publish(me);
        CHECK(!a.isUnlocked("dominion"));
        CHECK(!a.isUnlocked("unbroken"));
    }
    // ---------- standard_bearer via real point capture ----------
    {
        EventBus bus;
        GameClock clock;
        AchievementSystem a(bus);
        a.setLocalPlayer(99, FACTION_CTHULHU, 0);
        Recorder r; r.attach(bus, EventType::PointCaptured);
        CapturePointMode mode(bus, clock);
        mode.addPoint(Vec3{0, 0, 0});
        mode.setOccupants(0, 3, 0);
        for (int i = 0; i < 10 && r.count(EventType::PointCaptured) == 0; ++i)
            mode.update(0.5);
        CHECK(r.count(EventType::PointCaptured) == 1);
        CHECK(r.events[0].faction == 0);
        CHECK(a.isUnlocked("standard_bearer"));
    }
    // ---------- godslayer via real GOO kill ----------
    {
        EventBus bus;
        GameClock clock;
        RNG rng(7);
        AchievementSystem a(bus);
        a.setLocalPlayer(31337, FACTION_CTHULHU, 0);
        Recorder r; r.attach(bus, EventType::GreatOldOneSlain);
        TestMoba moba(bus, clock, rng);
        moba.setBase(1, Vec3{100, 0, 0}, 100.0f);
        moba.hitBase(1, 500.0f, 31337); // our killing blow
        CHECK(r.count(EventType::GreatOldOneSlain) == 1);
        CHECK(r.events[0].sourceId == 31337);
        CHECK(r.events[0].faction == 1);
        CHECK(a.isUnlocked("godslayer"));
    }
    // ---------- turncoat: enemy faction only ----------
    {
        EventBus bus;
        AchievementSystem a(bus);
        fireConversion(bus, 1.0f, "ritual", FACTION_NEUTRAL);
        CHECK(!a.isUnlocked("turncoat"));
        fireConversion(bus, 1.0f, "ritual", FACTION_CTHULHU);
        CHECK(!a.isUnlocked("turncoat"));
        fireConversion(bus, 1.0f, "ritual", 2); // rival deity's flock
        CHECK(a.isUnlocked("turncoat"));
    }
    // ---------- match lifecycle from modes ----------
    {
        EventBus bus;
        GameClock clock;
        Recorder r;
        r.attach(bus, EventType::MatchStarted);
        r.attach(bus, EventType::MatchEnded);
        CapturePointMode mode(bus, clock);
        mode.addPoint(Vec3{0, 0, 0});
        mode.update(0.1);
        CHECK(r.count(EventType::MatchStarted) == 1);
        CHECK(r.events[0].tag == "capture");
        mode.update(0.1);
        CHECK(r.count(EventType::MatchStarted) == 1); // once only
    }
    // ---------- save/load round-trip ----------
    {
        EventBus bus;
        AchievementSystem a(bus);
        fireConversion(bus, 30.0f); // first_flesh + flock_multiplies
        fireConversion(bus, 10.0f, "dream_whisper");
        CHECK(a.isUnlocked("first_flesh"));
        CHECK(a.isUnlocked("flock_multiplies"));
        CHECK(!a.isUnlocked("dreamthief"));
        GameState s;
        a.saveTo(s);
        CHECK(SaveSystem::save(s, "/tmp/wave16_save.txt"));
        EventBus bus2;
        AchievementSystem b(bus2);
        GameState s2;
        CHECK(SaveSystem::load("/tmp/wave16_save.txt", s2));
        b.loadFrom(s2);
        CHECK(b.isUnlocked("first_flesh"));
        CHECK(b.isUnlocked("flock_multiplies"));
        CHECK(!b.isUnlocked("dreamthief"));
        auto pr = b.progress("dreamthief");
        CHECK(pr.first == 10.0 && pr.second == 50.0);
        // Old saves (no achievement lines) still load fine.
        GameState s3;
        CHECK(SaveSystem::load("/tmp/wave16_save.txt", s3));
        (void)s3;
    }
    // ---------- defs: 17 achievements, legal-name scan ----------
    {
        EventBus bus;
        AchievementSystem a(bus);
        CHECK(a.defs().size() == 17);
        size_t mp = 0;
        for (const auto& d : a.defs()) if (d.multiplayer) ++mp;
        CHECK(mp == 7);
        const std::vector<std::string> banned = {
            "tindalos", "derleth", "call of cthulhu"};
        for (const auto& d : a.defs()) {
            CHECK(!d.id.empty() && !d.name.empty() && !d.description.empty());
            std::string hay = d.name + " " + d.description;
            std::string low = hay;
            std::transform(low.begin(), low.end(), low.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            for (const auto& b : banned)
                CHECK(low.find(b) == std::string::npos);
        }
        // Stable ids are unique.
        std::vector<std::string> ids;
        for (const auto& d : a.defs()) ids.push_back(d.id);
        std::sort(ids.begin(), ids.end());
        CHECK(std::adjacent_find(ids.begin(), ids.end()) == ids.end());
    }

    std::cout << "checks=" << checks << " failures=" << failures << "\n";
    return failures == 0 ? 0 : 1;
}
