// CULT-ULHU playable beta driver: an interactive console REPL so the game is
// playable headless right now. The engine binding (Unreal/Unity) will replace
// this with real rendering later; all game logic lives in the core library.

#include "ai/AmbientBehavior.h"
#include "ai/RitualCaster.h"
#include "animation/AnimationStateMachine.h"
#include "assets/AssetManager.h"
#include "beliefs/BeliefSystem.h"
#include "camera/CameraSystem.h"
#include "chaos/LunaticSystem.h"
#include "combat/Attacks.h"
#include "combat/CrowdControl.h"
#include "commands/CommandSystem.h"
#include "commands/DirectiveExecutor.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "cult/CultManager.h"
#include "dreams/DreamSystem.h"
#include "entities/Structures.h"
#include "entities/Units.h"
#include "exertion/ExertionSystem.h"
#include "modes/FreeRoamMode.h"
#include "net/Discovery.h"
#include "net/Lobby.h"
#include "net/Netcode.h"
#include "net/RadminNet.h"
#include "net/Socket.h"
#include "power/PowerSystem.h"
#include "save/SaveSystem.h"

#include <cmath>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace cultulhu;

namespace {

float dist(Vec3 a, Vec3 b) {
    const float dx = a.x - b.x, dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

Belief beliefByName(const std::string& n) {
    for (int i = 0; i < static_cast<int>(Belief::Count); ++i) {
        Belief b = static_cast<Belief>(i);
        std::string name = beliefName(b);
        for (auto& ch : name) ch = static_cast<char>(std::tolower(ch));
        std::string q = n;
        for (auto& ch : q) ch = static_cast<char>(std::tolower(ch));
        if (name == q) return b;
    }
    return Belief::Count;
}

// Wave 5b: entity type ids in a save file are trusted only after this check.
bool validEntityType(int t) {
    return t >= static_cast<int>(EntityType::GreatOldOne) &&
           t <= static_cast<int>(EntityType::Artifact);
}

// Wave 5b: rebuild a world entity from a save record. The avatar and
// cultists are restored through BetaGame's own members instead.
std::unique_ptr<Entity> entityFromRecord(const GameState::EntityRec& r) {
    if (!validEntityType(r.type)) return nullptr;
    const EntityType t = static_cast<EntityType>(r.type);
    const float maxHp = r.maxHp > 0.0f ? r.maxHp : 100.0f;
    std::unique_ptr<Entity> e;
    switch (t) {
        case EntityType::GreatOldOne:
            e = std::make_unique<GreatOldOne>(r.faction, r.pos, maxHp);
            break;
        case EntityType::Cultist:
            e = std::make_unique<Cultist>(r.faction, r.pos, maxHp);
            break;
        case EntityType::Civilian:
            e = std::make_unique<Civilian>(r.pos, maxHp);
            break;
        case EntityType::Adventurer:
            e = std::make_unique<Adventurer>(r.pos, maxHp);
            break;
        case EntityType::Creature:
            e = std::make_unique<Creature>(r.faction, r.pos, "restored",
                                           maxHp);
            break;
        case EntityType::Monstrosity:
            e = std::make_unique<Monstrosity>(r.faction, r.pos, "restored",
                                              false, maxHp);
            break;
        case EntityType::Mimic:
            e = std::make_unique<Mimic>(r.faction, r.pos, maxHp);
            break;
        case EntityType::Sorcerer:
            e = std::make_unique<Sorcerer>(r.faction, r.pos, maxHp);
            break;
        case EntityType::Building:
            e = std::make_unique<Building>(r.faction, r.pos, maxHp);
            break;
        case EntityType::Relic:
            e = std::make_unique<Relic>(r.pos, 0.0f);
            break;
        case EntityType::Artifact:
            e = std::make_unique<Artifact>(r.pos, false);
            break;
        case EntityType::EldritchAvatar:
            return nullptr; // restored through BetaGame::avatar
    }
    if (e) e->revive(r.hp); // restores hp; 0 hp stays dead
    return e;
}

} // namespace

struct BetaGame {
    EventBus bus;
    GameClock clock;
    RNG rng{1234};
    PowerSystem power;
    BeliefSystem beliefs{bus, clock};
    CultManager cult{bus, clock, rng};
    DreamSystem dreams{bus, rng, beliefs, cult};
    LunaticSystem lunatics{bus, rng, beliefs, cult};
    FreeRoamMode freeroam{bus, clock, rng};
    CameraSystem camera;
    EldritchAvatar avatar{FACTION_CTHULHU, Vec3{0, 0, 0}, power};
    CommandSystem commands{bus, rng, beliefs, cult};
    // Wave 5b: live follow-through for obeyed directives. Spawns operations
    // off DirectiveResolved and ticks them in tickSecond().
    DirectiveExecutor executor{bus, rng, cult};
    AmbientDirector ambient{bus, rng, beliefs, cult, 30.0};
    // Wave 4: belief exertion owns the unified power pipeline (see
    // ExertionSystem). Must come after bus/beliefs/power/cult/rng.
    ExertionSystem exertion{bus, beliefs, power, cult, rng};
    RitualCaster rituals{bus, rng, beliefs, exertion, 45.0};
    ActiveEffects fx;
    AssetManager assets;
    AnimationStateMachine avatarAnim;

