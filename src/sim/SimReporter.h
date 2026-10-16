// Wave 9a: SimReporter — writes the balance-sim results to disk:
//   runs.csv         one row per (scenario, seed) with all scalar metrics
//   power_curves.csv power-over-time samples (long format)
//   summary.csv      one row per scenario with aggregate statistics
// All files land in <dir> (created if missing).

#pragma once

#include "sim/SimRunner.h"

#include <string>
#include <vector>

namespace cultulhu {
namespace sim {

class SimReporter {
public:
    explicit SimReporter(const std::string& dir);

    void writeRunCsv(const std::vector<SimMetrics>& runs);
    void writeCurveCsv(const std::vector<SimMetrics>& runs);
    void writeSummaryCsv(const std::vector<SimMetrics>& runs);
    void writeAll(const std::vector<SimMetrics>& runs);

    const std::string& dir() const { return dir_; }

private:
    std::string dir_;
};

} // namespace sim
} // namespace cultulhu
