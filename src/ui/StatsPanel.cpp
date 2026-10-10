#include "ui/StatsPanel.h"

#include "cult/CultManager.h"
#include "entities/Units.h"
#include "exertion/ExertionSystem.h"
#include "power/PowerSystem.h"

namespace cultulhu {

const char* cultistStateName(CultistState s) {
    switch (s) {
        case CultistState::Loyal:      return "Loyal";
        case CultistState::Infringer:  return "Infringer";
        case CultistState::Lunatic:     return "Lunatic";
        case CultistState::Imprisoned: return "Imprisoned";
        case CultistState::Converted:  return "Converted";
    }
    return "Unknown";
}

StatsData StatsPanel::gather(CultManager& cult,
                             const ExertionSystem& exertion,
                             const PowerSystem& power,
                             const std::vector<net::KdaRow>& kdaRows) {
    StatsData d;

    for (size_t i = 0; i < cult.size(); ++i)
        ++d.cultCountsByState[cultistStateName(cult.at(i).state())];

    d.power = power.value();
    d.insurrectionRisk = cult.insurrectionRisk();

    const float* levels = exertion.levels();
    for (int i = 0; i < static_cast<int>(Belief::Count); ++i)
        d.beliefGauges[i] = levels[i];

    d.kdaTable = kdaRows;
    for (const auto& row : kdaRows)
        d.totalKills += row.kills;

    return d;
}

} // namespace cultulhu
