#pragma once

// Wave 7 (Worker B): Tab stats overlay DATA provider. The engine renders
// the overlay; this module only fills a plain StatsData struct from the
// live game systems — no I/O, no formatting.
//
// Field notes:
//   cultCountsByState .. roster headcount keyed by cultist state name
//                        ("Loyal", "Infringer", "Lunatic", "Imprisoned",
//                        "Converted")
//   totalKills ......... multiplayer kill ledger: sum of KDA kills across
//                        the kda rows handed to gather(). Single-player
//                        drivers feed the same rows so the number is always
//                        meaningful. (Deaths/assists stay per-row in the
//                        table; the headline number is kills.)
//   power .............. Cthulhu's power, 0..1000 (PowerSystem)
//   insurrectionRisk ... cult revolt risk, 0..100 (CultManager)
//   beliefGauges ....... belief exertion (fervor) 0..100 per belief,
//                        indexed by static_cast<int>(Belief)
//   kdaTable ........... per-player kill/death/assist rows (net::KdaRow)

#include "beliefs/Belief.h"
#include "net/Kda.h"

#include <map>
#include <string>
#include <vector>

namespace cultulhu {

class CultManager;
class ExertionSystem;
class PowerSystem;

struct StatsData {
    std::map<std::string, int> cultCountsByState;
    int totalKills = 0;
    float power = 0.0f;
    float insurrectionRisk = 0.0f;
    float beliefGauges[static_cast<int>(Belief::Count)] = {};
    std::vector<net::KdaRow> kdaTable;
};

class StatsPanel {
public:
    // Fill StatsData from the live systems. kdaRows are the multiplayer
    // (or single-player) kill ledger rows, e.g. from PlayerStatsTracker.
    // cult is non-const because CultManager::at() exposes mutable access.
    static StatsData gather(CultManager& cult,
                            const ExertionSystem& exertion,
                            const PowerSystem& power,
                            const std::vector<net::KdaRow>& kdaRows);
};

} // namespace cultulhu