    // Manually spawned world entities (sorcerers, monstrosities, ...).
    std::vector<std::unique_ptr<Entity>> world;

    BetaGame() {
        // The exertion system now owns the power pipeline (event -> belief
        // power rules -> synergy multipliers -> PowerSystem). Narration for
        // the notable moments.
        dreams.setExertion(&exertion);
        bus.subscribe(EventType::Revolt, [](const GameEvent&) {
            std::cout << "\n!! THE CULT REVOLTS !!\n";
        });
        bus.subscribe(EventType::Nightmare, [](const GameEvent& e) {
            std::cout << "[nightmare] cultist " << e.sourceId
                      << " woke up Lunatic\n";
        });
        bus.subscribe(EventType::DirectiveResolved, [](const GameEvent& e) {
            std::cout << "[directive] " << e.tag
                      << " (obedience was " << e.amount << ")\n";
        });
        bus.subscribe(EventType::DirectiveProgress, [](const GameEvent& e) {
            std::cout << "[directive] " << e.tag << " "
                      << static_cast<int>(e.amount * 100.0f + 0.5f)
                      << "%\n";
        });
        bus.subscribe(EventType::DirectiveCompleted, [](const GameEvent& e) {
            std::cout << "[directive] " << e.tag << " completed\n";
        });
        bus.subscribe(EventType::DreamWhisper, [](const GameEvent& e) {
            std::cout << "[dream] a distant civilian stirs in their sleep "
                         "(cultist " << e.sourceId << " dreaming)\n";
        });
        bus.subscribe(EventType::DistrictRazed, [](const GameEvent& e) {
            std::cout << "[city] a district lies in ruins\n";
            (void)e;
        });
    }

    void setupWorld() {
        freeroam.setMapBounds(Vec3{-500, 0, -500}, Vec3{500, 0, 500});
        freeroam.addCivilianSpawn(Vec3{60, 0, 40});
        freeroam.addCivilianSpawn(Vec3{-80, 0, 120});
        freeroam.addCivilianSpawn(Vec3{150, 0, -60});
        freeroam.addCreatureSpawn(Vec3{-120, 0, -90}, "ghoul");
        freeroam.addCreatureSpawn(Vec3{200, 0, 80}, "deep one");
        freeroam.addRelicSpawn(Vec3{90, 0, -40}, 0.25f);
        freeroam.addRelicSpawn(Vec3{-150, 0, 60}, 0.15f);

        // Beta starts with Dreams and Conversion already adopted.
        beliefs.requestChange(Belief::Dreams, Belief::Count);
        beliefs.requestChange(Belief::Conversion, Belief::Count);
        beliefs.update(BeliefSystem::ADOPTION_TIME + 1.0);

        for (int i = 0; i < 3; ++i) {
            Cultist& c = cult.recruit();
            c.setPosition(Vec3{static_cast<float>(i * 3), 0, 5});
        }
        auto sorc = std::make_unique<Sorcerer>(FACTION_CTHULHU,
                                               Vec3{-4, 0, 3});
        world.push_back(std::move(sorc));
    }

