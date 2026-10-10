// Wave 9d FUZZ TARGET 2 — command/directive parser.
//
// Feeds the command-line parsing layer random strings:
//   - tokenization exactly like the driver (std::istringstream >>)
//   - verb lookup against the driver's dispatch table
//   - directiveByName / beliefByName on arbitrary tokens (must reject
//     garbage gracefully, never crash)
//   - CommandSystem::issueCommand with fuzzed DirectiveType values
//     (valid, Count, negative, far out of range), extreme Vec3 targets,
//     and wild factions
//   - DirectiveResolved events with fuzzed tags (exercises the
//     DirectiveExecutor / ExertionSystem tag parsers)
// Mutations: token swaps, truncations, duplications, pure garbage, overlong
// inputs (10KB), unicode/whitespace edge cases. Deterministic on `seed`.

#include "fuzz/fuzz_driver.h"
#include "fuzz/fuzz_targets.h"
#include "fuzz/fuzz_util.h"

#include "core/Events.h"
#include "core/Vec3.h"

#include <cstdio>
#include <sstream>

using namespace cultulhu;

namespace fuzz {
namespace {

// Mirrors the verb dispatch table in src/driver/main.cpp's processLine.
bool isKnownVerb(const std::string& cmd) {
    static const char* const verbs[] = {
        "help", "status", "beliefs", "move", "camera", "look", "spawn",
        "belief", "rest", "command", "attack", "cast", "tick", "save",
        "load", "myip", "discover", "host", "join", "ready", "players",
        "startgame", "chat", "netent", "leave", "build", "menu", "combo",
        "chars", "addchar", "validate", "kda", "interact", "jump",
        "sprint", "rmb", "ambient", "dungeon", "quit", "exit",
    };
    for (const char* v : verbs)
        if (cmd == v) return true;
    return false;
}

DirectiveType fuzzDirective(FuzzRng& rng) {
    const double r = rng.uni01();
    if (r < 0.70)
        return static_cast<DirectiveType>(
            rng.below(static_cast<size_t>(DirectiveType::Count)));
    if (r < 0.85) return DirectiveType::Count;
    return static_cast<DirectiveType>(rng.range(-4, 24)); // out of range
}

void fuzzParseLine(BetaGame& g, FuzzRng& rng, const std::string& line) {
    // 1) Tokenize exactly like the driver.
    std::istringstream in(line);
    std::string cmd;
    in >> cmd;
    (void)isKnownVerb(cmd);

    // 2) Name parsers must reject garbage gracefully.
    std::string t1, t2, t3;
    in >> t1 >> t2 >> t3;
    (void)directiveByName(cmd);
    (void)beliefByName(cmd);
    (void)directiveByName(t1);
    (void)beliefByName(t1);
    (void)directiveByName(t2);
    (void)beliefByName(t2);
    (void)directiveByName(t3);
    (void)beliefByName(t3);

    // 3) issueCommand with a fuzzed directive enum / target / faction.
    if (rng.chance(0.5)) {
        const DirectiveType d = fuzzDirective(rng);
        const Vec3 target(randomFloat(rng), randomFloat(rng),
                          randomFloat(rng));
        const int faction = rng.chance(0.8)
                                ? static_cast<int>(rng.range(-2, 6))
                                : static_cast<int>(rng.range(-100000, 100000));
        gameCommands(g).issueCommand(d, target, faction);
    }

    // 4) DirectiveResolved with a fuzzed tag: the executor and the
    // exertion system both parse "DirectiveName/OutcomeName" tags.
    if (rng.chance(0.25)) {
        GameEvent e(EventType::DirectiveResolved);
        e.tag = randomTag(rng);
        e.amount = randomFloat(rng);
        e.faction = static_cast<int>(rng.range(-100000, 100000));
        e.pos = Vec3(randomFloat(rng), randomFloat(rng), randomFloat(rng));
        e.sourceId = rng.u64();
        e.targetId = rng.u64();
        gameBus(g).publish(e);
    }
}

} // namespace

int runTarget2(uint64_t seed, size_t iterations) {
    logf("[fuzz] target2 command-parser: seed=%llu iterations=%zu\n",
                 (unsigned long long)seed, iterations);
    FuzzRng rng(seed);
    // discover/join lines are included here: parsing only, no sockets.
    const std::vector<std::string> corpus = driverCommandCorpus(true);
    BetaGame* g = newGame();
    int rc = 0;

    for (size_t i = 0; i < iterations; ++i) {
        // issueCommand can recruit cultists (ConvertCampaign) and the
        // executor can accumulate operations: rebuild periodically to
        // bound state growth (deterministic schedule).
        if (i > 0 && i % 2048 == 0) {
            deleteGame(g);
            g = newGame();
        }
        const std::string line = mutateLine(rng, corpus);
        Watchdog wd;
        wd.start();
        bool threw = false;
        std::string what;
        try {
            fuzzParseLine(*g, rng, line);
        } catch (const std::exception& ex) {
            threw = true;
            what = std::string("std::exception: ") + ex.what();
        } catch (...) {
            threw = true;
            what = "unknown exception";
        }
        const double ms = wd.elapsedMs();
        if (threw) {
            reportFailure("command-parser", seed, i,
                          "line=" + escapeForReport(line), what);
            rc = 1;
            break;
        }
        if (ms > Watchdog::kHangThresholdMs) {
            char w[96];
            std::snprintf(w, sizeof(w),
                          "hang: single parse took %.1fms (> %.0fms)",
                          ms, Watchdog::kHangThresholdMs);
            reportFailure("command-parser", seed, i,
                          "line=" + escapeForReport(line), w);
            rc = 1;
            break;
        }
    }

    deleteGame(g);
    logf("[fuzz] target2 command-parser: %s\n",
                 rc == 0 ? "clean" : "BUG FOUND");
    return rc;
}

} // namespace fuzz
