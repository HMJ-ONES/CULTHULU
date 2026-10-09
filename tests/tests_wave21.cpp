// CULT-ULHU wave 21 tests: fully-implemented 5v5 modes — Match framework,
// capture-the-point, MOBA defense, protocol ModeState, and achievement
// wiring verification (incl. Turncoat as verified-but-not-triggerable).
//
// Compile manually (CMakeLists.txt wires it as cultulhu_tests_wave21):
//   g++ -std=c++17 -Isrc tests/tests_wave21.cpp build/libcultulhu.a \
//       -o /tmp/wave21_test && /tmp/wave21_test

#include "achievements/AchievementSystem.h"
#include "core/EventBus.h"
#include "core/Events.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "entities/Entity.h"
#include "modes/CapturePointMode.h"
#include "modes/Match.h"
#include "modes/MobaDefense.h"
#include "net/Protocol.h"

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
    const GameEvent* last(EventType t) const {
        for (auto it = events.rbegin(); it != events.rend(); ++it)
            if (it->type == t) return &(*it);
        return nullptr;
    }
};

static int countKind(const MobaDefense& m, const std::string& kind) {
    int n = 0;
    for (const auto& x : m.minions())
        if (x.kind == kind && x.hp > 0.0f) ++n;
    return n;
}

int main() {
    // ---------- capture: onslaught default point ----------
    {
        EventBus bus; GameClock clock;
        CapturePointMode mode(bus, clock);
        mode.setupOnslaughtPoint();
        CHECK(mode.pointCount() == 1);
        CHECK(std::fabs(mode.pointPos(0).x - 0.0f) < 0.01f);
        CHECK(std::fabs(mode.pointPos(0).z - 0.0f) < 0.01f);
        CHECK(std::fabs(mode.pointRadius(0) - CapturePointMode::POINT_RADIUS) < 0.01f);
        CHECK(mode.holder() == -1);
    }
    // ---------- capture: holding banks, contest pauses, empty idles ----
    {
        EventBus bus; GameClock clock;
        CapturePointMode mode(bus, clock);
        mode.addPoint(Vec3{0, 0, 0});
        mode.setOccupants(0, 2, 0);
        mode.update(10.0);
        CHECK(mode.holder() == 0);
        CHECK(std::fabs(mode.score(0) - 10.0f) < 0.01f); // 1 pt/s uncontested
        float s = mode.score(0);
        mode.setOccupants(0, 1, 1);
        mode.update(10.0);
        CHECK(mode.holder() == -1);              // contested: nobody banks
        CHECK(mode.contested());
        CHECK(mode.score(0) == s);
        mode.setOccupants(0, 0, 0);
        mode.update(10.0);
        CHECK(mode.holder() == -1);
        CHECK(!mode.contested());
        CHECK(mode.score(0) == s);                // empty: still nothing
    }
    // ---------- capture: kills score for the killer's team -------------
    {
        EventBus bus; GameClock clock;
        CapturePointMode mode(bus, clock);
        mode.addPoint(Vec3{0, 0, 0});
        mode.notePlayerKill(0);
        mode.notePlayerKill(0);
        mode.notePlayerKill(1);
        CHECK(std::fabs(mode.score(0) - 2.0f * CapturePointMode::KILL_POINTS) < 0.01f);
        CHECK(std::fabs(mode.score(1) - CapturePointMode::KILL_POINTS) < 0.01f);
        mode.notePlayerKill(7); // invalid team ignored
        CHECK(std::fabs(mode.score(0) - 2.0f * CapturePointMode::KILL_POINTS) < 0.01f);
    }
    // ---------- capture: seizing publishes PointCaptured ---------------
    {
        EventBus bus; GameClock clock;
        Recorder r; r.attach(bus, EventType::PointCaptured);
        CapturePointMode mode(bus, clock);
        mode.addPoint(Vec3{0, 0, 0});
        mode.setOccupants(0, 3, 0);
        mode.update(1.0);
        CHECK(mode.holder() == 0);
        CHECK(r.count(EventType::PointCaptured) == 1);
        CHECK(r.events[0].faction == 0);
        CHECK(r.events[0].targetId == 0);
        // Enemy steps in: contested, no new event, nobody holds.
        mode.setOccupants(0, 3, 1);
        mode.update(1.0);
        CHECK(mode.holder() == -1);
        CHECK(r.count(EventType::PointCaptured) == 1);
        // Team 1 drives team 0 off and seizes it.
        mode.setOccupants(0, 0, 3);
        mode.update(1.0);
        CHECK(mode.holder() == 1);
        CHECK(r.count(EventType::PointCaptured) == 2);
        CHECK(r.events[1].faction == 1);
    }
    // ---------- capture: target-score winner ---------------------------
    {
        EventBus bus; GameClock clock;
        Recorder r;
        r.attach(bus, EventType::MatchStarted);
        r.attach(bus, EventType::MatchEnded);
        CapturePointMode mode(bus, clock);
        mode.setupOnslaughtPoint();
        mode.setOccupants(0, 5, 0);
        int ticks = 0;
        while (!mode.isOver() && ticks < 1000) { mode.update(10.0); ++ticks; }
        CHECK(mode.isOver());
        CHECK(mode.winner() == 0);
        CHECK(mode.score(0) >= CapturePointMode::TARGET_SCORE);
        CHECK(r.count(EventType::MatchStarted) == 1);
        CHECK(r.count(EventType::MatchEnded) == 1);
        const GameEvent* me = r.last(EventType::MatchEnded);
        CHECK(me && me->faction == 0); // MatchEnded carries the winner
        CHECK(me && me->tag == "capture");
    }
    // ---------- capture: time limit, higher score wins -----------------
    {
        EventBus bus; GameClock clock;
        Recorder r; r.attach(bus, EventType::MatchEnded);
        CapturePointMode mode(bus, clock);
        mode.addPoint(Vec3{0, 0, 0});
        mode.setOccupants(0, 3, 0);
        mode.update(250.0);              // team 0 banks 250 < 400
        CHECK(!mode.isOver());
        mode.setOccupants(0, 0, 3);
        mode.update(100.0);              // team 1 banks 100
        mode.setOccupants(0, 0, 0);
        mode.update(250.0);              // elapsed hits 600 -> time limit
        CHECK(mode.isOver());
        CHECK(!mode.overtime());         // not tied: no overtime
        CHECK(mode.winner() == 0);       // 250 > 100
        CHECK(mode.score(0) < CapturePointMode::TARGET_SCORE);
        CHECK(r.count(EventType::MatchEnded) == 1);
        CHECK(r.events[0].faction == 0);
    }
    // ---------- capture: tie at time limit -> sudden-death overtime ----
    {
        EventBus bus; GameClock clock;
        CapturePointMode mode(bus, clock);
        mode.addPoint(Vec3{0, 0, 0});
        mode.setOccupants(0, 3, 0);
        mode.update(100.0);              // 100-0
        mode.setOccupants(0, 0, 3);
        mode.update(100.0);              // 100-100
        mode.setOccupants(0, 0, 0);
        mode.update(400.0);              // clock hits 600 tied
        CHECK(!mode.isOver());           // overtime, not a draw
        CHECK(mode.overtime());
        CHECK(mode.winner() == -1);
        mode.notePlayerKill(1);          // first blood in overtime decides
        CHECK(mode.isOver());
        CHECK(mode.winner() == 1);
    }
    // ---------- capture: scoreless overtime expires as a draw ----------
    {
        EventBus bus; GameClock clock;
        Recorder r; r.attach(bus, EventType::MatchEnded);
        CapturePointMode mode(bus, clock);
        mode.addPoint(Vec3{0, 0, 0});
        mode.update(CapturePointMode::TIME_LIMIT); // 0-0 at the whistle
        CHECK(mode.overtime());
        CHECK(!mode.isOver());
        mode.update(CapturePointMode::OVERTIME_LIMIT);
        CHECK(mode.isOver());
        CHECK(mode.winner() == -1);      // nobody scored: draw
        CHECK(r.count(EventType::MatchEnded) == 1);
        CHECK(r.events[0].faction == -1);
    }
    // ---------- moba: default map ----------
    {
        EventBus bus; GameClock clock; RNG rng(1);
        MobaDefense moba(bus, clock, rng);
        moba.setupDefaultMap();
        CHECK(moba.laneCount() == 3);
        CHECK(std::fabs(moba.basePos(0).x + 150.0f) < 0.01f);
        CHECK(std::fabs(moba.basePos(1).x - 150.0f) < 0.01f);
        CHECK(std::fabs(moba.baseHp(0) - MobaDefense::GOO_MAX_HP) < 0.01f);
        moba.update(0.1); // towers auto-build
        CHECK(moba.towerCount() == 12); // 3 lanes x 2 teams x 2
        CHECK(moba.teamTowerAlive(0));
        CHECK(moba.teamTowerAlive(1));
    }
    // ---------- moba: wave composition ----------
    {
        EventBus bus; GameClock clock; RNG rng(2);
        MobaDefense moba(bus, clock, rng);
        moba.setupDefaultMap();
        moba.update(31.0); // first wave set fires (30s interval)
        // 2 teams x 3 lanes x (3 melee + 1 ranged); waveNumber hits 3 and 6
        // -> 2 siege engines.
        CHECK(moba.minionCount() == 26);
        CHECK(countKind(moba, "melee") == 18);
        CHECK(countKind(moba, "ranged") == 6);
        CHECK(countKind(moba, "siege") == 2);
    }
    // ---------- moba: minions march waypoints ----------
    {
        EventBus bus; GameClock clock; RNG rng(3);
        MobaDefense moba(bus, clock, rng);
        moba.setupDefaultMap();
        moba.spawnWave(0, 1);
        Vec3 before = moba.minions()[0].pos;
        moba.update(5.0);
        Vec3 after = moba.minions()[0].pos;
        CHECK(after.distance(before) > 1.0f); // actually marching
        // Closer to the lane's far end than it started.
        Vec3 farEnd = moba.lanePoint(0, 1, 1.0f);
        CHECK(after.distance(farEnd) < before.distance(farEnd));
    }
    // ---------- moba: towers shoot nearest enemy (players included) ----------
    {
        EventBus bus; GameClock clock; RNG rng(4);
        MobaDefense moba(bus, clock, rng);
        moba.setupDefaultMap();
        moba.update(0.1); // build towers
        Vec3 tp = moba.towerPos(0, 0, 0);
        CHECK(tp.length() > 0.01f);
        struct Hit { uint64_t target; uint64_t attacker; float dmg; };
        std::vector<Hit> hits;
        moba.setPlayerDamageHook([&](uint64_t t, float d, uint64_t a) {
            hits.push_back({t, a, d});
        });
        std::vector<MobaDefense::PlayerTarget> pts;
        MobaDefense::PlayerTarget near, far;
        near.id = 1001; near.team = 1; near.pos = tp + Vec3(3, 0, 0);
        far.id = 1002; far.team = 1; far.pos = tp + Vec3(10, 0, 0);
        pts.push_back(near); pts.push_back(far);
        moba.setPlayerTargets(pts);
        moba.update(1.0);
        CHECK(!hits.empty());
        for (const auto& h : hits) CHECK(h.target == 1001); // nearest only
    }
    // ---------- moba: backdoor protection + GOO kill ----------
    {
        EventBus bus; GameClock clock; RNG rng(5);
        Recorder r;
        r.attach(bus, EventType::GreatOldOneSlain);
        r.attach(bus, EventType::MatchEnded);
        MobaDefense moba(bus, clock, rng);
        moba.setupDefaultMap();
        moba.update(0.1);
        float hpBefore = moba.baseHp(1);
        moba.playerHitStructures(0, moba.basePos(1), 5.0f, 100.0f, 4242);
        CHECK(moba.baseHp(1) == hpBefore); // blocked: towers alive
        // Raze all team-1 towers (600 HP each).
        for (int l = 0; l < 3; ++l)
            for (int k = 0; k < 2; ++k)
                for (int h = 0; h < 13; ++h)
                    moba.playerHitStructures(0, moba.towerPos(1, l, k),
                                             5.0f, 50.0f, 4242);
        CHECK(!moba.teamTowerAlive(1));
        moba.playerHitStructures(0, moba.basePos(1), 5.0f, 100.0f, 4242);
        CHECK(moba.baseHp(1) < hpBefore); // now damage lands
        moba.playerHitStructures(0, moba.basePos(1), 5.0f, 5000.0f, 4242);
        CHECK(moba.isOver());
        CHECK(moba.winner() == 0);
        CHECK(r.count(EventType::GreatOldOneSlain) == 1);
        CHECK(r.events[0].sourceId == 4242); // Godslayer attribution
        CHECK(r.events[0].faction == 1);
        moba.update(0.1); // let the mode notice the fallen GOO
        CHECK(r.count(EventType::MatchEnded) == 1);
        const GameEvent* me = r.last(EventType::MatchEnded);
        CHECK(me && me->faction == 0);
    }
    // ---------- moba: minion cap skips waves ----------
    {
        EventBus bus; GameClock clock; RNG rng(6);
        MobaDefense moba(bus, clock, rng);
        moba.setupDefaultMap();
        for (int i = 0; i < 20; ++i) moba.spawnWave(0, 0);
        CHECK(moba.liveMinionCount(0) > MobaDefense::MINION_CAP_PER_TEAM);
        size_t before = moba.minionCount();
        int t0 = moba.liveMinionCount(0);
        moba.update(31.0); // wave tick: team 0 capped, team 1 spawns
        // No NEW team-0 waves (tower attrition may thin the herd, hence <=).
        CHECK(moba.liveMinionCount(0) <= t0);
        CHECK(moba.minionCount() > before);  // team 1 still gets waves
    }
    // ---------- match: bot auto-balance ----------
    {
        EventBus bus; GameClock clock; RNG rng(7);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        CHECK(!m.start("bogus"));
        for (int i = 0; i < 10; ++i) CHECK(m.addBot(-1) != 0);
        CHECK(m.addBot(-1) == 0); // roster full at 10
        int t0 = 0, t1 = 0;
        for (size_t i = 0; i < m.playerCount(); ++i)
            (m.player(i).team == 0 ? t0 : t1)++;
        CHECK(t0 == 5 && t1 == 5);
        CHECK(m.player(0).id == Match::kPlayerIdBase);
    }
    // ---------- match: kill -> PlayerKilled + KDA, respawn ----------
    {
        EventBus bus; GameClock clock; RNG rng(8);
        Recorder r; r.attach(bus, EventType::PlayerKilled);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t a = m.addBot(0);
        uint64_t b = m.addBot(1);
        m.movePlayer(a, Vec3(0, 0, 0));
        m.movePlayer(b, Vec3(1, 0, 0));
        int ticks = 0;
        while (r.count(EventType::PlayerKilled) == 0 && ticks < 30) {
            m.update(1.0); ++ticks;
        }
        CHECK(r.count(EventType::PlayerKilled) == 1);
        const GameEvent& e = r.events[0];
        CHECK(e.sourceId == a);          // killer is player a
        CHECK(e.targetId == b);          // victim is player b
        CHECK(e.faction == 1);           // victim's team
        // Onslaught: the PvP kill scored for the killer's team. The killing
        // tick also banks 1 hold point: the victim died before occupancy was
        // fed, so the killer stood alone on the point that tick.
        CHECK(std::fabs(m.capture()->score(0) -
                        (CapturePointMode::KILL_POINTS + 1.0f)) < 0.01f);
        CHECK(m.capture()->score(1) == 0.0f);
        auto rows = m.stats().rows();
        CHECK(rows.size() == 2);
        CHECK(rows[0].kills == 1 && rows[0].deaths == 0);
        CHECK(rows[1].kills == 0 && rows[1].deaths == 1);
        // Respawn after 5s (capture): the killing tick already ate 1s of
        // the timer, so still dead at +3s, back at +5s.
        CHECK(!m.findPlayer(b)->alive);
        m.update(3.0);
        CHECK(!m.findPlayer(b)->alive);
        m.update(2.0);
        CHECK(m.findPlayer(b)->alive);
        CHECK(m.findPlayer(b)->hp == Match::kPlayerMaxHp);
    }
    // ---------- match: full capture bot game makes progress ----------
    {
        EventBus bus; GameClock clock; RNG rng(9);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        m.botfill();
        CHECK(m.playerCount() == 10);
        for (int i = 0; i < 180; ++i) m.update(1.0);
        CHECK(m.capture()->score(0) + m.capture()->score(1) > 0.0f);
    }
    // ---------- match: full moba bot game runs ----------
    {
        EventBus bus; GameClock clock; RNG rng(10);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        m.botfill();
        for (int i = 0; i < 200; ++i) m.update(1.0);
        CHECK(m.moba()->minionCount() > 0); // waves flowing
        CHECK(!m.isOver());                // 200s won't topple a 5000-HP GOO
    }
    // ---------- match: moba respawn scales with match time ----------
    {
        EventBus bus; GameClock clock; RNG rng(11);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        uint64_t a = m.addBot(0);
        uint64_t b = m.addBot(1);
        for (int i = 0; i < 660; ++i) m.update(1.0); // 11 min in
        Recorder r; r.attach(bus, EventType::PlayerKilled);
        int ticks = 0;
        while (r.count(EventType::PlayerKilled) == 0 && ticks < 40) {
            // Re-stage each tick: a respawn during the fight must not stall.
            m.movePlayer(a, Vec3(0, 0, 0));
            m.movePlayer(b, Vec3(1, 0, 0));
            m.update(1.0); ++ticks;
        }
        CHECK(r.count(EventType::PlayerKilled) == 1);
        uint64_t victim = r.events[0].targetId;
        m.update(10.0);
        CHECK(!m.findPlayer(victim)->alive); // 5 + 11 = 16s respawn
        m.update(8.0);
        CHECK(m.findPlayer(victim)->alive);
        (void)a; (void)b;
    }
    // ---------- protocol: ModeState round-trip ----------
    {
        net::ModeState s;
        s.mode = "capture";
        s.score0 = 12.5f; s.score1 = 7.0f;
        s.pointOwners = {0, -1, 1};
        s.pointProg0 = {0.0f, 0.3f, 0.0f};
        s.pointProg1 = {0.0f, 0.0f, 0.8f};
        net::Message msg = net::encodeModeState(s);
        CHECK(msg.type == net::MsgType::ModeState);
        auto frame = net::encode(msg);
        auto back = net::decodeFrame(frame.data(), frame.size());
        CHECK(back.has_value());
        net::ModeState out;
        CHECK(net::decodeModeState(*back, out));
        CHECK(out.mode == "capture");
        CHECK(std::fabs(out.score0 - 12.5f) < 0.01f);
        CHECK(out.pointOwners == s.pointOwners);
        CHECK(out.pointProg1.size() == 3);
        CHECK(std::fabs(out.pointProg1[2] - 0.8f) < 0.01f);
        // Moba flavor.
        net::ModeState t;
        t.mode = "moba";
        t.baseHp0 = 5000.0f; t.baseHp1 = 1234.0f;
        t.towerHps = {600, 0, 300};
        t.minionCount = 42;
        net::Message m2 = net::encodeModeState(t);
        net::ModeState o2;
        CHECK(net::decodeModeState(m2, o2));
        CHECK(o2.mode == "moba");
        CHECK(std::fabs(o2.baseHp1 - 1234.0f) < 0.01f);
        CHECK(o2.towerHps.size() == 3);
        CHECK(o2.minionCount == 42);
    }
    // ---------- match: modeStateMessage reflects live state ----------
    {
        EventBus bus; GameClock clock; RNG rng(12);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        m.botfill();
        m.update(60.0);
        net::ModeState s;
        CHECK(net::decodeModeState(m.modeStateMessage(), s));
        CHECK(s.mode == "capture");
        CHECK(s.pointOwners.size() == 1); // Onslaught: single point
        Match m2(bus, clock, rng);
        CHECK(m2.start("moba"));
        m2.botfill();
        m2.update(60.0);
        net::ModeState t;
        CHECK(net::decodeModeState(m2.modeStateMessage(), t));
        CHECK(t.mode == "moba");
        CHECK(t.towerHps.size() == 12);
        CHECK(std::fabs(t.baseHp0 - MobaDefense::GOO_MAX_HP) < 0.01f);
    }
    // ---------- achievements: Turncoat handler verified ----------
    // 5v5 matches publish no ConversionPerformed (no conversion mechanic),
    // so Turncoat is verified-but-not-triggerable in 5v5.
    {
        EventBus bus;
        AchievementSystem a(bus);
        a.setLocalPlayer(1, FACTION_CTHULHU, 0);
        GameEvent e(EventType::ConversionPerformed);
        e.amount = 1.0f;
        e.faction = FACTION_NEUTRAL; // neutral soul: no turncoat
        bus.publish(e);
        CHECK(!a.isUnlocked("turncoat"));
        GameEvent e2(EventType::ConversionPerformed);
        e2.amount = 1.0f;
        e2.faction = 7; // a rival faction's soul turned
        bus.publish(e2);
        CHECK(a.isUnlocked("turncoat"));
    }
    // ---------- achievements: match events fire through a real match ----
    {
        EventBus bus; GameClock clock; RNG rng(13);
        AchievementSystem a(bus);
        a.setLocalPlayer(Match::kPlayerIdBase, FACTION_CTHULHU, 0);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t me = m.addPlayer("Henrique", 0);
        CHECK(me == Match::kPlayerIdBase);
        uint64_t foe = m.addBot(1);
        // Stage a capture by the local player's team.
        m.movePlayer(me, Vec3(0, 0, 0));
        m.movePlayer(foe, Vec3(200, 0, 200)); // far away, no contest
        int ticks = 0;
        while (!a.isUnlocked("standard_bearer") && ticks < 30) {
            m.update(1.0); ++ticks;
        }
        CHECK(a.isUnlocked("standard_bearer")); // PointCaptured faction==0
        // Stage a PvP kill by the local player.
        m.movePlayer(foe, Vec3(1, 0, 0));
        ticks = 0;
        while (!a.isUnlocked("first_blood") && ticks < 30) {
            m.update(1.0); ++ticks;
        }
        CHECK(a.isUnlocked("first_blood")); // PlayerKilled sourceId==localId
    }

    if (failures == 0)
        std::cout << "wave21: all " << checks << " checks passed\n";
    else
        std::cout << "wave21: " << failures << " of " << checks
                  << " checks FAILED\n";
    return failures == 0 ? 0 : 1;
}