    // One game-second of simulation.
    void tickSecond() {
        clock.advance(1.0);
        const bool night = freeroam.isNight();
        beliefs.setNight(night);
        dreams.setNight(night);
        beliefs.update(1.0);
        power.add(beliefs.tick(1.0));
        power.add(dreams.update(1.0));
        lunatics.update(1.0);
        ambient.setHourOfDay(freeroam.hourOfDay());
        ambient.update(1.0);
        // Wave 4: exertion decay, derived stats, loyalty drift, tensions.
        exertion.update(1.0);
        // Wave 4: ambient sorcerer conversion rituals.
        std::vector<Sorcerer*> sorcs;
        std::vector<Civilian*> civs;
        for (const auto& e : world) {
            if (auto* s = dynamic_cast<Sorcerer*>(e.get()))
                sorcs.push_back(s);
            if (auto* c = dynamic_cast<Civilian*>(e.get()))
                civs.push_back(c);
        }
        for (const auto& c : freeroam.civilians()) civs.push_back(c.get());
        rituals.setSorcerers(sorcs);
        rituals.setCivilians(civs);
        rituals.update(1.0);
        // Wave 5b: directive follow-through operations tick here; their
        // events feed the exertion/power pipeline like any other events.
        executor.update(1.0);
        freeroam.update(1.0);
        fx.tick(1.0);
        avatarAnim.update(1.0);
        if (cult.update(1.0)) {
            // revolt handled by narration subscription
        }
        cult.dismissDead();
    }

    // Nearest attackable neutral entity (freeroam civilians/creatures first,
    // then manually spawned world entities).
    Entity* nearestTarget(Vec3 from, float maxDist = 60.0f) {
        Entity* best = nullptr;
        float bestD = maxDist;
        for (const auto& c : freeroam.civilians()) {
            if (!c->alive()) continue;
            const float d = dist(from, c->position());
            if (d < bestD) { bestD = d; best = c.get(); }
        }
        for (const auto& c : freeroam.creatures()) {
            if (!c->alive() || c->faction() == FACTION_CTHULHU) continue;
            const float d = dist(from, c->position());
            if (d < bestD) { bestD = d; best = c.get(); }
        }
        for (const auto& e : world) {
            if (!e->alive() || e->faction() == FACTION_CTHULHU) continue;
            const float d = dist(from, e->position());
            if (d < bestD) { bestD = d; best = e.get(); }
        }
        return best;
    }

    void printStatus() {
        CameraPose pose = camera.poseFor(avatar.position(),
                                         avatar.facingYaw());
        std::cout << "--- status ---\n";
        std::cout << "power: " << power.value() << " / "
                  << PowerSystem::MAX_POWER << "\n";
        std::cout << "beliefs:";
        for (Belief b : beliefs.active()) std::cout << " " << beliefName(b);
        std::cout << "\nrisk: " << cult.insurrectionRisk()
                  << "  fear: " << beliefs.fearLevel() << "\n";
        std::cout << "exertion:";
        for (int i = 0; i < static_cast<int>(Belief::Count); ++i) {
            Belief b = static_cast<Belief>(i);
            std::cout << " " << beliefName(b) << "="
                      << static_cast<int>(exertion.exertion(b));
        }
        std::cout << "\n  combatPower x" << exertion.stats().combatPowerMult
                  << "  loyaltyDrift " << exertion.stats().loyaltyDriftPerSec
                  << "/s  convertChance "
                  << exertion.stats().conversionChance << "\n";
        std::cout << "cultists: " << cult.size()
                  << "  resting: " << dreams.restingCount()
                  << "  ambient acts: " << ambient.actionsPerformed() << "\n";
        std::cout << "directive ops: " << executor.activeCount() << " active"
                  << (executor.defenseActive() ? " (DEFENDING)" : "") << "\n";
        for (size_t i = 0; i < cult.size(); ++i) {
            const Cultist& c = cult.at(i);
            std::cout << "  [" << i << "] id=" << c.id()
                      << " hp=" << c.hp()
                      << " state=" << static_cast<int>(c.state())
                      << " dev=" << c.devotion()
                      << (dreams.isResting(c.id()) ? " RESTING" : "")
                      << "\n";
        }
        std::cout << "avatar: (" << avatar.position().x << ", "
                  << avatar.position().z << ") hp=" << avatar.hp() << "\n";
        std::cout << "camera: " << cameraModeName(camera.mode())
                  << "  anim: "
                  << animationStateName(avatarAnim.currentState())
                  << " (" << avatarAnim.currentClipName() << ")\n";
        std::cout << "time: " << freeroam.hourOfDay() << "h"
                  << (freeroam.isNight() ? " (night)" : " (day)") << "\n";
        std::cout << "world: " << freeroam.civilians().size()
                  << " civilians, " << freeroam.creatures().size()
                  << " creatures, " << relicsInReach() << " relics near\n";
        (void)pose;
    }

    size_t relicsInReach() const {
        size_t n = 0;
        for (const auto& r : freeroam.relics())
            if (dist(avatar.position(), r->position()) < 120.0f) ++n;
        return n;
    }

