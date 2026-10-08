// Wave 9d FUZZ TARGET 3 — driver REPL interpreter.
//
// Feeds the REAL driver dispatch (processLine from src/driver/main.cpp)
// random command lines using the same mutation strategy as target 2:
// mutations of valid commands, token soup, pure garbage, overlong inputs
// (10KB), unicode/whitespace edge cases. Must not crash, throw, or hang;
// invalid input must be rejected gracefully.
//
// Notes:
//   - `discover`/`join` verbs are excluded from the corpus: they perform
//     blocking network I/O by design (beacon listen up to 15s, TCP connect
//     timeout 5s) and would trip the hang watchdog without exercising any
//     game parsing. Their argument parsing is still covered by target 2.
//   - The game is rebuilt every kResetEvery iterations to bound state
//     growth (spawned cultists/world entities/construction sites).
//   - File side effects (save/addchar) land in a mkdtemp sandbox dir.
// Deterministic on `seed`.

#include "fuzz/fuzz_driver.h"
#include "fuzz/fuzz_targets.h"
#include "fuzz/fuzz_util.h"

#include <cstdio>
#include <cstdlib>

namespace fuzz {

int runTarget3(uint64_t seed, size_t iterations) {
    logf("[fuzz] target3 driver-dispatch: seed=%llu iterations=%zu\n",
                 (unsigned long long)seed, iterations);
    // FUZZ_TRACE=1: log every input line before dispatch (crash triage).
    const bool trace = std::getenv("FUZZ_TRACE") != nullptr;
    FuzzRng rng(seed);
    // No discover/join: blocking network I/O by design (see above).
    const std::vector<std::string> corpus = driverCommandCorpus(false);
    BetaGame* g = newGame();
    NetSession* n = newNetSession();
    int rc = 0;

    constexpr size_t kResetEvery = 512;
    for (size_t i = 0; i < iterations; ++i) {
        if (i > 0 && i % kResetEvery == 0) {
            deleteGame(g);
            g = newGame();
        }
        const std::string line = mutateLine(rng, corpus);
        if (trace) logf("[trace3] iter=%zu line=%s\n", i,
                        escapeForReport(line).c_str());
        Watchdog wd;
        wd.start();
        bool threw = false;
        std::string what;
        try {
            // Return value (quit/exit) is intentionally ignored: the
            // session keeps going so later inputs are still exercised.
            (void)processLine(*g, *n, line);
        } catch (const std::exception& ex) {
            threw = true;
            what = std::string("std::exception: ") + ex.what();
        } catch (...) {
            threw = true;
            what = "unknown exception";
        }
        const double ms = wd.elapsedMs();
        if (threw) {
            reportFailure("driver-dispatch", seed, i,
                          "line=" + escapeForReport(line), what);
            rc = 1;
            break;
        }
        if (ms > Watchdog::kHangThresholdMs) {
            char w[96];
            std::snprintf(w, sizeof(w),
                          "hang: single dispatch took %.1fms (> %.0fms)",
                          ms, Watchdog::kHangThresholdMs);
            reportFailure("driver-dispatch", seed, i,
                          "line=" + escapeForReport(line), w);
            rc = 1;
            break;
        }
    }

    deleteNetSession(n);
    deleteGame(g);
    logf("[fuzz] target3 driver-dispatch: %s\n",
                 rc == 0 ? "clean" : "BUG FOUND");
    return rc;
}

} // namespace fuzz
