#pragma once

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

} // namespace cultulhu
