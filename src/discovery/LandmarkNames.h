#pragma once

// CULT-ULHU wave 31: R'lyehian landmark-name generator. The game invents
// cool eldritch names for the places it seeds — the player discovers them,
// never names them. Deterministic per RNG draw.

#include "core/RNG.h"

#include <cctype>
#include <string>
#include <vector>

namespace cultulhu {

inline std::string generateLandmarkName(RNG& rng) {
    // Guttural openers, writhing cores, abyssal endings — Lovecraft's
    // phonetics without copying his proper nouns.
    static const std::vector<std::string> open = {
        "R", "Y", "N", "Sh", "Ts", "X", "K", "Z", "Ng", "Th",
        "Vh", "M", "Yh", "Gha", "Kha", "Ny", "Shu", "Yo", "Yu",
    };
    static const std::vector<std::string> core = {
        "lyeh", "lath", "iggur", "oth", "oggua", "ggoth", "dath",
        "rnath", "lthar", "nthlei", "xath", "orath", "ulhu", "thot",
        "thlei", "een", "ath", "uul", "eph", "ozoa",
    };
    static const std::vector<std::string> tail = {
        "hotep", "urath", "oggua", "thlei", "oth", "een", "ul",
    };
    static const std::vector<std::string> epithet = {
        "Shattered", "Drowned", "Weeping", "Sunken", "Silent",
        "Pale", "Dreaming", "Forgotten", "Hollow", "Black",
    };

    auto pick = [&](const std::vector<std::string>& v) -> std::string {
        return v[static_cast<size_t>(
            rng.intRange(0, static_cast<int>(v.size()) - 1))];
    };
    auto cap = [](std::string s) -> std::string {
        if (!s.empty())
            s[0] = static_cast<char>(
                std::toupper(static_cast<unsigned char>(s[0])));
        return s;
    };

    const int pattern = rng.intRange(0, 99);
    std::string name;
    if (pattern < 35) {
        // R'lyeh form: guttural + apostrophe + writhing core.
        name = pick(open) + "'" + pick(core);
        name = cap(name);
    } else if (pattern < 60) {
        // Hyphenated elder form: Shub-Niggurath cadence.
        name = cap(pick(open) + pick(core)) + "-" +
               cap(pick(open) + pick(core));
    } else if (pattern < 85) {
        // Single brooding mass: Kadath, Sarnath, Ulthar cadence.
        name = cap(pick(open) + pick(core));
        if (rng.intRange(0, 1)) name += pick(tail);
    } else {
        // Epithet + abyssal name: the Drowned Xulhu.
        name = pick(epithet) + " " + cap(pick(open) + pick(core));
    }
    return name;
}

} // namespace cultulhu
