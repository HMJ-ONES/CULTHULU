// Wave 9a: standalone balance-sim driver.
//
// IMPORTANT: this file is intentionally inert in the CMake build (the body
// is guarded by CULTULHU_SIM_MAIN) so the existing library and test targets
// keep linking without a duplicate `main`. Build it by hand against the
// already-built static library:
//
//   cd ~/workspace/cult-ulhu && cd build && cmake --build .   # refresh lib
//   cd ~/workspace/cult-ulhu
//   g++ -std=c++17 -O2 -DCULTULHU_SIM_MAIN -Isrc src/sim/main.cpp \
//       build/libcultulhu.a -o /tmp/cultulhu_sim
//   /tmp/cultulhu_sim [output_dir]     # default: sim_reports
//
// Runs the full scenario matrix (18 scenarios x 8 seeds x 3600 ticks) and
// writes runs.csv, power_curves.csv and summary.csv.

#ifdef CULTULHU_SIM_MAIN

#include "sim/SimReporter.h"
#include "sim/SimRunner.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace cultulhu::sim;

int main(int argc, char** argv) {
    const std::string outDir = (argc > 1) ? argv[1] : "sim_reports";

    SimRunner runner(/*ticks=*/3600, /*seedsPerScenario=*/8);
    const auto scenarios = buildScenarios();
    std::cout << "cult-ulhu balance sim: " << scenarios.size()
              << " scenarios x " << runner.seedsPerScenario() << " seeds x "
              << runner.ticks() << " ticks\n";

    const std::vector<SimMetrics> runs = runner.runAll();

    SimReporter reporter(outDir);
    reporter.writeAll(runs);
    std::cout << "wrote " << outDir
              << "/runs.csv, power_curves.csv, summary.csv (" << runs.size()
              << " runs)\n";

    // Quick console table: scenario | median ticks to 500 | mean final power.
    std::cout << std::left << std::setw(28) << "scenario"
              << std::right << std::setw(12) << "med_t500"
              << std::setw(12) << "med_t800" << std::setw(14)
              << "mean_power" << '\n';
    std::string cur;
    for (const auto& m : runs) {
        if (m.scenario == cur) continue;
        cur = m.scenario;
        std::vector<int> t5, t8;
        double fp = 0.0;
        int n = 0;
        for (const auto& r : runs) {
            if (r.scenario != cur) continue;
            t5.push_back(r.ticksTo500);
            t8.push_back(r.ticksTo800);
            fp += r.finalPower;
            ++n;
        }
        auto med = [](std::vector<int> v) -> double {
            v.erase(std::remove(v.begin(), v.end(), -1), v.end());
            if (v.empty()) return -1;
            std::sort(v.begin(), v.end());
            const size_t k = v.size();
            return (k % 2) ? v[k / 2] : (v[k / 2 - 1] + v[k / 2]) / 2.0;
        };
        std::cout << std::left << std::setw(28) << cur << std::right
                  << std::setw(12) << med(t5) << std::setw(12) << med(t8)
                  << std::setw(14) << std::fixed << std::setprecision(1)
                  << fp / n << '\n';
    }
    return 0;
}

#endif // CULTULHU_SIM_MAIN
