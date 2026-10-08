#pragma once

// Wave 9c driver commands: `ambient` and `dungeon`.
//
// This module is header-inline on purpose: src/driver/ is excluded from
// the engine library's CMake source glob, so the implementation lives in
// main.cpp's translation unit via #include (one #include plus one
// dispatch line in the command chain). wave9_content_cmds.cpp is a stub
// TU that includes this header.
//
// registerWave9ContentCommands(g, cmd, in): dispatches the wave-9c
// commands. Returns true when the command was consumed (the caller then
// `continue`s), false for anything it does not own.

#include "ai/AmbientBehavior.h"
#include "assets/ModelCatalog.h"
#include "core/Events.h"
#include "world/Dungeon.h"
#include "world/ValeOfPnath.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace cultulhu {

// Parses the `ambient` sub-argument into an AmbientAction.
inline bool parseWave9AmbientAction(const std::string& s, AmbientAction& out) {
    if (s == "pray")           out = AmbientAction::Pray;
    else if (s == "patrol")    out = AmbientAction::Patrol;
    else if (s == "gather")    out = AmbientAction::Gather;
    else if (s == "preach")    out = AmbientAction::Preach;
    else if (s == "brawl")     out = AmbientAction::Brawl;
    else if (s == "desecrate") out = AmbientAction::Desecrate;
    else if (s == "omen")      out = AmbientAction::OmenReading;
    else if (s == "spar")       out = AmbientAction::Sparring;
    else if (s == "tend")       out = AmbientAction::TendWounded;
    else if (s == "graffiti")   out = AmbientAction::Graffiti;
    else if (s == "chant")      out = AmbientAction::ChantingCircle;
    else return false;
    return true;
}