    // Wave 5b: snapshot the beta game into a SaveSystem GameState.
    GameState buildSaveState() {
        GameState s;
        s.clockTime = clock.now();
        s.power = power.value();
        s.activeBeliefs = beliefs.active();
        s.insurrectionRisk = cult.insurrectionRisk();
        auto rec = [](const Entity& e) {
            GameState::EntityRec r;
            r.id = e.id();
            r.type = static_cast<int>(e.type());
            r.faction = e.faction();
            r.pos = e.position();
            r.hp = e.hp();
            r.maxHp = e.maxHp();
            return r;
        };
        s.entities.push_back(rec(avatar));
        for (size_t i = 0; i < cult.size(); ++i)
            s.entities.push_back(rec(cult.at(i)));
        for (const auto& e : world) s.entities.push_back(rec(*e));
        return s;
    }

    // Wave 5b: restore a snapshot. Beta limitations: entity AI state is NOT
    // restored — rest/anim state, cultist devotion, sorcerer mana, relic
    // amplifiers, monstrosity species/feral flags, mimic disguise, building
    // rebuild progress, and ambient/ritual timers all reset to defaults;
    // respawned entities get fresh ids.
    void applySaveState(const GameState& s) {
        clock.reset();
        clock.advance(s.clockTime);
        power.set(s.power);
        beliefs.restoreActive(s.activeBeliefs);
        cult.addRisk(s.insurrectionRisk - cult.insurrectionRisk());
        cult.clear();
        world.clear();
        bool avatarSeen = false;
        for (const auto& r : s.entities) {
            if (!validEntityType(r.type)) continue;
            const EntityType t = static_cast<EntityType>(r.type);
            if (t == EntityType::EldritchAvatar && !avatarSeen) {
                avatarSeen = true;
                avatar.setPosition(r.pos);
                avatar.revive(r.hp);
                continue;
            }
            if (t == EntityType::Cultist) {
                Cultist& c = cult.recruit();
                c.setPosition(r.pos);
                c.revive(r.hp);
                continue;
            }
            auto e = entityFromRecord(r);
            if (e) world.push_back(std::move(e));
        }
    }

    void printHelp() {
        std::cout <<
            "commands:\n"
            "  move <n|s|e|w|ne|nw|se|sw> [steps]  walk the avatar\n"
            "  camera <fp|tp>                      switch camera\n"
            "  look                                survey surroundings\n"
            "  spawn <cultist|civilian|monstrosity|sorcerer> [n]\n"
            "  belief <name> [replace <old>]       adopt a belief\n"
            "  beliefs                             list active beliefs\n"
            "  rest <i>                            toggle rest for cultist i\n"
            "  command <raid|war|convert|sacrifice|defend|relic>\n"
            "  attack                              melee the nearest target\n"
            "  cast <fireball|fear>                sorcerer spell + CC\n"
            "  tick <n>                            advance n game-seconds\n"
            "  save <file>                         save game to file\n"
            "  load <file>                         load game from file\n"
            "  myip                                show adapters (Radmin IP)\n"
            "  discover [secs]                     find hosts on Radmin LAN\n"
            "  host <port> [name]                  host a lobby\n"
            "  join <ip> <port> <name>              join a lobby\n"
            "  ready | players | startgame [force]\n"
            "  chat <msg> | netent | leave\n"
            "  status                              dump game state\n"
            "  help | quit\n";
    }
};

static DirectiveType directiveByName(const std::string& n) {
    if (n == "raid") return DirectiveType::RaidCity;
    if (n == "war") return DirectiveType::GoToWar;
    if (n == "convert") return DirectiveType::ConvertCampaign;
    if (n == "sacrifice") return DirectiveType::MassSacrifice;
    if (n == "defend") return DirectiveType::Defend;
    if (n == "relic") return DirectiveType::GatherRelic;
    return DirectiveType::Count;
}

// Wave 6: Radmin VPN multiplayer session for the REPL driver.
// Poll-driven (no threads): poll() is called once per REPL iteration.
struct NetSession {
    enum class Role { None, Hosting, Joined };
    Role role = Role::None;
    std::unique_ptr<net::HostLobby> host;
    std::unique_ptr<net::JoinLobby> client;
    std::unique_ptr<net::HostBeacon> beacon;
    std::unique_ptr<net::NetHost> netHost;
    std::unique_ptr<net::NetClient> netClient;
    bool inGame = false;
    bool hostReady = false;
    bool clientReady = false;
    Vec3 pendingMove{0, 0, 0};  // fed by the local "move" command
    uint8_t pendingButtons = 0;  // fed by "attack"/"cast" (edge-triggered)

    bool active() const { return role != Role::None; }

