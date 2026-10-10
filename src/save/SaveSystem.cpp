#include "save/SaveSystem.h"

#include <fstream>
#include <sstream>

// File format (line based, human readable):
//   clock=<seconds>
//   power=<value>
//   beliefs=<b0>,<b1>,<b2>        (belief enum ints; may be empty)
//   risk=<insurrection risk>
//   entity=<id> <type> <faction> <x> <y> <z> <hp> <maxHp>
//   achievement=<id>              (wave 16: unlocked achievement)
//   achprogress=<name> <value>    (wave 16: named progress counter)
//   discovery=<tab-separated fields, wave 26: codex record; text fields use
//             backslash escapes for tab/newline/backslash>

namespace cultulhu {

namespace {

// Wave 26: escape free-text save fields (names/flavors may hold anything).
std::string escField(const std::string& s) {
    std::string out;
    for (char ch : s) {
        if (ch == '\\') out += "\\\\";
        else if (ch == '\t') out += "\\t";
        else if (ch == '\n') out += "\\n";
        else out += ch;
    }
    return out;
}

std::string unescField(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            const char n = s[++i];
            if (n == 't') out += '\t';
            else if (n == 'n') out += '\n';
            else out += n; // '\\' and anything else: literal
        } else {
            out += s[i];
        }
    }
    return out;
}

std::vector<std::string> splitTab(const std::string& s) {
    std::vector<std::string> parts;
    std::string cur;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            cur += s[i];
            cur += s[++i]; // keep escapes intact; unescape per-field later
        } else if (s[i] == '\t') {
            parts.push_back(cur);
            cur.clear();
        } else {
            cur += s[i];
        }
    }
    parts.push_back(cur);
    return parts;
}

} // namespace

bool SaveSystem::save(const GameState& s, const std::string& path) {
    std::ofstream f(path);
    if (!f.is_open()) return false;

    f << "clock=" << s.clockTime << "\n";
    f << "power=" << s.power << "\n";
    f << "beliefs=";
    for (size_t i = 0; i < s.activeBeliefs.size(); ++i) {
        if (i) f << ",";
        f << static_cast<int>(s.activeBeliefs[i]);
    }
    f << "\n";
    f << "risk=" << s.insurrectionRisk << "\n";
    for (const auto& e : s.entities) {
        f << "entity=" << e.id << " " << e.type << " " << e.faction << " "
          << e.pos.x << " " << e.pos.y << " " << e.pos.z << " "
          << e.hp << " " << e.maxHp << "\n";
    }
    // Wave 16: achievements (unlocked ids + named progress counters).
    for (const auto& id : s.unlockedAchievements) f << "achievement=" << id << "\n";
    for (const auto& kv : s.achievementProgress)
        f << "achprogress=" << kv.first << " " << kv.second << "\n";
    // Wave 26: discovery codex records (wave 31: 7 fields; old 8-field
    // lines with a trailing renamed flag still load).
    for (const auto& r : s.discoveries) {
        f << "discovery=" << escField(r.id) << "\t" << r.kind << "\t"
          << escField(r.name) << "\t" << escField(r.flavor) << "\t"
          << r.pos.x << " " << r.pos.y << " " << r.pos.z << "\t"
          << r.gameTime << "\t" << (r.night ? 1 : 0) << "\n";
    }
    return static_cast<bool>(f);
}

bool SaveSystem::load(const std::string& path, GameState& out) {
    std::ifstream f(path);
    if (!f.is_open()) return false;

    GameState s;
    std::string line;
    // Wave 9d: a corrupt/hand-edited save must fail gracefully (return
    // false), never throw. std::stod/stof/stoi throw invalid_argument or
    // out_of_range on malformed fields, which would otherwise terminate
    // the driver via an uncaught exception.
    try {
        while (std::getline(f, line)) {
            if (line.rfind("clock=", 0) == 0) {
                s.clockTime = std::stod(line.substr(6));
            } else if (line.rfind("power=", 0) == 0) {
                s.power = std::stof(line.substr(6));
            } else if (line.rfind("beliefs=", 0) == 0) {
                std::string rest = line.substr(8);
                std::stringstream ss(rest);
                std::string tok;
                while (std::getline(ss, tok, ',')) {
                    if (tok.empty()) continue;
                    int b = std::stoi(tok);
                    if (b >= 0 && b < static_cast<int>(Belief::Count))
                        s.activeBeliefs.push_back(static_cast<Belief>(b));
                }
            } else if (line.rfind("risk=", 0) == 0) {
                s.insurrectionRisk = std::stof(line.substr(5));
            } else if (line.rfind("entity=", 0) == 0) {
                std::stringstream ss(line.substr(7));
                GameState::EntityRec e;
                if (ss >> e.id >> e.type >> e.faction >> e.pos.x >> e.pos.y
                       >> e.pos.z >> e.hp >> e.maxHp) {
                    s.entities.push_back(e);
                } else {
                    return false; // malformed entity line
                }
            } else if (line.rfind("achievement=", 0) == 0) {
                // Wave 16: unlocked achievement id (skip empties).
                std::string id = line.substr(12);
                if (!id.empty()) s.unlockedAchievements.push_back(id);
            } else if (line.rfind("achprogress=", 0) == 0) {
                // Wave 16: named progress counter; malformed lines are
                // skipped rather than failing the whole load.
                std::stringstream ss(line.substr(12));
                std::string name;
                double value = 0.0;
                if (ss >> name >> value && !name.empty())
                    s.achievementProgress[name] = value;
            } else if (line.rfind("discovery=", 0) == 0) {
                // Wave 26: codex record; malformed lines are skipped.
                // Wave 31: 7 fields; pre-wave-31 saves had 8 (trailing
                // renamed flag) and still load — the extra field is ignored.
                const std::vector<std::string> p =
                    splitTab(line.substr(10));
                if (p.size() == 7 || p.size() == 8) {
                    GameState::DiscoveryRec r;
                    r.id = unescField(p[0]);
                    r.name = unescField(p[2]);
                    r.flavor = unescField(p[3]);
                    std::stringstream ps(p[4]);
                    float x = 0, y = 0, z = 0;
                    int kind = 0, night = 0;
                    double t = 0;
                    if (ps >> x >> y >> z &&
                        (std::stringstream(p[1]) >> kind) &&
                        (std::stringstream(p[5]) >> t) &&
                        (std::stringstream(p[6]) >> night) &&
                        !r.id.empty()) {
                        r.kind = kind;
                        r.pos = Vec3{x, y, z};
                        r.gameTime = t;
                        r.night = night != 0;
                        s.discoveries.push_back(r);
                    }
                }
            }
            // Unknown lines are ignored for forward compatibility.
        }
    } catch (...) {
        return false; // corrupt save file: fail the load, don't crash
    }
    out = s;
    return true;
}

} // namespace cultulhu
