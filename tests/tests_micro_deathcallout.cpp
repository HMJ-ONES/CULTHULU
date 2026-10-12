// CULT-ULHU micro-dev regression tests: death callout ("slain by").
//
// Deaths used to pass silently on the console — the match log recorded
// the KILL line, but a player watching the fight never saw who went down
// until their respawn line. Every death now publishes EventType::PlayerKilled
// (already the production contract), and the driver's console subscriber
// names the slaying, with a personal "You were slain by ..." line when the
// victim is the joined human (matched by player id). These pin, through
// the production match loop:
//
// 1. A real duel publishes exactly one PlayerKilled per death, with the
//    killer's player id as sourceId and the victim's as targetId.
// 2. The exported match log still carries the KILL line for that death.
// 3. The driver-side naming logic (same predicate as src/driver/main.cpp)
//    personalizes when the victim is the human ("You were slain by Bot1"),
//    names bot-on-bot slayings, and falls back to "the minions" when the
//    killer is not a player (minion/tower entity id).
// 4. The staging is the proven deterministic duel (tests_micro_firstblood
//    / tests_micro_respawn): RNG(8), cthulhu_avatar vs cthulhu_avatar,
//    1 m apart, team 0 facing team 1.

#include "modes/Match.h"
#include "modes/MatchLogger.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"

#include <cstdint>
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

// Same naming logic as the driver's PlayerKilled console subscriber in
// src/driver/main.cpp: personal "You were slain by ..." when the victim
// is the joined human (by player id), otherwise name both fighters, with
// "the minions" for non-player killers.
static std::string consoleLine(const Match& m, const GameEvent& e,
                               uint64_t humanId) {
    const Match::Player* k = m.findPlayer(e.sourceId);
    const Match::Player* v = m.findPlayer(e.targetId);
    const std::string ks = k ? k->name : "the minions";
    if (v && v->id == humanId) return "*** You were slain by " + ks + " ***";
    const std::string vs = v ? v->name : "an unfortunate soul";
    return "*** " + vs + " was slain by " + ks + " ***";
}

struct Hit {
    uint64_t sourceId;
    uint64_t targetId;
};

static void duelUntilDeath(Match& m, uint64_t victimId) {
    int ticks = 0;
    while (m.findPlayer(victimId)->alive && ticks < 60) {
        m.update(1.0);
        ++ticks;
    }
    CHECK(!m.findPlayer(victimId)->alive);
    CHECK(ticks < 60);
}

int main() {
    // 1-2: bot-vs-bot duel through the production killPlayer path —
    // exactly one PlayerKilled, killer = a, victim = b, and the match
    // log carries the KILL line naming both.
    {
        EventBus bus; GameClock clock; RNG rng(8);
        std::vector<Hit> hits;
        bus.subscribe(EventType::PlayerKilled, [&hits](const GameEvent& e) {
            hits.push_back({e.sourceId, e.targetId});
        });
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        MatchLogger logger;
        logger.attach(m);
        const uint64_t a = m.addBot(0);
        const uint64_t b = m.addBot(1);
        CHECK(m.setPlayerKit(a, "cthulhu_avatar"));
        CHECK(m.setPlayerKit(b, "cthulhu_avatar"));
        m.movePlayer(a, Vec3(0, 0, 0));
        m.movePlayer(b, Vec3(1, 0, 0));
        CHECK(m.setFacingYaw(a, 0.0f));

        duelUntilDeath(m, b);
        CHECK(hits.size() == 1);
        CHECK(hits[0].sourceId == a);
        CHECK(hits[0].targetId == b);

        int kills = 0;
        for (const auto& e : logger.events()) {
            if (e.kind == "KILL") {
                ++kills;
                CHECK(e.text.find("Bot1") != std::string::npos);
                CHECK(e.text.find("Bot2") != std::string::npos);
            }
        }
        CHECK(kills == 1);

        // 2b: the console naming logic names a bot-on-bot slaying.
        GameEvent e(EventType::PlayerKilled);
        e.sourceId = hits[0].sourceId;
        e.targetId = hits[0].targetId;
        CHECK(consoleLine(m, e, 0) ==
              "*** Bot2 was slain by Bot1 ***");
    }

    // 3: human victim — joinAsHuman takes the victim's slot (name "You").
    // The PlayerKilled id contract is pinned by the real duel above; here
    // we pin that the naming logic personalizes when the event's targetId
    // is the joined human's id (the driver compares e.targetId's player
    // against humanMatchId).
    {
        EventBus bus; GameClock clock; RNG rng(8);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        const uint64_t a = m.addBot(0);
        const uint64_t b = m.addBot(1);
        const uint64_t human = m.joinAsHuman(1);
        CHECK(human == b); // takes over the team-1 bot's slot
        CHECK(m.findPlayer(human)->name == "You");

        // The event a real death of the human would publish: killer = a,
        // victim = the human's player id.
        GameEvent e(EventType::PlayerKilled);
        e.sourceId = a;
        e.targetId = human;
        CHECK(consoleLine(m, e, human) == "*** You were slain by Bot1 ***");
    }

    // 4: non-player killer (minion/tower entity id) falls back to
    // "the minions" instead of a bogus id string.
    {
        EventBus bus; GameClock clock; RNG rng(8);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        const uint64_t a = m.addBot(0);
        GameEvent e(EventType::PlayerKilled);
        e.sourceId = 42; // minion/tower entity id — not a player id
        e.targetId = a;
        CHECK(consoleLine(m, e, 0) == "*** Bot1 was slain by the minions ***");
        e.targetId = 7777; // unknown victim id — never crash on the name
        CHECK(consoleLine(m, e, 0) ==
              "*** an unfortunate soul was slain by the minions ***");
    }

    if (failures == 0) std::cout << "tests_micro_deathcallout: ALL OK\n";
    return failures ? 1 : 0;
}
