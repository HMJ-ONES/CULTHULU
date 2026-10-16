// CULT-ULHU micro-dev regression tests: held-fast alert.
//
// A stun/root landing on a match fighter was invisible — the player only
// found out when their next `mcast` fizzled "held fast". Match now
// publishes CCApplied once on the rising edge of a fighter's ccTimer
// (re-stuns while already held stay silent), the driver prints
// "*** HELD FAST — Ns of crowd control! ***" for the joined human, the
// exported match log gets a CC line, and fxForEvent resolves it to the
// existing "stun" preset (pinned in tests_wave36). These pin:
//
// 1. Contract: sourceId = victim id, tag = name, faction = team,
//    amount = CC time still held (post-tick decay), pos = victim pos.
// 2. Exactly-once: refreshing the stun while held re-fires nothing.
// 3. Re-arm: after the CC expires, a fresh application fires again.
// 4. MatchLogger records one CC line per application (the exported
//    GitHub story gets the held-fast beat).
//
// Single-player matches: no enemy bot can land a stray stun and make the
// counts flaky.

#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "modes/Match.h"
#include "modes/MatchLogger.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace cultulhu;

static int failures = 0;
#define CHECK(cond)                                        \
    do {                                                   \
        if (!(cond)) {                                     \
            std::cout << "FAIL line " << __LINE__ << ": "  \
                      << #cond << "\n";                     \
            ++failures;                                    \
        }                                                  \
    } while (0)

struct CcHit {
    uint64_t sourceId;
    int faction;
    float amount;
    std::string tag;
};

static void tickMatch(Match& m, int seconds) {
    for (int i = 0; i < seconds && !m.isOver(); ++i) m.update(1.0);
}

static int countKind(const MatchLogger& log, const std::string& kind) {
    int n = 0;
    for (const auto& e : log.events())
        if (e.kind == kind) ++n;
    return n;
}

int main() {
    // ---------- 1: contract + first application ----------
    {
        EventBus bus; GameClock clock; RNG rng(7);
        std::vector<CcHit> hits;
        bus.subscribe(EventType::CCApplied,
                      [&hits](const GameEvent& e) {
                          hits.push_back({e.sourceId, e.faction,
                                          e.amount, e.tag});
                      });
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        const uint64_t a = m.addBot(0);
        const Match::Player* pl = m.findPlayer(a);
        CHECK(pl != nullptr);

        CHECK(m.debugStunPlayer(a, 2.5));
        CHECK(hits.empty()); // fires on the tick, not on application
        m.update(1.0);
        CHECK(hits.size() == 1);
        const Match::Player* p = m.findPlayer(a);
        CHECK(hits[0].sourceId == a);
        CHECK(hits[0].faction == 0);
        CHECK(hits[0].tag == p->name);
        CHECK(!hits[0].tag.empty());
        // 2.5s applied, 1.0s decayed by the tick: the player is told the
        // time they still have to wait out.
        CHECK(std::fabs(hits[0].amount - 1.5f) < 1e-4f);
    }

    // ---------- 2: exactly-once while held ----------
    {
        EventBus bus; GameClock clock; RNG rng(8);
        std::vector<CcHit> hits;
        bus.subscribe(EventType::CCApplied,
                      [&hits](const GameEvent& e) {
                          hits.push_back({e.sourceId, e.faction,
                                          e.amount, e.tag});
                      });
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        const uint64_t a = m.addBot(0);

        CHECK(m.debugStunPlayer(a, 3.0));
        m.update(1.0);
        CHECK(hits.size() == 1);
        CHECK(std::fabs(hits[0].amount - 2.0f) < 1e-4f);
        // Refresh while still held: max() keeps the longer timer but the
        // rising edge already fired, so nothing new publishes.
        CHECK(m.debugStunPlayer(a, 5.0));
        m.update(1.0);
        CHECK(hits.size() == 1);
        // Natural decay across the stun fires nothing either.
        tickMatch(m, 3);
        CHECK(hits.size() == 1);
    }

    // ---------- 3: re-arm after the CC expires ----------
    {
        EventBus bus; GameClock clock; RNG rng(9);
        std::vector<CcHit> hits;
        bus.subscribe(EventType::CCApplied,
                      [&hits](const GameEvent& e) {
                          hits.push_back({e.sourceId, e.faction,
                                          e.amount, e.tag});
                      });
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        const uint64_t a = m.addBot(0);

        CHECK(m.debugStunPlayer(a, 1.5));
        m.update(1.0);
        CHECK(hits.size() == 1);
        tickMatch(m, 2); // 0.5s remaining decays to zero; still silent
        CHECK(hits.size() == 1);
        CHECK(m.debugStunPlayer(a, 2.0)); // fresh stun: fires again
        m.update(1.0);
        CHECK(hits.size() == 2);
        CHECK(std::fabs(hits[1].amount - 1.0f) < 1e-4f);
    }

    // ---------- 4: the exported story gets the held-fast beat ----------
    {
        EventBus bus; GameClock clock; RNG rng(10);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        const uint64_t a = m.addPlayer("Hero", 0);
        MatchLogger log;
        log.attach(m);
        CHECK(m.debugStunPlayer(a, 2.5));
        m.update(1.0);
        CHECK(countKind(log, "CC") == 1);
        std::string ccText;
        for (const auto& e : log.events())
            if (e.kind == "CC") ccText = e.text;
        CHECK(ccText.find("Hero") != std::string::npos);
        CHECK(ccText.find("held fast") != std::string::npos);
        // Refresh while held: the story doesn't get a second line.
        CHECK(m.debugStunPlayer(a, 5.0));
        m.update(1.0);
        CHECK(countKind(log, "CC") == 1);
    }

    if (failures == 0) std::cout << "ALL CC ALERT CHECKS PASSED\n";
    return failures == 0 ? 0 : 1;
}
