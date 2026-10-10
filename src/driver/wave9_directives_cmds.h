#pragma once

// Wave 9b driver commands: `directive assassinate|blight|summon|zones`
// plus wave 15: `directive dream|rebuild`.
//
// This module is header-inline on purpose: src/driver/ is excluded from
// the engine library's CMake source glob, so the implementation lives in
// main.cpp's translation unit via #include (one #include plus one
// dispatch line in the command chain). wave9_directives_cmds.cpp is a stub
// TU that includes this header.
//
// registerWave9DirectiveCommands(g, cmd, in): dispatches the wave-9b
// directive commands. Returns true when the command was consumed (the
// caller then `return true`s), false for anything it does not own.
//
// Commands:
//   directive assassinate [entity-id]  kill an enemy leader: the named
//                                      entity, the nearest hostile entity,
//                                      or a freshly arrived rival prophet
//                                      when the world has no hostiles.
//   directive blight                   corrupt the zone at the avatar's
//                                      position over ~120 ticks.
//   directive summon                   90s ritual: 300 power is consumed
//                                      immediately; a dread champion
//                                      answers on completion.
//   directive zones                    list the session's zones and their
//                                      Blighted state.
//   directive dream                    (wave 15) 60s mass dream-rite:
//                                      the cult's dreamers channel visions
//                                      into power.
//   directive rebuild                  (wave 15) 60s rebuilding directive:
//                                      the sanctum rises again, and the
//                                      cult's devotion steadies.
//
// The first call wires the session: executor context (power, exertion, and
// a small world map with demo zones for blight targeting), a
// ChampionSummoned subscription that spawns the dread champion Monstrosity
// into the world, and a LeaderAssassinated subscription that kills the
// target entity in the world.

#include "commands/CommandSystem.h"
#include "commands/DirectiveExecutor.h"
#include "core/EventBus.h"
#include "core/Events.h"
#include "core/Vec3.h"
#include "entities/Entity.h"
#include "entities/Units.h"
#include "world/WorldMap.h"
#include "world/Zone.h"