    void showIPs() {
        auto as = net::listAdapters();
        std::cout << "adapters:\n";
        for (const auto& a : as) {
            std::cout << "  " << a.name << "  " << a.ip << " / " << a.netmask;
            if (a.isRadmin())
                std::cout << "   <-- RADMIN VPN (friends join this IP)";
            else if (a.isLoopback())
                std::cout << "   (loopback)";
            std::cout << "\n";
        }
        if (as.empty()) std::cout << "  (none found)\n";
    }

    bool startHost(int port, const std::string& name) {
        if (active()) {
            std::cout << "already in a net session ('leave' first)\n";
            return false;
        }
        auto h = std::make_unique<net::HostLobby>(
            static_cast<uint16_t>(port), name.empty() ? "Host" : name);
        if (!h->start()) return false;
        auto pref = net::preferredAdapter(net::listAdapters());
        auto b = std::make_unique<net::HostBeacon>(
            name.empty() ? "Host" : name, "freeroam", h->port());
        if (!b->start(net::broadcastAddress(pref))) return false;
        role = Role::Hosting;
        host = std::move(h);
        beacon = std::move(b);
        std::cout << "hosting on " << pref.ip << ":" << host->port()
                  << " — friends run: discover, or: join " << pref.ip
                  << " " << host->port() << " <name>\n";
        return true;
    }

    bool startJoin(const std::string& ip, int port,
                   const std::string& name) {
        if (active()) {
            std::cout << "already in a net session ('leave' first)\n";
            return false;
        }
        auto c = std::make_unique<net::JoinLobby>();
        if (!c->connect(ip, static_cast<uint16_t>(port), name)) return false;
        role = Role::Joined;
        client = std::move(c);
        return true;
    }

    std::vector<net::SnapshotEntity> snapshotOf(BetaGame& g) {
        std::vector<net::SnapshotEntity> out;
        net::SnapshotEntity e;
        e.id = g.avatar.id();
        e.x = g.avatar.position().x;
        e.y = g.avatar.position().y;
        e.z = g.avatar.position().z;
        e.hp = g.avatar.hp();
        e.state = 0;
        out.push_back(e);
        for (size_t i = 0; i < g.cult.size() && out.size() < 32; ++i) {
            const Cultist& c = g.cult.at(i);
            net::SnapshotEntity se;
            se.id = c.id();
            se.x = c.position().x;
            se.y = c.position().y;
            se.z = c.position().z;
            se.hp = c.hp();
            se.state = static_cast<uint8_t>(c.state());
            out.push_back(se);
        }
        return out;
    }

    void poll(BetaGame& g) {
        if (!active()) return;
        double now = net::nowSeconds();
        if (role == Role::Hosting && host) {
            host->poll();
            if (beacon) {
                beacon->setPlayerCount(
                    static_cast<int>(host->players().size()));
                beacon->tick(now);
            }
            for (const auto& line : host->drainChat())
                std::cout << "[chat] " << line << "\n";
            if (!inGame && host->gameStarted()) {
                netHost = std::make_unique<net::NetHost>(
                    host->takeClientSockets());
                inGame = true;
                std::cout << "*** game live: simulating for "
                          << netHost->clientCount() << " client(s)\n";
            }
            if (inGame && netHost)
                netHost->poll(now, [&]() { return snapshotOf(g); });
        } else if (role == Role::Joined && client) {
            client->poll();
            for (const auto& line : client->drainChat())
                std::cout << "[chat] " << line << "\n";
            if (!inGame && client->gameStarted()) {
                netClient = std::make_unique<net::NetClient>(
                    client->takeSocket());
                inGame = true;
                std::cout << "*** game live: receiving snapshots\n";
            }
            if (inGame && netClient) {
                netClient->poll(now, [&]() {
                    net::ClientInput in;
                    in.moveX = pendingMove.x;
                    in.moveZ = pendingMove.z;
                    in.buttons = pendingButtons;
                    pendingButtons = 0;
                    return in;
                });
                if (!netClient->connected()) {
                    std::cout << "*** disconnected from host\n";
                    reset();
                }
            }
        }
    }

    void printPlayers() const {
        if (role == Role::Hosting && host) {
            for (const auto& p : host->players())
                std::cout << "  [" << p.id << "] " << p.name
                          << (p.ready ? " READY" : "")
                          << " team " << p.team << "\n";
        } else if (role == Role::Joined && client) {
            for (const auto& p : client->players())
                std::cout << "  [" << p.id << "] " << p.name
                          << (p.ready ? " READY" : "")
                          << " team " << p.team << "\n";
        } else {
            std::cout << "not in a lobby\n";
        }
    }

