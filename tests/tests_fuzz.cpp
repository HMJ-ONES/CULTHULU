// CULT-ULHU wave-9d regression tests: deterministic reproducers for every
// bug the fuzz harness found (plus the hardening fixes it motivated).
// Compiled manually (not a cmake target):
//   g++ -std=c++17 -Isrc tests/tests_fuzz.cpp build/libcultulhu.a -o /tmp/tests_fuzz
// Run directly; exits nonzero on failure.

#include "animation/ClipSerializer.h"
#include "beliefs/BeliefSystem.h"
#include "characters/abilities/RmbAbility.h"
#include "characters/abilities/WaveOfDomination.h"
#include "combat/CrowdControl.h"
#include "commands/CommandSystem.h"
#include "commands/DirectiveExecutor.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "cult/CultManager.h"
#include "entities/Units.h"
#include "exertion/ExertionSystem.h"
#include "net/Netcode.h"
#include "net/Protocol.h"
#include "power/PowerSystem.h"
#include "save/SaveSystem.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <string>

using namespace cultulhu;

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

static std::string tmpPath(const std::string& name) {
    return "/tmp/cultulhu_fuzztest_" + name;
}

static void writeFile(const std::string& path, const std::string& data) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f.write(data.data(), static_cast<std::streamsize>(data.size()));
}

// ---------------------------------------------------------------------------
// BUG 1 (wave 9d): SaveSystem::load threw uncaught std::invalid_argument /
// std::out_of_range on malformed save files (std::stod/stof/stoi), which
// terminated the driver. Reproducer: any corrupt numeric field.
// Fix: the parse is wrapped in try/catch(...) -> load returns false.
// ---------------------------------------------------------------------------
static void test_save_load_malformed_no_throw() {
    const std::string badClock = tmpPath("badclock.sav");
    const std::string badPower = tmpPath("badpower.sav");
    const std::string badBeliefs = tmpPath("badbeliefs.sav");
    const std::string badRisk = tmpPath("badrisk.sav");
    const std::string badEntity = tmpPath("badentity.sav");
    const std::string garbage = tmpPath("garbage.sav");

    writeFile(badClock, "clock=abc\npower=10\n");
    writeFile(badPower, "clock=1\npower=1e999999999\n"); // stof out_of_range
    writeFile(badBeliefs, "clock=1\nbeliefs=1,xyz,2\n");
    writeFile(badRisk, "clock=1\nrisk=--5\n");
    writeFile(badEntity, "clock=1\nentity=1 2 3\n"); // truncated entity line
    std::string bin("clock=");
    bin.push_back('\xff');
    bin.push_back('\xfe');
    bin.push_back('\x00');
    bin += "garbage-bytes";
    writeFile(garbage, bin);

    GameState s;
    bool threw = false;
    try {
        CHECK(!SaveSystem::load(badClock, s));
        CHECK(!SaveSystem::load(badPower, s));
        CHECK(!SaveSystem::load(badBeliefs, s));
        CHECK(!SaveSystem::load(badRisk, s));
        CHECK(!SaveSystem::load(badEntity, s));
        CHECK(!SaveSystem::load(garbage, s));
        CHECK(!SaveSystem::load("/tmp/cultulhu_fuzztest_does_not_exist.sav", s));
    } catch (...) {
        threw = true;
    }
    CHECK(!threw); // must never throw, on any input

    // A valid save still round-trips.
    GameState good;
    good.clockTime = 123.5;
    good.power = 77.0f;
    good.activeBeliefs = {Belief::Fear, Belief::Dreams};
    good.insurrectionRisk = 12.5f;
    const std::string okPath = tmpPath("ok.sav");
    CHECK(SaveSystem::save(good, okPath));
    GameState back;
    CHECK(SaveSystem::load(okPath, back));
    CHECK(back.clockTime == 123.5);
    CHECK(back.activeBeliefs.size() == 2);

    std::remove(badClock.c_str());
    std::remove(badPower.c_str());
    std::remove(badBeliefs.c_str());
    std::remove(badRisk.c_str());
    std::remove(badEntity.c_str());
    std::remove(garbage.c_str());
    std::remove(okPath.c_str());
}

