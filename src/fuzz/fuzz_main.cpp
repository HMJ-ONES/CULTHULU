// CULT-ULHU wave-9d fuzz harness driver.
//
// Runs the three fuzz targets (event bus, command parser, driver REPL
// dispatch). Each target prints its deterministic seed up front; on a bug
// it prints the seed + iteration + exact failing input, aborts that target,
// and the harness moves on to the next target.
//
// Usage:
//   cultulhu_fuzz [all|target1|target2|target3] [iterations] [seed]
//
// Defaults: all targets, 50000 iterations each, fixed per-target seeds.
// A custom seed applies to a single-target run and fully reproduces it.
//
// Exit code: 0 if every target ran clean, 1 if any target found a bug.

#include "fuzz/fuzz_driver.h"
#include "fuzz/fuzz_targets.h"
#include "fuzz/fuzz_util.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#ifndef _WIN32
#include <unistd.h>  // not used directly here; kept for POSIX builds
#endif

namespace {

constexpr uint64_t kSeed1 = 0xC0FFEE1u;
constexpr uint64_t kSeed2 = 0xC0FFEE2u;
constexpr uint64_t kSeed3 = 0xC0FFEE3u;
constexpr size_t kDefaultIterations = 50000;

bool makeSandbox() {
    char dir[] = "/tmp/cultulhu_fuzz_XXXXXX";
    if (::mkdtemp(dir) == nullptr) {
        fuzz::logf("[fuzz] mkdtemp failed\n");
        return false;
    }
    if (::chdir(dir) != 0) {
        fuzz::logf("[fuzz] chdir to %s failed\n", dir);
        return false;
    }
    fuzz::logf("[fuzz] sandbox dir: %s\n", dir);
    return true;
}

} // namespace

int main(int argc, char** argv) {
    std::string which = (argc > 1) ? argv[1] : "all";
    size_t iterations = kDefaultIterations;
    if (argc > 2) iterations = static_cast<size_t>(std::strtoull(argv[2], nullptr, 10));
    uint64_t seedOverride = 0;
    bool hasSeedOverride = false;
    if (argc > 3) {
        seedOverride = std::strtoull(argv[3], nullptr, 0);
        hasSeedOverride = true;
    }

    const bool run1 = (which == "all" || which == "target1");
    const bool run2 = (which == "all" || which == "target2");
    const bool run3 = (which == "all" || which == "target3");
    if (!run1 && !run2 && !run3) {
        std::fprintf(stderr, "usage: %s [all|target1|target2|target3] "
                             "[iterations] [seed]\n", argv[0]);
        return 2;
    }

    std::fprintf(stderr,
                 "[fuzz] CULT-ULHU wave-9d harness: %s, %zu iterations/target\n",
                 which.c_str(), iterations);

    // File side effects from the driver (save/addchar) stay in a sandbox.
    if (!makeSandbox()) return 2;

    // The driver is chatty; silence it for the whole run. Failure reports
    // bypass iostreams and still reach stderr.
    fuzz::StreamSilencer silence;
    fuzz::setReportFd(silence.reportFd());

    int rc = 0;
    using clock = std::chrono::steady_clock;
    if (run1) {
        const auto t0 = clock::now();
        rc |= fuzz::runTarget1(hasSeedOverride ? seedOverride : kSeed1,
                               iterations);
        fuzz::logf("[fuzz] target1 wall time: %.1fs\n",
                     std::chrono::duration<double>(clock::now() - t0).count());
    }
    if (run2) {
        const auto t0 = clock::now();
        rc |= fuzz::runTarget2(hasSeedOverride ? seedOverride : kSeed2,
                               iterations);
        fuzz::logf("[fuzz] target2 wall time: %.1fs\n",
                     std::chrono::duration<double>(clock::now() - t0).count());
    }
    if (run3) {
        const auto t0 = clock::now();
        rc |= fuzz::runTarget3(hasSeedOverride ? seedOverride : kSeed3,
                               iterations);
        fuzz::logf("[fuzz] target3 wall time: %.1fs\n",
                     std::chrono::duration<double>(clock::now() - t0).count());
    }

    fuzz::logf("[fuzz] done: %s\n",
                 rc == 0 ? "ALL TARGETS CLEAN" : "BUGS FOUND (see above)");
    return rc;
}