#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace cultulhu {
namespace wave9b {

// Session state: a small world map with demo zones so the blight directive
// always resolves to a named zone.
struct DirectiveSession {
    bool wired = false;
    WorldMap map{"driver_surface"};
};

inline DirectiveSession& directiveSession() {
    static DirectiveSession s;
    return s;
}

inline void addDirectiveDemoZones(WorldMap& map) {
    ZoneDef fields;
    fields.name = "Dunwich Fields";
    fields.min = Vec3{-500, -50, -500};
    fields.max = Vec3{500, 50, 500};
    fields.ambient.ambientFear = 10.0f;
    map.addZone(std::move(fields));

    ZoneDef docks;
    docks.name = "Innsmouth Docks";
    docks.min = Vec3{600, -50, -500};
    docks.max = Vec3{900, 50, 500};
    docks.ambient.ambientFear = 20.0f;
    map.addZone(std::move(docks));
}

// Wire the executor context and the two world-mutating subscriptions once.
inline void ensureDirectiveWired(BetaGame& g) {
    DirectiveSession& s = directiveSession();
    if (s.wired) return;
    s.wired = true;
    addDirectiveDemoZones(s.map);

    DirectiveContext ctx;
    ctx.power = &g.power;
    ctx.exertion = &g.exertion;
    ctx.worldMap = &s.map;
    g.executor.setContext(ctx);

    // The champion answers: spawn the dread champion Monstrosity into the
    // world at the summoning site (boosted stats per the event payload).
    g.bus.subscribe(EventType::ChampionSummoned, [&g](const GameEvent& e) {
        const float hp = e.amount > 0.0f ? e.amount : 1200.0f;
        auto champ = std::make_unique<Monstrosity>(FACTION_CTHULHU, e.pos,
                                                  "dread champion", false, hp);
        const uint64_t id = champ->id();
        g.world.push_back(std::move(champ));
        std::cout << "[summon] a dread champion rises (entity " << id
                  << ", " << hp << " HP)\n";
    });
    // The blade found its mark: the leader dies in the world too.
    g.bus.subscribe(EventType::LeaderAssassinated, [&g](const GameEvent& e) {
        if (e.sourceId == 0) return;
        for (auto& ent : g.world) {
            if (ent->id() == e.sourceId && ent->alive()) {
                ent->takeDamage(ent->hp() + 1.0f);
                std::cout << "[assassinate] the enemy leader (entity "
                          << e.sourceId << ") is slain\n";
                return;
            }
        }
    });
}

inline Entity* findWorldEntity(BetaGame& g, uint64_t id) {
    for (auto& e : g.world)
        if (e->id() == id) return e.get();
    return nullptr;
}

inline Entity* nearestHostile(BetaGame& g, Vec3 from) {
    Entity* best = nullptr;
    float bestD = std::numeric_limits<float>::max();
    for (auto& e : g.world) {
        if (!e->alive()) continue;
        if (e->faction() == FACTION_CTHULHU || e->faction() == FACTION_NEUTRAL)
            continue;
        const float d = e->position().distance(from);
        if (d < bestD) {
            bestD = d;
            best = e.get();
        }
    }
    return best;
}

inline bool runAssassinate(BetaGame& g, std::istringstream& in) {
    uint64_t id = 0;
    in >> id;
    Entity* target = (id != 0) ? findWorldEntity(g, id) : nullptr;
    if (id != 0 && !target) {
        std::cout << "no entity " << id << "\n";
        return true;
    }
    if (!target) target = nearestHostile(g, g.avatar.position());
    if (!target) {
        // No enemy leader in the world: a rival prophet comes to parley.
        Vec3 p = g.avatar.position();
        p.x += 40.0f;
        auto prophet = std::make_unique<Sorcerer>(1, p);
        target = prophet.get();
        std::cout << "(a rival prophet (entity " << target->id()
                  << ") approaches from the east)\n";
        g.world.push_back(std::move(prophet));
    }
    DirectiveContext ctx = g.executor.context();
    ctx.targetEntityId = target->id();
    g.executor.setContext(ctx);
    CommandResult r = g.commands.issueCommand(
        DirectiveType::AssassinateProphet, target->position(),
        target->faction());
    std::cout << commandOutcomeName(r.outcome) << " — " << r.detail << "\n";
    return true;
}

inline bool runBlight(BetaGame& g) {
    CommandResult r =
        g.commands.issueCommand(DirectiveType::BlightLand, g.avatar.position());
    std::cout << commandOutcomeName(r.outcome) << " — " << r.detail << "\n";
    return true;
}

inline bool runSummon(BetaGame& g) {
    if (g.power.value() < 300.0f)
        std::cout << "(warning: the rite needs 300 power; the cult has "
                  << g.power.value() << " — it will fail)\n";
    CommandResult r = g.commands.issueCommand(DirectiveType::GrandSummoning,
                                              g.avatar.position());
    std::cout << commandOutcomeName(r.outcome) << " — " << r.detail << "\n";
    return true;
}

inline bool runZones() {
    DirectiveSession& s = directiveSession();
    std::cout << "zones (" << s.map.zoneCount() << "):\n";
    for (size_t i = 0; i < s.map.zoneCount(); ++i) {
        const Zone& z = s.map.zone(i);
        std::cout << "  " << z.name()
                  << (z.blighted() ? "  [BLIGHTED]" : "") << "\n";
    }
    return true;
}

// Wave 15: `directive dream` — 60s mass dream-rite.
inline bool runDreamHarvest(BetaGame& g) {
    CommandResult r = g.commands.issueCommand(DirectiveType::OneiricHarvest,
                                              g.avatar.position());
    std::cout << commandOutcomeName(r.outcome) << " — " << r.detail << "\n";
    return true;
}

// Wave 15: `directive rebuild` — 60s sanctum rebuilding.
inline bool runRebuildSanctum(BetaGame& g) {
    CommandResult r = g.commands.issueCommand(DirectiveType::RebuildSanctum,
                                              g.avatar.position());
    std::cout << commandOutcomeName(r.outcome) << " — " << r.detail << "\n";
    return true;
}

} // namespace wave9b

inline bool registerWave9DirectiveCommands(BetaGame& g, const std::string& cmd,
                                           std::istringstream& in) {
    if (cmd != "directive") return false;
    wave9b::ensureDirectiveWired(g);
    std::string sub;
    in >> sub;
    if (sub == "assassinate") return wave9b::runAssassinate(g, in);
    if (sub == "blight")      return wave9b::runBlight(g);
    if (sub == "summon")      return wave9b::runSummon(g);
    if (sub == "zones")       return wave9b::runZones();
    if (sub == "dream")       return wave9b::runDreamHarvest(g);
    if (sub == "rebuild")     return wave9b::runRebuildSanctum(g);
    std::cout << "usage: directive "
                 "<assassinate [entity-id]|blight|summon|zones|dream|rebuild>\n";
    return true;
}

} // namespace cultulhu