// ---------------------------------------------------------------------------
// BUG 2 (wave 9d): net::decodeSnapshot / net::decodePlayerKda looped
// `for (i < n)` with n taken straight from the remote message. A malicious
// host could set n = INT_MAX and hang the client for billions of iterations.
// Reproducer: n=2147483647. Fix: sanity cap (n > 4096 -> false).
// ---------------------------------------------------------------------------
static void test_decode_count_cap() {
    using net::Message;
    using net::MsgType;

    Message huge;
    huge.type = MsgType::HostSnapshot;
    huge.fields["tick"] = "1";
    huge.fields["n"] = "2147483647";
    uint32_t tick = 0;
    std::vector<net::SnapshotEntity> ents;
    // Would hang ~forever before the fix; must return false immediately.
    CHECK(!net::decodeSnapshot(huge, tick, ents));

    Message neg;
    neg.type = MsgType::HostSnapshot;
    neg.fields["n"] = "-5";
    CHECK(!net::decodeSnapshot(neg, tick, ents));

    Message kda;
    kda.type = MsgType::PlayerKda;
    kda.fields["n"] = "2147483647";
    std::vector<net::KdaEntry> rows;
    CHECK(!net::decodePlayerKda(kda, rows));

    // Legitimate small messages still decode.
    Message ok;
    ok.type = MsgType::HostSnapshot;
    ok.fields["tick"] = "7";
    ok.fields["n"] = "2";
    ok.fields["e0"] = "10,1.0,2.0,3.0,100.0,5";
    ok.fields["e1"] = "11,4.0,5.0,6.0,80.0,0";
    CHECK(net::decodeSnapshot(ok, tick, ents));
    CHECK(tick == 7);
    CHECK(ents.size() == 2);
    CHECK(ents[0].id == 10);
    CHECK(ents[0].state == 5);

    Message okKda;
    okKda.type = MsgType::PlayerKda;
    okKda.fields["n"] = "1";
    okKda.fields["e0"] = "3,10,2,5,Bob";
    CHECK(net::decodePlayerKda(okKda, rows));
    CHECK(rows.size() == 1);
    CHECK(rows[0].row.kills == 10);
}

// ---------------------------------------------------------------------------
// BUG 3 (wave 9d): ClipSerializer::load did keys.reserve(nkeys) with nkeys
// read straight from the file. A corrupt file (e.g. 1e12 keys, fits in
// size_t) made reserve() throw bad_alloc/length_error -> terminate.
// Reproducer below. Fix: sanity cap on nkeys; NaN durations rejected too.
// ---------------------------------------------------------------------------
static void test_clip_load_huge_keycount() {
    const std::string huge = tmpPath("huge.canim");
    const std::string nanDur = tmpPath("nandur.canim");
    writeFile(huge, "CLIP \"x\" 1.0 0\nTRACK bone 1000000000000\n");
    writeFile(nanDur, "CLIP \"x\" nan 0\n");

    AnimationClip clip;
    bool threw = false;
    try {
        CHECK(!ClipSerializer::load(huge, clip));
        CHECK(!ClipSerializer::load(nanDur, clip));
    } catch (...) {
        threw = true;
    }
    CHECK(!threw);

    // A well-formed clip still loads.
    AnimationClip src;
    src.name = "ok";
    src.durationSeconds = 2.0;
    BoneTrack t;
    t.bone = "spine";
    t.keys.push_back(Keyframe{});
    src.tracks["spine"] = t;
    const std::string ok = tmpPath("ok.canim");
    CHECK(ClipSerializer::save(src, ok));
    AnimationClip back;
    CHECK(ClipSerializer::load(ok, back));
    CHECK(back.name == "ok");

    std::remove(huge.c_str());
    std::remove(nanDur.c_str());
    std::remove(ok.c_str());
}

// ---------------------------------------------------------------------------
// Target-2 pin (wave 9d): CommandSystem::issueCommand with an out-of-range
// DirectiveType. directiveName() falls through its switch and returns
// "Unknown"; the executor then parses the "Unknown/..." tag. The path is
// exercised by the fuzzer; this pins the graceful behavior.
// ---------------------------------------------------------------------------
static void test_issue_command_bad_directive() {
    EventBus bus;
    GameClock clock;
    RNG rng(42);
    BeliefSystem beliefs(bus, clock);
    CultManager cult(bus, clock, rng);
    PowerSystem power;
    ExertionSystem exertion(bus, beliefs, power, cult, rng);
    DirectiveExecutor executor(bus, rng, cult);
    CommandSystem commands(bus, rng, beliefs, cult);

    bool threw = false;
    CommandResult r1{CommandOutcome::Refused, 0.0f, ""};
    CommandResult r2{CommandOutcome::Refused, 0.0f, ""};
    CommandResult r3{CommandOutcome::Refused, 0.0f, ""};
    try {
        r1 = commands.issueCommand(DirectiveType::Count, Vec3{0, 0, 0});
        r2 = commands.issueCommand(static_cast<DirectiveType>(999),
                                   Vec3{1, 2, 3});
        r3 = commands.issueCommand(static_cast<DirectiveType>(-1),
                                   Vec3{std::numeric_limits<float>::quiet_NaN(),
                                        0, 0});
    } catch (...) {
        threw = true;
    }
    CHECK(!threw);
    (void)r1;
    (void)r2;
    (void)r3;
}