inline bool registerWave9ContentCommands(BetaGame& g, const std::string& cmd,
                                         std::istringstream& in) {
    if (cmd == "ambient") {
        // ambient            -> one random eligible cultist does one
        //                       weighted-random ambient action
        // ambient <name>       -> force a specific action on a random
        //                       eligible cultist
        //   names: pray patrol gather preach brawl desecrate omen spar
        //          tend graffiti chant
        if (g.cult.size() == 0) {
            std::cout << "ambient: no cultists yet (recruit some first)\n";
            return true;
        }
        std::string which;
        in >> which;
        if (which.empty()) {
            if (g.ambient.forceRandom())
                std::cout << "ambient: an ambient event fired (see above)\n";
            else
                std::cout << "ambient: no eligible cultist right now\n";
            return true;
        }
        AmbientAction a;
        if (!parseWave9AmbientAction(which, a)) {
            std::cout << "ambient: unknown action '" << which
                      << "' (try: pray patrol gather preach brawl desecrate"
                      << " omen spar tend graffiti chant)\n";
            return true;
        }
        // Try random cultists until one is eligible.
        for (size_t t = 0; t < g.cult.size() * 2; ++t) {
            size_t idx = static_cast<size_t>(
                g.rng.intRange(0, static_cast<int>(g.cult.size()) - 1));
            if (g.ambient.forceAction(idx, a)) {
                std::cout << "ambient: cultist " << g.cult.at(idx).id()
                          << " -> " << ambientActionName(a) << "\n";
                return true;
            }
        }
        std::cout << "ambient: no eligible cultist right now\n";
        return true;
    }

    if (cmd == "dungeon") {
        // dungeon [seed]        -> generate a dungeon and print its layout
        //                          with hazards marked. Default seed 1337.
        // dungeon pnath [seed]  -> generate the Vale of Pnath (wave 13):
        //                          deeper, dread-scaled, dhole-haunted.
        std::string sub;
        in >> sub;
        if (sub == "pnath") {
            uint64_t seed = 4242;
            in >> seed; // no argument: stream read fails, seed stays 4242
            ValeOfPnath v(g.bus, 2, seed, Vec3{0, 0, 0});
            std::vector<char> view = v.tiles();
            for (const auto& h : v.hazards()) {
                const DungeonRoom& r =
                    v.rooms()[static_cast<size_t>(h.roomIndex)];
                char glyph = '?';
                switch (h.type) {
                    case HazardType::AbyssPit: glyph = 'O'; break;
                    case HazardType::Whispers: glyph = 'w'; break;
                    case HazardType::DholeTunnel:
                        glyph = h.primed ? 'D' : 'd';
                        break;
                    default: break;
                }
                view[static_cast<size_t>(r.centerY() * v.width() +
                                        r.centerX())] = glyph;
            }
            std::cout
                << "the Vale of Pnath (seed " << seed << "): " << v.width()
                << "x" << v.height() << ", " << v.rooms().size()
                << " rooms, max depth " << v.maxDepth() << ", "
                << v.hazards().size() << " hazards\n"
                << "entrance: surface fissure (or the deepest cave tier)\n"
                << "legend: # wall  . floor  E mouth  O abyss pit"
                << "  w maddening whispers  D/d dhole tunnel\n";
            for (int y = 0; y < v.height(); ++y) {
                for (int x = 0; x < v.width(); ++x)
                    std::cout
                        << view[static_cast<size_t>(y * v.width() + x)];
                std::cout << "\n";
            }
            std::cout << "rooms (index: depth, dread, hazard):\n";
            for (size_t i = 0; i < v.rooms().size(); ++i) {
                std::cout << "  room " << i << ": depth " << v.roomDepth(i)
                          << ", dread " << v.dreadAt(static_cast<int>(i));
                for (const auto& h : v.hazards())
                    if (h.roomIndex == static_cast<int>(i))
                        std::cout << ", " << hazardTypeName(h.type);
                if (static_cast<int>(i) == v.deepestRoomIndex())
                    std::cout << "  <-- RELIC VAULT (deepest)";
                std::cout << "\n";
            }
            std::cout << "relic vault at (" << v.valeRelicSpot().x << ", "
                      << v.valeRelicSpot().z << "); guardian spawns at ("
                      << v.guardianSpawnPos().x << ", "
                      << v.guardianSpawnPos().z << ")\n"
                      << "dhole model slot: '"
                      << ModelCatalog::creatureModel("dhole")
                      << "' (empty = procedural serpent/worm fallback)\n";
            return true;
        }
        // Not "pnath": `sub` was actually the seed (or empty).
        uint64_t seed = 1337;
        if (!sub.empty()) {
            try {
                seed = static_cast<uint64_t>(std::stoull(sub));
            } catch (...) {
                std::cout << "dungeon: unknown subcommand '" << sub
                          << "' (try: dungeon [seed] | dungeon pnath [seed])\n";
                return true;
            }
        }
        DungeonInstance d(g.bus, 1, seed, Vec3{0, 0, 0});

        // Overlay hazard glyphs on a copy of the tile grid.
        std::vector<char> view = d.tiles();
        for (const auto& h : d.hazards()) {
            const DungeonRoom& r = d.rooms()[static_cast<size_t>(h.roomIndex)];
            char glyph = '?';
            switch (h.type) {
                case HazardType::SpikePit: glyph = '^'; break;
                case HazardType::Trapped:  glyph = 'x'; break;
                case HazardType::CaveIn:
                    glyph = h.sealed ? 'X' : '~';
                    break;
                case HazardType::None:
                case HazardType::Count:    break;
            }
            view[static_cast<size_t>(r.centerY() * d.width() + r.centerX())] =
                glyph;
        }

        std::cout << "dungeon 1 (seed " << seed << "): " << d.width() << "x"
                  << d.height() << ", " << d.rooms().size() << " rooms, "
                  << d.hazards().size() << " hazards\n"
                  << "legend: # wall  . floor  E entrance  ^ spike pit"
                  << "  x hidden trap  ~ cave-in risk  X sealed\n";
        for (int y = 0; y < d.height(); ++y) {
            for (int x = 0; x < d.width(); ++x)
                std::cout << view[static_cast<size_t>(y * d.width() + x)];
            std::cout << "\n";
        }
        if (!d.hazards().empty()) {
            std::cout << "hazards:\n";
            for (const auto& h : d.hazards()) {
                const DungeonRoom& r =
                    d.rooms()[static_cast<size_t>(h.roomIndex)];
                std::cout << "  room " << h.roomIndex << " (" << r.centerX()
                          << "," << r.centerY() << "): "
                          << hazardTypeName(h.type);
                if (h.sealed) std::cout << " [SEALED]";
                std::cout << "\n";
            }
        }
        return true;
    }

    return false; // not a wave-9c command
}

} // namespace cultulhu
