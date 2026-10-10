#pragma once

// CULT-ULHU wave 26: seeded relic-name generator. Every relic spawn gets a
// name with identity ("the Chalice of Gnawing Shadows") instead of a bare
// stat stick, so claiming one is a discovery moment. Deterministic per RNG
// draw — same seed, same name.

#include "core/RNG.h"

#include <string>
#include <vector>

namespace cultulhu {

inline std::string generateRelicName(RNG& rng) {
    static const std::vector<std::string> forms = {
        "Chalice", "Idol", "Dagger", "Mask", "Bell", "Lens",
        "Crown", "Flute", "Mirror", "Coil", "Brand", "Seal",
    };
    static const std::vector<std::string> epithets = {
        "of Gnawing Shadows", "of the Drowned Choir", "of Hollow Stars",
        "of the Ninth Tide", "of Whispered Debts", "of the Pale Womb",
        "of Sundered Vows", "of the Blind Moon", "of Crawling Frost",
        "of the Last Litany", "of Dreaming Stone", "of the Starved Deep",
    };
    const std::string& form =
        forms[static_cast<size_t>(rng.intRange(0, (int)forms.size() - 1))];
    const std::string& epithet =
        epithets[static_cast<size_t>(rng.intRange(0, (int)epithets.size() - 1))];
    return "the " + form + " " + epithet;
}

// One-line flavor per creature species for first-sighting codex entries.
inline std::string speciesFlavor(const std::string& species) {
    if (species == "dhole") return "A burrowing enormity: all hunger, no eyes.";
    if (species == "ghoul") return "It wears the graveyard like a coat.";
    if (species == "wraith") return "Where it passes, candles forget fire.";
    if (species == "pale_wight") return "A drowned prayer that learned to walk.";
    if (species == "charnel_imp") return "Small, quick, and full of teeth.";
    if (species == "skittering_ghoul") return "It heard you before you saw it.";
    if (species == "risen_dead") return "The grave gave back, reluctantly.";
    if (species == "dagon_spawn") return "The tide's children, grown unkind.";
    if (species == "ossified_brute") return "Bone that decided to keep moving.";
    return "Something that should not be, being.";
}

inline std::string prettySpecies(const std::string& species) {
    std::string out;
    bool cap = true;
    for (char ch : species) {
        if (ch == '_') {
            out += ' ';
            cap = true;
        } else {
            out += cap ? static_cast<char>(std::toupper((unsigned char)ch)) : ch;
            cap = false;
        }
    }
    return out;
}

} // namespace cultulhu
