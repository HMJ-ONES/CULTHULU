#include "characters/AbilityFx.h"

#include <fstream>
#include <sstream>

namespace cultulhu {

namespace {

std::string trim(std::string s) {
    const char* ws = " \t\r\n";
    s.erase(0, s.find_first_not_of(ws));
    if (!s.empty()) s.erase(s.find_last_not_of(ws) + 1);
    return s;
}

bool parseBool(const std::string& s, bool& out) {
    if (s == "true" || s == "yes" || s == "1") { out = true; return true; }
    if (s == "false" || s == "no" || s == "0") { out = false; return true; }
    return false;
}

bool parseFloat(const std::string& s, float& out) {
    try {
        size_t n = 0;
        out = std::stof(s, &n);
        return n == s.size();
    } catch (...) {
        return false;
    }
}

bool parseInt(const std::string& s, int& out) {
    try {
        size_t n = 0;
        out = std::stoi(s, &n);
        return n == s.size();
    } catch (...) {
        return false;
    }
}

// "a,b" -> two floats.
bool parsePair(const std::string& s, float& a, float& b) {
    const size_t comma = s.find(',');
    if (comma == std::string::npos) return false;
    return parseFloat(trim(s.substr(0, comma)), a) &&
           parseFloat(trim(s.substr(comma + 1)), b);
}

bool parseEmitter(const std::string& s, FxEmitter& out) {
    if (s == "point") { out = FxEmitter::Point; return true; }
    if (s == "cone") { out = FxEmitter::Cone; return true; }
    if (s == "ring") { out = FxEmitter::Ring; return true; }
    if (s == "sphere") { out = FxEmitter::Sphere; return true; }
    if (s == "beam") { out = FxEmitter::Beam; return true; }
    if (s == "wall") { out = FxEmitter::Wall; return true; }
    return false;
}

bool isHexColor(const std::string& s) {
    if (s.size() != 6) return false;
    for (char c : s) {
        const bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                         (c >= 'A' && c <= 'F');
        if (!hex) return false;
    }
    return true;
}

} // namespace

const FxPreset* FxLibrary::find(const std::string& id) const {
    for (const auto& p : presets)
        if (p.id == id) return &p;
    return nullptr;
}

FxLoadResult parseFxLibraryText(const std::string& text) {
    FxLoadResult r;
    FxPreset cur;
    bool inPreset = false;
    std::istringstream in(text);
    std::string line;
    int lineNo = 0;

    auto finish = [&]() -> bool {
        if (!inPreset) return true;
        if (cur.id.empty()) {
            r.error = "preset with empty id";
            return false;
        }
        if (r.library.find(cur.id) != nullptr) {
            r.error = "duplicate preset '" + cur.id + "'";
            return false;
        }
        // Sanity: ranges ordered, colors valid.
        if (cur.lifetimeMin > cur.lifetimeMax || cur.speedMin > cur.speedMax ||
            cur.sizeMin > cur.sizeMax) {
            r.error = "preset '" + cur.id + "': range min > max";
            return false;
        }
        if (!isHexColor(cur.color0) || !isHexColor(cur.color1)) {
            r.error = "preset '" + cur.id + "': bad hex color";
            return false;
        }
        if (cur.count <= 0) {
            r.error = "preset '" + cur.id + "': count must be positive";
            return false;
        }
        r.library.presets.push_back(cur);
        return true;
    };

    while (std::getline(in, line)) {
        ++lineNo;
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            if (!finish()) return r;
            cur = FxPreset{};
            cur.id = trim(line.substr(1, line.size() - 2));
            inPreset = true;
            continue;
        }
        if (!inPreset) {
            r.error = "line " + std::to_string(lineNo) +
                      ": key before first [preset] section";
            return r;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos) {
            r.error = "line " + std::to_string(lineNo) + ": no '='";
            return r;
        }
        const std::string key = trim(line.substr(0, eq));
        const std::string val = trim(line.substr(eq + 1));

        float f = 0.0f, g = 0.0f;
        int n = 0;
        bool b = false;
        FxEmitter e = FxEmitter::Point;

        if (key == "display_name") {
            cur.displayName = val;
        } else if (key == "emitter" && parseEmitter(val, e)) {
            cur.emitter = e;
        } else if (key == "emitter") {
            r.error = "line " + std::to_string(lineNo) +
                      ": unknown emitter '" + val + "'";
            return r;
        } else if (key == "count" && parseInt(val, n)) {
            cur.count = n;
        } else if (key == "lifetime" && parsePair(val, f, g)) {
            cur.lifetimeMin = f;
            cur.lifetimeMax = g;
        } else if (key == "speed" && parsePair(val, f, g)) {
            cur.speedMin = f;
            cur.speedMax = g;
        } else if (key == "spread" && parseFloat(val, f)) {
            cur.spreadDeg = f;
        } else if (key == "size" && parsePair(val, f, g)) {
            cur.sizeMin = f;
            cur.sizeMax = g;
        } else if (key == "colors") {
            const size_t comma = val.find(',');
            if (comma == std::string::npos) {
                r.error = "line " + std::to_string(lineNo) +
                          ": colors needs 'hex0,hex1'";
                return r;
            }
            cur.color0 = trim(val.substr(0, comma));
            cur.color1 = trim(val.substr(comma + 1));
        } else if (key == "additive" && parseBool(val, b)) {
            cur.additive = b;
        } else if (key == "gravity" && parseFloat(val, f)) {
            cur.gravity = f;
        } else if (key == "texture") {
            cur.texture = val;
        } else if (key == "duration" && parseFloat(val, f)) {
            cur.durationSec = f;
        } else if (key == "loop" && parseBool(val, b)) {
            cur.loop = b;
        } else {
            r.error = "line " + std::to_string(lineNo) + ": bad key '" +
                      key + "' or value '" + val + "'";
            return r;
        }
    }
    if (!finish()) return r;
    r.ok = true;
    return r;
}

FxLoadResult loadFxLibrary(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        FxLoadResult r;
        r.error = "cannot open fx library '" + path + "'";
        return r;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return parseFxLibraryText(ss.str());
}

const FxPreset* fxForSpell(const SpellDef& spell, const FxLibrary& lib) {
    if (!spell.fxPreset.empty()) return lib.find(spell.fxPreset);
    return lib.find(spell.effectKind);
}

const FxPreset* fxForEvent(EventType type, const FxLibrary& lib) {
    switch (type) {
        case EventType::CityBuildingDestroyed:
        case EventType::DistrictRazed:
        case EventType::CityDestroyed:
            return lib.find("raze");
        default:
            return nullptr;
    }
}

} // namespace cultulhu