// ---------------------------------------------------------------------------
// Target-1 pin (wave 9d): publishing events with invalid type IDs and
// NaN/Inf payloads through the full subscriber stack must not throw or
// corrupt the bus. The fuzzer hammers this path; this pins it.
// ---------------------------------------------------------------------------
static void test_event_bus_malformed() {
    EventBus bus;
    GameClock clock;
    RNG rng(7);
    BeliefSystem beliefs(bus, clock);
    CultManager cult(bus, clock, rng);
    PowerSystem power;
    ExertionSystem exertion(bus, beliefs, power, cult, rng);
    DirectiveExecutor executor(bus, rng, cult);

    bool threw = false;
    try {
        for (int i = 0; i < 2000; ++i) {
            GameEvent e;
            const int kind = i % 5;
            if (kind == 0)
                e.type = static_cast<EventType>(-1 - (i % 8));
            else if (kind == 1)
                e.type = static_cast<EventType>(
                    static_cast<int>(EventType::Count) + (i % 8));
            else
                e.type = static_cast<EventType>(
                    i % static_cast<int>(EventType::Count));
            e.sourceId = static_cast<uint64_t>(i) * 0x9E3779B97F4A7C15ull;
            e.targetId = 0;
            e.faction = (i % 3 == 0) ? -100000 : (i % 7) - 2;
            const float nan = std::numeric_limits<float>::quiet_NaN();
            const float inf = std::numeric_limits<float>::infinity();
            e.amount = (i % 4 == 0) ? nan : ((i % 4 == 1) ? inf : -1.0e30f);
            e.pos = Vec3(nan, inf, -inf);
            e.tag = std::string(1024, 'x') + "/Obeyed";
            bus.publish(e);
        }
        // The bus still works afterwards.
        CHECK(bus.handlerCount(EventType::PrayerOffered) >= 0);
    } catch (...) {
        threw = true;
    }
    CHECK(!threw);
}

// ---------------------------------------------------------------------------
// BUG 6 (wave 9d, found by target 3 under AddressSanitizer):
// heap-use-after-free in WaveOfDomination. The ability cached raw Entity*
// victims; when the world was cleared under it (save/load) the victims
// were freed, and the next launch/update dereferenced the dangling
// pointers (updateFlying read freed memory).
// Reproducer: rmb press (catch) -> load (world cleared) -> rmb launch.
// Fix: pruneStaleVictims() drops victims whose pointer is no longer in
// the live context entity list (checked before every update/input edge),
// plus BetaGame::applySaveState cancels the ability on load.
// ---------------------------------------------------------------------------
static void test_wave_uaf_world_cleared() {
    EventBus bus;
    GameClock clock;
    RNG rng(1);
    PowerSystem power;
    BeliefSystem beliefs(bus, clock);
    ActiveEffects fx;
    EldritchAvatar avatar(FACTION_CTHULHU, Vec3{0, 0, 0}, power);

    // Scenario 1: launch after the world was cleared.
    {
        WaveOfDomination ability;
        RmbContext ctx(bus, fx, beliefs, rng);
        ctx.caster = &avatar;
        ctx.casterYaw = 0.0f;
        auto civ = std::make_unique<Civilian>(Vec3{6.0f, 0.0f, 0.0f});
        ctx.entities.push_back(civ.get());

        ability.onPress(ctx);
        for (int i = 0;
             i < 40 && ability.phase() == WaveOfDomination::Phase::Wave;
             ++i)
            ability.update(ctx, 0.1);
        CHECK(ability.phase() == WaveOfDomination::Phase::Hold);
        CHECK(ability.victimCount() == 1);

        // Simulate save/load: world cleared (victim freed), context
        // rebuilt without it.
        civ.reset();
        ctx.entities.clear();

        ability.onLeftClick(ctx); // launch: must not touch freed memory
        for (int i = 0; i < 30; ++i) ability.update(ctx, 0.1);
        CHECK(ability.victimCount() == 0);
        CHECK(ability.phase() == WaveOfDomination::Phase::Idle);
        ability.cancel(); // safe on an idle ability
    }

    // Scenario 2: release (drop) after the world was cleared.
    {
        WaveOfDomination ability;
        RmbContext ctx(bus, fx, beliefs, rng);
        ctx.caster = &avatar;
        auto civ = std::make_unique<Civilian>(Vec3{6.0f, 0.0f, 0.0f});
        ctx.entities.push_back(civ.get());

        ability.onPress(ctx);
        for (int i = 0;
             i < 40 && ability.phase() == WaveOfDomination::Phase::Wave;
             ++i)
            ability.update(ctx, 0.1);
        CHECK(ability.phase() == WaveOfDomination::Phase::Hold);

        civ.reset();
        ctx.entities.clear();

        ability.onRelease(ctx); // drop: must not touch freed memory
        CHECK(ability.phase() == WaveOfDomination::Phase::Idle);
        CHECK(ability.victimCount() == 0);
    }
}

int main() {
    test_save_load_malformed_no_throw();
    test_decode_count_cap();
    test_clip_load_huge_keycount();
    test_issue_command_bad_directive();
    test_event_bus_malformed();
    test_wave_uaf_world_cleared();
    std::cout << "fuzz: " << checks << " checks, " << failures
              << " failures\n";
    return failures == 0 ? 0 : 1;
}