    void toggleReady() {
        if (role == Role::Hosting && host) {
            hostReady = !hostReady;
            host->setHostReady(hostReady);
            std::cout << "host ready: " << (hostReady ? "yes" : "no")
                      << "\n";
        } else if (role == Role::Joined && client) {
            clientReady = !clientReady;
            client->setReady(clientReady);
            std::cout << "ready: " << (clientReady ? "yes" : "no") << "\n";
        } else {
            std::cout << "not in a lobby\n";
        }
    }

    void reset() {
        role = Role::None;
        host.reset();
        client.reset();
        beacon.reset();
        netHost.reset();
        netClient.reset();
        inGame = false;
        hostReady = clientReady = false;
        pendingButtons = 0;
    }
};

int main() {
    BetaGame g;
    g.setupWorld();
    NetSession nets;

    std::cout << "CULT-ULHU playable beta — free roam\n"
              << "Your eldritch avatar stalks the map. Type 'help'.\n";
    g.printStatus();

    std::string line;
    while (std::cout << "\n> " && std::getline(std::cin, line)) {
        nets.poll(g);  // pump multiplayer (non-blocking)
        std::istringstream in(line);
        std::string cmd;
        in >> cmd;
        if (cmd.empty()) continue;

        if (cmd == "quit" || cmd == "exit") break;
        if (cmd == "help") { g.printHelp(); continue; }
        if (cmd == "status") { g.printStatus(); continue; }
        if (cmd == "beliefs") {
            std::cout << "active:";
            for (Belief b : g.beliefs.active())
                std::cout << " " << beliefName(b);
            std::cout << "\n";
            continue;
        }

        if (cmd == "move") {
            std::string dir; int steps = 1;
            in >> dir >> steps;
            if (steps < 1) steps = 1; if (steps > 50) steps = 50;
            Vec3 d{0, 0, 0};
            if (dir.find('n') != std::string::npos) d.z -= 1;
            if (dir.find('s') != std::string::npos) d.z += 1;
            if (dir.find('e') != std::string::npos) d.x += 1;
            if (dir.find('w') != std::string::npos) d.x -= 1;
            if (d.x == 0 && d.z == 0) {
                std::cout << "usage: move <n|s|e|w|ne|nw|se|sw> [steps]\n";
                continue;
            }
            nets.pendingMove = d;  // also feeds multiplayer input
            for (int i = 0; i < steps; ++i) {
                g.avatar.move(d, 1.0, 8.0f);
                g.avatar.setFacingYaw(std::atan2(d.z, d.x));
                g.tickSecond();
            }
            g.avatarAnim.requestState(steps > 3 ? AnimationState::Run
                                                : AnimationState::Walk);
            std::cout << "moved to (" << g.avatar.position().x << ", "
                      << g.avatar.position().z << ")\n";
            continue;
        }

        if (cmd == "camera") {
            std::string m; in >> m;
            if (m == "fp") g.camera.setMode(CameraMode::FirstPerson);
            else if (m == "tp") g.camera.setMode(CameraMode::ThirdPerson);
            else {
                std::cout << "usage: camera <fp|tp>\n";
                continue;
            }
            CameraPose p = g.camera.poseFor(g.avatar.position(),
                                            g.avatar.facingYaw());
            std::cout << "camera: " << cameraModeName(g.camera.mode())
                      << " eye=(" << p.eye.x << "," << p.eye.y << ","
                      << p.eye.z << ")\n";
            continue;
        }

        if (cmd == "look") {
            std::cout << g.freeroam.civilians().size() << " civilians, "
                      << g.freeroam.creatures().size() << " creatures, "
                      << g.freeroam.relics().size() << " relics on the map. ";
            Entity* t = g.nearestTarget(g.avatar.position(), 200.0f);
            if (t)
                std::cout << "nearest target " << dist(g.avatar.position(),
                                                       t->position())
                          << "m away (hp " << t->hp() << ").\n";
            else
                std::cout << "no targets in range.\n";
            continue;
        }

        if (cmd == "spawn") {
            std::string what; int n = 1;
            in >> what >> n;
            if (n < 1) n = 1; if (n > 20) n = 20;
            Vec3 p = g.avatar.position();
            for (int i = 0; i < n; ++i) {
                Vec3 q{p.x + i * 2.0f, 0, p.z + 2.0f};
                if (what == "cultist") {
                    Cultist& c = g.cult.recruit();
                    c.setPosition(q);
                } else if (what == "sorcerer") {
                    g.world.push_back(std::make_unique<Sorcerer>(
                        FACTION_CTHULHU, q));
                } else if (what == "monstrosity") {
                    g.world.push_back(std::make_unique<Monstrosity>(
                        FACTION_CTHULHU, q, "spawned", false));
                } else if (what == "civilian") {
                    g.world.push_back(
                        std::make_unique<Civilian>(q));
                } else {
                    std::cout << "unknown: " << what << "\n";
                    break;
                }
            }
            std::cout << "spawned " << n << " " << what << "(s)\n";
            continue;
        }

        if (cmd == "belief") {
            std::string name, rw, old;
            in >> name >> rw >> old;
            Belief b = beliefByName(name);
            if (b == Belief::Count) {
                std::cout << "unknown belief\n";
                continue;
            }
            Belief out = Belief::Count;
            if (rw == "replace") {
                out = beliefByName(old);
                if (out == Belief::Count) {
                    std::cout << "unknown belief to replace\n";
                    continue;
                }
            }
            if (g.beliefs.requestChange(b, out))
                std::cout << beliefName(b)
                          << " adoption started (~120s game time)\n";
            else
                std::cout << "cannot adopt (already active or no room)\n";
            continue;
        }

        if (cmd == "rest") {
            size_t i; in >> i;
            if (i >= g.cult.size()) {
                std::cout << "no cultist " << i << "\n";
                continue;
            }
            Cultist& c = g.cult.at(i);
            if (g.dreams.isResting(c.id())) {
                g.dreams.endRest(c.id());
                std::cout << "cultist " << i << " wakes\n";
            } else {
                g.dreams.startRest(c.id());
                std::cout << "cultist " << i << " rests (dream-visions)\n";
            }
            continue;
        }

        if (cmd == "command") {
            std::string what; in >> what;
            DirectiveType d = directiveByName(what);
            if (d == DirectiveType::Count) {
                std::cout << "usage: command "
                             "<raid|war|convert|sacrifice|defend|relic>\n";
                continue;
            }
            CommandResult r = g.commands.issueCommand(d,
                                                      g.avatar.position());
            std::cout << commandOutcomeName(r.outcome) << " — "
                      << r.detail << "\n";
            continue;
        }

        if (cmd == "attack") {
            nets.pendingButtons |= 1;  // also feeds multiplayer input
            Entity* t = g.nearestTarget(g.avatar.position());
            if (!t) { std::cout << "no target in range\n"; continue; }
            g.avatarAnim.requestState(AnimationState::Attack);
            float dmg = strikeMelee(g.avatar.id(), EntityType::EldritchAvatar,
                                    *t, 40.0f, g.beliefs, g.bus, g.fx,
                                    EventType::CivilianSlain,
                                    g.exertion.stats().combatPowerMult);
            std::cout << "struck for " << dmg << " (target hp " << t->hp()
                      << ")\n";
            g.tickSecond();
            continue;
        }

        if (cmd == "cast") {
            std::string spell; in >> spell;
            Entity* t = g.nearestTarget(g.avatar.position());
            if (!t) { std::cout << "no target in range\n"; continue; }
            // The avatar channels through its sorcerer attendant's craft.
            g.avatarAnim.requestState(AnimationState::Cast);
            float dmg;
            if (spell == "fear") {
                dmg = castSpell(g.avatar.id(), EntityType::Sorcerer, *t,
                                20.0f, DamageType::Shadow, CCType::Fear, 4.0f,
                                g.beliefs, g.bus, g.fx,
                                EventType::CivilianSlain,
                                g.exertion.stats().combatPowerMult);
                std::cout << "fear cast for " << dmg << "\n";
            } else if (spell == "fireball") {
                dmg = castSpell(g.avatar.id(), EntityType::Sorcerer, *t,
                                35.0f, DamageType::Fire, CCType::Stun, 3.0f,
                                g.beliefs, g.bus, g.fx,
                                EventType::CivilianSlain,
                                g.exertion.stats().combatPowerMult);
                std::cout << "fireball for " << dmg << "\n";
            } else {
                std::cout << "usage: cast <fireball|fear>\n";
                continue;
            }
            std::cout << "target hp " << t->hp() << "\n";
            g.tickSecond();
            continue;
        }

        if (cmd == "tick") {
            int n = 1;
            in >> n;
            if (n < 1) n = 1; if (n > 3600) n = 3600;
            const float before = g.power.value();
            for (int i = 0; i < n; ++i) g.tickSecond();
            std::cout << "t+" << n << "s  power " << before << " -> "
                      << g.power.value() << "  (" << g.freeroam.hourOfDay()
                      << "h)\n";
            continue;
        }

        if (cmd == "save") {
            std::string file; in >> file;
            if (file.empty()) {
                std::cout << "usage: save <file>\n";
                continue;
            }
            GameState s = g.buildSaveState();
            if (SaveSystem::save(s, file))
                std::cout << "saved t=" << s.clockTime << "s power="
                          << s.power << " beliefs=" << s.activeBeliefs.size()
                          << " " << s.entities.size() << " entities -> "
                          << file << "\n";
            else
                std::cout << "save failed: " << file << "\n";
            continue;
        }

        if (cmd == "load") {
            std::string file; in >> file;
            if (file.empty()) {
                std::cout << "usage: load <file>\n";
                continue;
            }
            GameState s;
            if (!SaveSystem::load(file, s)) {
                std::cout << "load failed: " << file << "\n";
                continue;
            }
            g.applySaveState(s);
            std::cout << "loaded " << file << ": t=" << s.clockTime
                      << "s power=" << s.power << " beliefs="
                      << s.activeBeliefs.size() << " entities="
                      << s.entities.size() << "\n";
            continue;
        }

        // ---- Wave 6: Radmin VPN multiplayer ----
        if (cmd == "myip") {
            nets.showIPs();
            continue;
        }

        if (cmd == "discover") {
            int secs = 3;
            in >> secs;
            if (secs < 1) secs = 1;
            if (secs > 15) secs = 15;
            net::DiscoveryClient dc;
            if (!dc.start()) {
                std::cout << "discovery failed (UDP unavailable?)\n";
                continue;
            }
            std::cout << "listening for hosts (" << secs << "s)...\n";
            auto hosts = dc.listenFor(secs * 1000);
            if (hosts.empty()) {
                std::cout << "no hosts found. Is the host's beacon running "
                             "on your Radmin network?\n";
            } else {
                for (const auto& h : hosts)
                    std::cout << "  " << h.hostName << "  " << h.ip << ":"
                              << h.tcpPort << "  " << h.mode << "  "
                              << h.players << "/" << h.maxPlayers << "\n";
            }
            continue;
        }

        if (cmd == "host") {
            int port = 47778;
            std::string name;
            in >> port >> name;
            if (port <= 0 || port > 65535) {
                std::cout << "usage: host <port> [name]\n";
                continue;
            }
            nets.startHost(port, name);
            continue;
        }

        if (cmd == "join") {
            std::string ip, name;
            int port = 0;
            in >> ip >> port >> name;
            if (ip.empty() || port <= 0 || port > 65535) {
                std::cout << "usage: join <ip> <port> <name>\n";
                continue;
            }
            nets.startJoin(ip, port, name.empty() ? "Cultist" : name);
            continue;
        }

        if (cmd == "ready") {
            nets.toggleReady();
            continue;
        }

        if (cmd == "players") {
            nets.printPlayers();
            continue;
        }

        if (cmd == "startgame") {
            std::string f;
            in >> f;
            if (nets.role != NetSession::Role::Hosting || !nets.host) {
                std::cout << "only the host can start the game\n";
                continue;
            }
            nets.host->startGame(f == "force");
            continue;
        }

        if (cmd == "chat") {
            std::string text;
            std::getline(in, text);
            while (!text.empty() && text.front() == ' ') text.erase(0, 1);
            if (text.empty()) {
                std::cout << "usage: chat <message>\n";
                continue;
            }
            if (nets.role == NetSession::Role::Hosting && nets.host)
                nets.host->sendChatAll("Host", text);
            else if (nets.role == NetSession::Role::Joined && nets.client)
                nets.client->sendChat(text);
            else
                std::cout << "not in a net session\n";
            continue;
        }

        if (cmd == "netent") {
            if (nets.netClient) {
                const auto& ents = nets.netClient->entities();
                std::cout << ents.size() << " snapshot entities (tick "
                          << nets.netClient->lastTick() << "):\n";
                for (const auto& [id, e] : ents)
                    std::cout << "  id=" << id << " (" << e.x << "," << e.z
                              << ") hp=" << e.hp << "\n";
            } else {
                std::cout << "no client snapshot stream (join a game first)\n";
            }
            continue;
        }

        if (cmd == "leave") {
            nets.reset();
            std::cout << "left net session\n";
            continue;
        }
        // ---- end Wave 6 ----

        std::cout << "unknown command. Type 'help'.\n";
    }

    std::cout << "The dream ends. Power: " << g.power.value() << "\n";
    return 0;
}
