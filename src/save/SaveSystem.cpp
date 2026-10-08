#include "save/SaveSystem.h"

#include <fstream>
#include <sstream>

// File format (line based, human readable):
//   clock=<seconds>
//   power=<value>
//   beliefs=<b0>,<b1>,<b2>        (belief enum ints; may be empty)
//   risk=<insurrection risk>
//   entity=<id> <type> <faction> <x> <y> <z> <hp> <maxHp>

namespace cultulhu {

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
