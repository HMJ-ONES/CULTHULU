#include "sim/SimReporter.h"

#include "beliefs/Belief.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <numeric>

namespace cultulhu {
namespace sim {

namespace fs = std::filesystem;

SimReporter::SimReporter(const std::string& dir) : dir_(dir) {
    fs::create_directories(dir_);
}

namespace {
void writeHeader(std::ofstream& f, const char* const* cols, int n) {
    for (int i = 0; i < n; ++i) {
        if (i) f << ',';
        f << cols[i];
    }
    f << '\n';
}

int deathsTotal(const SimMetrics& m) {
    int n = 0;
    for (const auto& kv : m.deathsByCause) n += kv.second;
    return n;
}

int deathsOf(const SimMetrics& m, const std::string& cause) {
    auto it = m.deathsByCause.find(cause);
    return it == m.deathsByCause.end() ? 0 : it->second;
}

// Median of ints, ignoring -1 ("never") entries. Returns -1 if none reached.
double medianReached(std::vector<int> v) {
    v.erase(std::remove(v.begin(), v.end(), -1), v.end());
    if (v.empty()) return -1.0;
    std::sort(v.begin(), v.end());
    const size_t n = v.size();
    return (n % 2 == 1) ? static_cast<double>(v[n / 2])
                        : (v[n / 2 - 1] + v[n / 2]) / 2.0;
}

double mean(const std::vector<double>& v) {
    if (v.empty()) return 0.0;
    return std::accumulate(v.begin(), v.end(), 0.0) / v.size();
}
} // namespace

void SimReporter::writeRunCsv(const std::vector<SimMetrics>& runs) {
    std::ofstream f(dir_ + "/runs.csv");
    const char* cols[] = {"scenario", "seed",      "final_power",
                          "ticks_to_500", "ticks_to_800", "revolts",
                          "risk_events",  "max_risk",     "conversions",
                          "war_kills",    "deaths_one_hit", "deaths_feral",
                          "deaths_lost",  "deaths_sacrificed",
                          "ex_torture",   "ex_fear",      "ex_breeding",
                          "ex_chaos",     "ex_sacrifice", "ex_conversion",
                          "ex_war",       "ex_recon",     "ex_trickery",
                          "ex_magic",     "ex_onslaught", "ex_dreams"};
    writeHeader(f, cols, 26);
    for (const auto& m : runs) {
        f << m.scenario << ',' << m.seed << ',' << m.finalPower << ','
          << m.ticksTo500 << ',' << m.ticksTo800 << ',' << m.revolts << ','
          << m.riskEvents << ',' << m.maxRisk << ',' << m.conversions << ','
          << m.warKills << ',' << deathsOf(m, "one_hit") << ','
          << deathsOf(m, "feral") << ',' << deathsOf(m, "lost") << ','
          << deathsOf(m, "sacrificed");
        for (int i = 0; i < 12; ++i) f << ',' << m.finalExertion[i];
        f << '\n';
    }
}

void SimReporter::writeCurveCsv(const std::vector<SimMetrics>& runs) {
    std::ofstream f(dir_ + "/power_curves.csv");
    const char* cols[] = {"scenario", "seed", "tick", "power"};
    writeHeader(f, cols, 4);
    for (const auto& m : runs)
        for (size_t i = 0; i < m.powerCurve.size(); ++i)
            f << m.scenario << ',' << m.seed << ',' << (i * 100) << ','
              << m.powerCurve[i] << '\n';
}

void SimReporter::writeSummaryCsv(const std::vector<SimMetrics>& runs) {
    std::ofstream f(dir_ + "/summary.csv");
    const char* cols[] = {"scenario",
                          "runs",
                          "reach500_rate",
                          "median_ticks_to_500",
                          "reach800_rate",
                          "median_ticks_to_800",
                          "mean_final_power",
                          "mean_revolts",
                          "mean_conversions",
                          "mean_war_kills",
                          "mean_deaths",
                          "mean_max_risk",
                          "mean_final_max_exertion"};
    writeHeader(f, cols, 13);

    // Group by scenario, preserving first-seen order.
    std::vector<std::string> order;
    std::map<std::string, std::vector<const SimMetrics*>> groups;
    for (const auto& m : runs) {
        if (!groups.count(m.scenario)) order.push_back(m.scenario);
        groups[m.scenario].push_back(&m);
    }

    for (const auto& name : order) {
        const auto& g = groups[name];
        std::vector<int> t500, t800;
        std::vector<double> fp, rv, cv, wk, dt, mr, pe;
        for (const SimMetrics* m : g) {
            t500.push_back(m->ticksTo500);
            t800.push_back(m->ticksTo800);
            fp.push_back(m->finalPower);
            rv.push_back(m->revolts);
            cv.push_back(m->conversions);
            wk.push_back(m->warKills);
            dt.push_back(deathsTotal(*m));
            mr.push_back(m->maxRisk);
            float peak = 0.0f;
            for (int i = 0; i < 12; ++i)
                peak = std::max(peak, m->finalExertion[i]);
            pe.push_back(peak);
        }
        const double n = static_cast<double>(g.size());
        const double r500 = std::count_if(t500.begin(), t500.end(),
                                         [](int t) { return t >= 0; }) / n;
        const double r800 = std::count_if(t800.begin(), t800.end(),
                                         [](int t) { return t >= 0; }) / n;
        f << name << ',' << g.size() << ',' << r500 << ','
          << medianReached(t500) << ',' << r800 << ',' << medianReached(t800)
          << ',' << mean(fp) << ',' << mean(rv) << ',' << mean(cv) << ','
          << mean(wk) << ',' << mean(dt) << ',' << mean(mr) << ','
          << mean(pe) << '\n';
    }
}

void SimReporter::writeAll(const std::vector<SimMetrics>& runs) {
    writeRunCsv(runs);
    writeCurveCsv(runs);
    writeSummaryCsv(runs);
}

} // namespace sim
} // namespace cultulhu
