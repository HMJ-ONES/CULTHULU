#pragma once

#include <string>
#include <vector>

namespace cultulhu {

// The design doc calls for 12 beliefs; Dreams (the 12th) was chosen by the
// project owner and implemented in the playable-beta wave.
enum class Belief {
    Torture,
    Fear,
    Breeding,
    Chaos,
    Sacrifice,
    Conversion,
    War,
    Reconstruction,
    Trickery,
    Magic,
    Onslaught,
    Dreams,
    Count
};

inline const char* beliefName(Belief b) {
    switch (b) {
        case Belief::Torture:        return "Torture";
        case Belief::Fear:           return "Fear";
        case Belief::Breeding:       return "Breeding";
        case Belief::Chaos:          return "Chaos";
        case Belief::Sacrifice:       return "Sacrifice";
        case Belief::Conversion:      return "Conversion";
        case Belief::War:            return "War";
        case Belief::Reconstruction: return "Reconstruction";
        case Belief::Trickery:        return "Trickery";
        case Belief::Magic:           return "Magic";
        case Belief::Onslaught:       return "Onslaught";
        case Belief::Dreams:          return "Dreams";
        case Belief::Count:           return "Count";
    }
    return "Unknown";
}

// Canonical belief names in enum order ("Torture" .. "Dreams").
// Used by the driver's typo-tolerant belief help so it never drifts
// out of sync with the enum.
inline std::vector<std::string> beliefNamesList() {
    std::vector<std::string> out;
    for (int i = 0; i < static_cast<int>(Belief::Count); ++i)
        out.push_back(beliefName(static_cast<Belief>(i)));
    return out;
}

} // namespace cultulhu
