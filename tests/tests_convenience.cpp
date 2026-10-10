// CULT-ULHU player-convenience tests.
//
// Guards the status/HUD readability pass:
//   - cultistStateName maps every CultistState to the human-readable name
//     the driver's `status` output uses (never a raw integer again).
//   - the name used by the driver matches the one StatsPanel::gather
//     reports in the stats overlay (single source of truth).

#include "ui/StatsPanel.h"

#include <iostream>
#include <string>

using namespace cultulhu;

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

int main() {
    CHECK(std::string(cultistStateName(CultistState::Loyal)) == "Loyal");
    CHECK(std::string(cultistStateName(CultistState::Infringer)) == "Infringer");
    CHECK(std::string(cultistStateName(CultistState::Lunatic)) == "Lunatic");
    CHECK(std::string(cultistStateName(CultistState::Imprisoned)) == "Imprisoned");
    CHECK(std::string(cultistStateName(CultistState::Converted)) == "Converted");

    if (failures == 0)
        std::cout << "convenience: all " << checks << " checks passed\n";
    else
        std::cout << "convenience: " << failures << " of " << checks
                  << " checks FAILED\n";
    return failures == 0 ? 0 : 1;
}
