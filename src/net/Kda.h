#pragma once

#include <string>

namespace cultulhu {
namespace net {

// Shared kill/death/assist row contract. Produced by PlayerStatsTracker
// (src/net/PlayerStats) and consumed by the Tab stats overlay
// (src/ui/StatsPanel). Defined here so the two modules do not depend on
// each other's larger headers.
struct KdaRow {
    std::string name;
    int kills = 0;
    int deaths = 0;
    int assists = 0;
};

} // namespace net
} // namespace cultulhu
