// CULT-ULHU playable beta driver: an interactive console REPL so the game is
// playable headless right now. The engine binding (Unreal/Unity) will replace
// this with real rendering later; all game logic lives in the core library.

#include "ai/AmbientBehavior.h"
#include "ai/RitualCaster.h"
#include "animation/AnimationStateMachine.h"
#include "assets/AssetManager.h"
#include "assets/GlbInfo.h"
#include "assets/ModelCatalog.h"
#include "beliefs/BeliefSystem.h"
#include "camera/CameraSystem.h"
#include "characters/CharacterPackageLoader.h"
#include "characters/CharacterValidator.h"
#include "characters/abilities/WaveOfDomination.h"
#include "chaos/LunaticSystem.h"
#include "combat/Attacks.h"
#include "combat/Combos.h"
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
#include "input/InputManager.h"
#include "modes/FreeRoamMode.h"
#include "net/Discovery.h"
#include "net/Lobby.h"
#include "net/Netcode.h"
#include "net/PlayerStats.h"
#include "net/RadminNet.h"
#include "net/Socket.h"
#include "power/PowerSystem.h"
#include "save/SaveSystem.h"
#include "ui/CommandMenu.h"
#include "world/MapLoader.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
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
// Wave 7: range extended to EntityType::Altar (newest enumerator; keep last).
bool validEntityType(int t) {
    return t >= static_cast<int>(EntityType::GreatOldOne) &&
           t <= static_cast<int>(EntityType::Altar);
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

    // Wave 7: driver-local construction sites. The `build` command starts
    // one at the avatar's position; tickSecond() advances them, and
    // finished sites spawn real buildings into `world`.
    std::vector<std::unique_ptr<ConstructionSite>> sites;

    // Wave 7: playable character packages. Auto-loaded at startup from
    // assets/characters/; `chars`/`addchar`/`validate` manage them.
    CharacterRegistry charReg;
    CharacterPackageLoader charLoader{charReg};

    // Wave 7: RMB state-machine demo (Cthulhu Avatar's Wave of Domination).
    // The ability is persistent; the context is re-pointed at the current
    // world for each `rmb` subcommand.
    WaveOfDomination waveAbility;
    RmbContext waveCtx{bus, fx, beliefs, rng};

    BetaGame() {
        // The exertion system now owns the power pipeline (event -> belief
        // power rules -> synergy multipliers -> PowerSystem). Narration for
        // the notable moments.
        dreams.setExertion(&exertion);
        ambient.setExertion(&exertion); // wave 9c: Dreams/Reconstruction
                                       // synergies for omen-reading/tending
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
        // Wave 7: driver construction sites progress; finished ones spawn
        // into the world. The god counts as labor (build sites get one
        // builder, so a solo site finishes in 120s of game time).
        for (auto& s : sites) s->update(1.0);
        for (auto it = sites.begin(); it != sites.end();) {
            if (!(*it)->finished()) { ++it; continue; }
            BuildingType t = (*it)->targetType();
            Vec3 p = (*it)->position();
            auto b = (*it)->complete();
            std::cout << "[build] " << buildingTypeName(t)
                      << " finished at (" << p.x << ", " << p.z << ")\n";
            if (b) world.push_back(std::move(b));
            it = sites.erase(it);
        }
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
        std::cout << "sites: " << sites.size() << " under construction";
        for (const auto& s : sites)
            std::cout << " [" << buildingTypeName(s->targetType()) << " "
                      << static_cast<int>(s->progress() * 100.0f) << "%]";
        std::cout << "\ncharacters: " << charReg.count()
                  << " registered\n";
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
        // Wave 9d: the old world's entities are gone; the Wave of
        // Domination ability may hold raw pointers to them. Cancel it
        // (pruneStaleVictims is the backstop inside the ability).
        waveAbility.cancel();
        waveCtx.entities.clear();
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
            "  spawn <monstrosity|creature> [species] [n]  (see 'bestiary')\n"
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
            "  build <wall|barracks|watchtower|trap|portal|altar>\n"
            "                                          start construction\n"
            "  menu [cultist|location|enemy|altar]  radial command menu demo\n"
            "  mapinfo                             map art: placements, file\n"
            "                                          check, asset budget\n"
            "  bestiary                            creature/NPC models\n"
            "  dungeon [seed] | dungeon pnath [seed]\n"
            "                                          dungeon map / Vale of Pnath\n"
            "  combo                               melee combo chain demo\n"
            "  chars | addchar <folder> | validate <id>\n"
            "                                          character packages\n"
            "  kda                                 demo KDA tracking\n"
            "  interact                            E-interact demo\n"
            "  jump | sprint <on|off>              input state demos\n"
            "  rmb <press|move|launch|release|stun> Wave of Domination demo\n"
            "  status                              dump game state\n"
            "  help | quit\n";
    }
};

// Wave 9c driver commands (`ambient`, `dungeon`): header-inline module;
// the implementation compiles into this TU via the include below.
#include "driver/wave9_content_cmds.h"

// Wave 9b driver commands (`directive assassinate|blight|summon|zones`):
// header-inline module; the implementation compiles into this TU via the
// include below.
#include "driver/wave9_directives_cmds.h"

static DirectiveType directiveByName(const std::string& n) {
    if (n == "raid") return DirectiveType::RaidCity;
    if (n == "war") return DirectiveType::GoToWar;
    if (n == "convert") return DirectiveType::ConvertCampaign;
    if (n == "sacrifice") return DirectiveType::MassSacrifice;
    if (n == "defend") return DirectiveType::Defend;
    if (n == "relic") return DirectiveType::GatherRelic;
    return DirectiveType::Count;
}

// Wave 7: re-point the persistent RMB context at the current world.
static void fillRmbCtx(BetaGame& g) {
    g.waveCtx.caster = &g.avatar;
    g.waveCtx.casterYaw = g.avatar.facingYaw();
    g.waveCtx.entities.clear();
    for (const auto& c : g.freeroam.civilians())
        g.waveCtx.entities.push_back(c.get());
    for (const auto& c : g.freeroam.creatures())
        g.waveCtx.entities.push_back(c.get());
    for (const auto& e : g.world)
        g.waveCtx.entities.push_back(e.get());
    g.waveCtx.targetedEntityId = 0;
}

static const char* wavePhaseName(WaveOfDomination::Phase p) {
    switch (p) {
        case WaveOfDomination::Phase::Idle:   return "Idle";
        case WaveOfDomination::Phase::Wave:   return "Wave";
        case WaveOfDomination::Phase::Hold:   return "Hold";
        case WaveOfDomination::Phase::Flying: return "Flying";
    }
    return "?";
}

static void printWaveState(BetaGame& g) {
    std::cout << "  phase=" << wavePhaseName(g.waveAbility.phase())
              << " victims=" << g.waveAbility.victimCount()
              << " cooldown=" << g.waveAbility.cooldownRemaining() << "s"
              << " caster-anim="
              << animationStateName(g.waveAbility.suggestedCasterState())
              << "\n";
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

// ---------------------------------------------------------------------------
// One REPL input line through the full driver dispatch. Extracted from
// main()'s loop so the fuzz harness (src/fuzz) can drive it directly.
// Returns false when the session should end (quit/exit).
// ---------------------------------------------------------------------------
static bool processLine(BetaGame& g, NetSession& nets,
                        const std::string& line) {
        nets.poll(g);  // pump multiplayer (non-blocking)
        std::istringstream in(line);
        std::string cmd;
        in >> cmd;
        if (cmd.empty()) return true;

        // Wave 9c content commands (ambient/dungeon); one registration line.
        if (registerWave9ContentCommands(g, cmd, in)) return true;

        // Wave 9b directive commands (directive assassinate|blight|summon);
        // one registration line.
        if (registerWave9DirectiveCommands(g, cmd, in)) return true;

        if (cmd == "quit" || cmd == "exit") return false;
        if (cmd == "help") { g.printHelp(); return true; }
        if (cmd == "status") { g.printStatus(); return true; }
        if (cmd == "beliefs") {
            std::cout << "active:";
            for (Belief b : g.beliefs.active())
                std::cout << " " << beliefName(b);
            std::cout << "\n";
            return true;
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
                return true;
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
            return true;
        }

        if (cmd == "camera") {
            std::string m; in >> m;
            if (m == "fp") g.camera.setMode(CameraMode::FirstPerson);
            else if (m == "tp") g.camera.setMode(CameraMode::ThirdPerson);
            else {
                std::cout << "usage: camera <fp|tp>\n";
                return true;
            }
            CameraPose p = g.camera.poseFor(g.avatar.position(),
                                            g.avatar.facingYaw());
            std::cout << "camera: " << cameraModeName(g.camera.mode())
                      << " eye=(" << p.eye.x << "," << p.eye.y << ","
                      << p.eye.z << ")\n";
            return true;
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
            return true;
        }

        if (cmd == "spawn") {
            // spawn <cultist|civilian|sorcerer> [n]
            // spawn <monstrosity|creature> [species] [n]  (species optional;
            //   a bare number is treated as the count, default species
            //   "spawned")
            std::string what;
            in >> what;
            int n = 1;
            std::string species = "spawned";
            const bool isMonster = (what == "monstrosity" || what == "creature");
            {
                std::string tok;
                auto isNum = [](const std::string& t) {
                    return !t.empty() &&
                           t.find_first_not_of("0123456789") ==
                               std::string::npos;
                };
                if (in >> tok) {
                    if (isMonster) {
                        if (isNum(tok)) {
                            n = std::stoi(tok);
                        } else {
                            species = tok;
                            std::string tok2;
                            if (in >> tok2 && isNum(tok2))
                                n = std::stoi(tok2);
                        }
                    } else if (isNum(tok)) {
                        n = std::stoi(tok);
                    }
                    // else: unrecognized trailing token — ignore like the
                    // old `in >> n` failbit path did.
                }
            }
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
                } else if (isMonster) {
                    auto m = std::make_unique<Monstrosity>(
                        FACTION_CTHULHU, q, species, false);
                    const std::string model =
                        ModelCatalog::creatureModel(species);
                    if (i == 0)
                        std::cout << "spawned " << n << " " << what << "(s) '"
                                  << species << "'"
                                  << (model.empty() ? " (no model: logic-only)"
                                                    : " -> " + model)
                                  << "\n";
                    g.world.push_back(std::move(m));
                } else if (what == "civilian") {
                    g.world.push_back(
                        std::make_unique<Civilian>(q));
                } else {
                    std::cout << "unknown: " << what << "\n";
                    break;
                }
            }
            if (!isMonster)
                std::cout << "spawned " << n << " " << what << "(s)\n";
            return true;
        }

        if (cmd == "belief") {
            std::string name, rw, old;
            in >> name >> rw >> old;
            Belief b = beliefByName(name);
            if (b == Belief::Count) {
                std::cout << "unknown belief\n";
                return true;
            }
            Belief out = Belief::Count;
            if (rw == "replace") {
                out = beliefByName(old);
                if (out == Belief::Count) {
                    std::cout << "unknown belief to replace\n";
                    return true;
                }
            }
            if (g.beliefs.requestChange(b, out))
                std::cout << beliefName(b)
                          << " adoption started (~120s game time)\n";
            else
                std::cout << "cannot adopt (already active or no room)\n";
            return true;
        }

        if (cmd == "rest") {
            size_t i; in >> i;
            if (i >= g.cult.size()) {
                std::cout << "no cultist " << i << "\n";
                return true;
            }
            Cultist& c = g.cult.at(i);
            if (g.dreams.isResting(c.id())) {
                g.dreams.endRest(c.id());
                std::cout << "cultist " << i << " wakes\n";
            } else {
                g.dreams.startRest(c.id());
                std::cout << "cultist " << i << " rests (dream-visions)\n";
            }
            return true;
        }

        if (cmd == "command") {
            std::string what; in >> what;
            DirectiveType d = directiveByName(what);
            if (d == DirectiveType::Count) {
                std::cout << "usage: command "
                             "<raid|war|convert|sacrifice|defend|relic>\n";
                return true;
            }
            CommandResult r = g.commands.issueCommand(d,
                                                      g.avatar.position());
            std::cout << commandOutcomeName(r.outcome) << " — "
                      << r.detail << "\n";
            return true;
        }

        if (cmd == "attack") {
            nets.pendingButtons |= 1;  // also feeds multiplayer input
            Entity* t = g.nearestTarget(g.avatar.position());
            if (!t) { std::cout << "no target in range\n"; return true; }
            g.avatarAnim.requestState(AnimationState::Attack);
            float dmg = strikeMelee(g.avatar.id(), EntityType::EldritchAvatar,
                                    *t, 40.0f, g.beliefs, g.bus, g.fx,
                                    EventType::CivilianSlain,
                                    g.exertion.stats().combatPowerMult);
            std::cout << "struck for " << dmg << " (target hp " << t->hp()
                      << ")\n";
            g.tickSecond();
            return true;
        }

        if (cmd == "cast") {
            std::string spell; in >> spell;
            Entity* t = g.nearestTarget(g.avatar.position());
            if (!t) { std::cout << "no target in range\n"; return true; }
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
                return true;
            }
            std::cout << "target hp " << t->hp() << "\n";
            g.tickSecond();
            return true;
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
            return true;
        }

        if (cmd == "save") {
            std::string file; in >> file;
            if (file.empty()) {
                std::cout << "usage: save <file>\n";
                return true;
            }
            GameState s = g.buildSaveState();
            if (SaveSystem::save(s, file))
                std::cout << "saved t=" << s.clockTime << "s power="
                          << s.power << " beliefs=" << s.activeBeliefs.size()
                          << " " << s.entities.size() << " entities -> "
                          << file << "\n";
            else
                std::cout << "save failed: " << file << "\n";
            return true;
        }

        if (cmd == "load") {
            std::string file; in >> file;
            if (file.empty()) {
                std::cout << "usage: load <file>\n";
                return true;
            }
            GameState s;
            if (!SaveSystem::load(file, s)) {
                std::cout << "load failed: " << file << "\n";
                return true;
            }
            g.applySaveState(s);
            std::cout << "loaded " << file << ": t=" << s.clockTime
                      << "s power=" << s.power << " beliefs="
                      << s.activeBeliefs.size() << " entities="
                      << s.entities.size() << "\n";
            return true;
        }

        // ---- Wave 6: Radmin VPN multiplayer ----
        if (cmd == "myip") {
            nets.showIPs();
            return true;
        }

        if (cmd == "discover") {
            int secs = 3;
            in >> secs;
            if (secs < 1) secs = 1;
            if (secs > 15) secs = 15;
            net::DiscoveryClient dc;
            if (!dc.start()) {
                std::cout << "discovery failed (UDP unavailable?)\n";
                return true;
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
            return true;
        }

        if (cmd == "host") {
            int port = 47778;
            std::string name;
            in >> port >> name;
            if (port <= 0 || port > 65535) {
                std::cout << "usage: host <port> [name]\n";
                return true;
            }
            nets.startHost(port, name);
            return true;
        }

        if (cmd == "join") {
            std::string ip, name;
            int port = 0;
            in >> ip >> port >> name;
            if (ip.empty() || port <= 0 || port > 65535) {
                std::cout << "usage: join <ip> <port> <name>\n";
                return true;
            }
            nets.startJoin(ip, port, name.empty() ? "Cultist" : name);
            return true;
        }

        if (cmd == "ready") {
            nets.toggleReady();
            return true;
        }

        if (cmd == "players") {
            nets.printPlayers();
            return true;
        }

        if (cmd == "startgame") {
            std::string f;
            in >> f;
            if (nets.role != NetSession::Role::Hosting || !nets.host) {
                std::cout << "only the host can start the game\n";
                return true;
            }
            nets.host->startGame(f == "force");
            return true;
        }

        if (cmd == "chat") {
            std::string text;
            std::getline(in, text);
            while (!text.empty() && text.front() == ' ') text.erase(0, 1);
            if (text.empty()) {
                std::cout << "usage: chat <message>\n";
                return true;
            }
            if (nets.role == NetSession::Role::Hosting && nets.host)
                nets.host->sendChatAll("Host", text);
            else if (nets.role == NetSession::Role::Joined && nets.client)
                nets.client->sendChat(text);
            else
                std::cout << "not in a net session\n";
            return true;
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
            return true;
        }

        if (cmd == "leave") {
            nets.reset();
            std::cout << "left net session\n";
            return true;
        }
        // ---- end Wave 6 ----

        // ---- Wave 7: world & controls driver demos ----
        if (cmd == "build") {
            std::string what; in >> what;
            BuildingType t;
            if (what == "wall") t = BuildingType::Wall;
            else if (what == "barracks") t = BuildingType::Barracks;
            else if (what == "watchtower") t = BuildingType::Watchtower;
            else if (what == "trap") t = BuildingType::Trap;
            else if (what == "portal") t = BuildingType::Portal;
            else if (what == "altar") t = BuildingType::Altar;
            else {
                std::cout << "usage: build "
                             "<wall|barracks|watchtower|trap|portal|altar>\n";
                return true;
            }
            Vec3 p = g.avatar.position();
            auto site = std::make_unique<ConstructionSite>(
                FACTION_CTHULHU, p, t);
            site->addBuilder(g.avatar.id());  // the god breaks ground
            g.sites.push_back(std::move(site));
            std::cout << "construction started: " << buildingTypeName(t)
                      << " (" << defaultHpFor(t) << " hp when done) at ("
                      << p.x << ", " << p.z << ") — the avatar works it; "
                      << "solo finish in ~120s game time ('tick 121')\n";
            return true;
        }

        if (cmd == "menu") {
            std::string kind; in >> kind;
            if (kind.empty()) kind = "location";
            MenuContext ctx;
            Vec3 p = g.avatar.position();
            if (kind == "cultist") {
                if (g.cult.size() == 0) {
                    std::cout << "no cultists — 'spawn cultist' first\n";
                    return true;
                }
                ctx.kind = MenuContext::Kind::OwnCultist;
                ctx.entityId = g.cult.at(0).id();
                ctx.pos = g.cult.at(0).position();
            } else if (kind == "location") {
                ctx.kind = MenuContext::Kind::Location;
                ctx.pos = Vec3{p.x + 20.0f, 0, p.z};
            } else if (kind == "enemy") {
                ctx.kind = MenuContext::Kind::Enemy;
                if (Entity* e = g.nearestTarget(p, 200.0f)) {
                    ctx.entityId = e->id();
                    ctx.pos = e->position();
                } else {
                    ctx.pos = Vec3{p.x + 30.0f, 0, p.z};
                }
            } else if (kind == "altar") {
                ctx.kind = MenuContext::Kind::Altar;
                for (const auto& e : g.world)
                    if (e->type() == EntityType::Altar) {
                        ctx.entityId = e->id();
                        ctx.pos = e->position();
                        break;
                    }
                if (ctx.entityId == 0) ctx.pos = p;
            } else {
                std::cout << "usage: menu [cultist|location|enemy|altar]\n";
                return true;
            }
            GameStateSummary summary;
            summary.commandableCultists =
                static_cast<int>(g.cult.size());
            CommandMenu menu;
            std::vector<MenuButton> buttons =
                menu.generateMenu(ctx, summary);
            std::cout << CommandMenu::renderText(buttons);
            // Demo dispatch: fire the first enabled non-Cancel button.
            for (const auto& b : buttons) {
                if (b.action == MenuAction::Cancel || !b.enabled) continue;
                if (menu.dispatch(b, ctx, g.commands, g.executor)) {
                    std::cout << "[demo dispatch] "
                              << menuActionName(b.action) << " -> ";
                    if (!menu.issuedOrders().empty()) {
                        std::cout << "queued order(s):";
                        for (const auto& o : menu.issuedOrders())
                            std::cout << " ["
                                      << menuActionName(o.action) << "]";
                    } else {
                        std::cout << "whole-cult directive issued";
                    }
                    std::cout << "\n";
                }
                break;
            }
            return true;
        }

        // ---- Wave 11: art pass map info ----
        if (cmd == "mapinfo") {
            // Resolve assets/maps/ruined_city.map like the character
            // packages above (repo root or one level up when run from
            // build/).
            std::string prefix;
            {
                std::ifstream probe("assets/maps/ruined_city.map");
                if (!probe) prefix = "../";
            }
            const std::string mapPath = prefix + "assets/maps/ruined_city.map";
            MapData map;
            try {
                map = MapLoader::load(mapPath);
            } catch (const MapParseError& e) {
                std::cout << "MAP PARSE ERROR: " << e.what() << "\n";
                return true;
            }
            std::cout << "map: " << map.name << " (" << mapPath << ")\n";
            std::cout << "zones: " << map.zones.size() << "\n";
            for (const auto& z : map.zones)
                std::cout << "  " << z.name << " fear=" << z.ambient.ambientFear
                          << "\n";
            // Group placements by category, then by model.
            std::map<std::string, std::map<std::string, int>> byCat;
            for (const auto& p : map.placements)
                byCat[ModelCatalog::categoryOf(p.modelPath)][p.modelPath]++;
            std::cout << "placements: " << map.placements.size() << "\n";
            for (const auto& [cat, models] : byCat) {
                int catTotal = 0;
                for (const auto& [m, n] : models) catTotal += n;
                std::cout << "  [" << (cat.empty() ? "?" : cat) << "] "
                          << catTotal << " placements, " << models.size()
                          << " models:\n";
                for (const auto& [m, n] : models) {
                    // Triangle count from the GLB header (budget: <5000).
                    std::string tris = "?";
                    GlbStats gs = readGlbStats(prefix + m);
                    if (gs.ok) tris = std::to_string(gs.triangles);
                    std::cout << "    x" << n << " " << m << " [" << tris
                              << " tris]\n";
                }
            }
            // Validate every referenced file exists on disk. Missing
            // files are LOUD, never silent.
            std::set<std::string> distinct;
            for (const auto& p : map.placements)
                distinct.insert(prefix + p.modelPath);
            int missing = 0;
            uint64_t totalBytes = 0;
            for (const auto& f : distinct) {
                std::ifstream probe(f, std::ios::binary);
                if (!probe) {
                    std::cout << "MISSING: " << f << "\n";
                    ++missing;
                    continue;
                }
                probe.seekg(0, std::ios::end);
                totalBytes += static_cast<uint64_t>(probe.tellg());
            }
            const int okFiles =
                static_cast<int>(distinct.size()) - missing;
            std::cout << "file check: " << okFiles << "/"
                      << distinct.size() << " distinct models present";
            if (missing > 0)
                std::cout << " -- " << missing << " MISSING (see above)";
            std::cout << "\n";
            const double mb =
                static_cast<double>(totalBytes) / (1024.0 * 1024.0);
            std::cout << "asset budget: " << distinct.size()
                      << " models, " << mb << " MB on disk (cap 30 MB)\n";
            // MANIFEST.md (asset worker delivery): best-effort peek.
            {
                std::ifstream mf(prefix + "assets/world/MANIFEST.md");
                if (mf) {
                    std::string text((std::istreambuf_iterator<char>(mf)),
                                     std::istreambuf_iterator<char>());
                    size_t glbRefs = 0, pos = 0;
                    while ((pos = text.find(".glb", pos)) != std::string::npos) {
                        ++glbRefs; pos += 4;
                    }
                    std::cout << "manifest: assets/world/MANIFEST.md present "
                              << "(mentions ~" << glbRefs << " .glb refs; "
                              << "disk totals shown above)\n";
                } else {
                    std::cout << "manifest: assets/world/MANIFEST.md not "
                              << "present yet (asset worker pending)\n";
                }
            }
            return true;
        }

        if (cmd == "bestiary") {
            // Wave 12: list every creature/NPC model registered in the
            // catalog (species/role key -> .glb), with tri counts and a
            // loud file-presence check. Binaries are base64-packed
            // (*.glb.b64); run assets/decode_assets.py after cloning.
            std::string prefix;
            {
                std::ifstream probe("assets/creatures/MANIFEST.md");
                if (!probe) prefix = "../";
            }
            // Bestiary flavor for known species keys (role keys first).
            const std::map<std::string, std::string> flavor{
                {"cultist", "Hooded Acolyte — rank-and-file cultist"},
                {"civilian", "Villager — future convert or victim"},
                {"adventurer", "Town Guard — city watch NPC"},
                {"sorcerer", "Ritual Magus — cult leader figure"},
                {"pale_wight", "Pale Wight — robed skeletal oracle"},
                {"ossified_brute", "Ossified Brute — heavy bone horror"},
                {"charnel_imp", "Charnel Imp — swarm chaff"},
                {"skittering_ghoul", "Skittering Ghoul — fast skirmisher"},
                {"wraith", "Wraith — lesser servitor, drifts between graves"},
                {"risen_dead", "Risen Dead — reanimated corpse"},
                {"dagon_spawn", "Dagon Spawn — deep-one hybrid brute"},
            };
            const auto& table = ModelCatalog::creatureModels();
            std::cout << "bestiary: " << table.size() << " entries\n";
            int missing = 0;
            for (const auto& [key, path] : table) {
                const std::string full = prefix + path;
                std::ifstream probe(full, std::ios::binary);
                std::string tris = "?";
                if (probe) {
                    GlbStats gs = readGlbStats(full);
                    if (gs.ok) tris = std::to_string(gs.triangles);
                } else {
                    ++missing;
                }
                std::cout << "  " << key << " -> " << path << " [" << tris
                          << " tris] " << (probe ? "present" : "MISSING")
                          << "\n";
                auto fi = flavor.find(key);
                if (fi != flavor.end())
                    std::cout << "      " << fi->second << "\n";
            }
            if (missing > 0)
                std::cout << missing << " MISSING files (see above)\n";
            return true;
        }

        if (cmd == "combo") {
            ComboTracker tr;  // default basic_flurry
            auto show = [&]() {
                const int st = tr.currentStage();
                if (st == 0) {
                    std::cout << "  chain inactive\n";
                    return;
                }
                const ComboStage* s = tr.stageDef();
                std::cout << "  stage " << st << ": anim=" << s->animState
                          << " dmg x" << s->damageMult
                          << (s->ccType.empty()
                                  ? ", no CC"
                                  : ", CC=" + s->ccType + " " +
                                        std::to_string(s->ccSeconds) +
                                        "s")
                          << "\n";
            };
            std::cout << "LMB chain, 0.5s apart (window 1.2s):\n";
            tr.onLmbClick(0.0); show();
            tr.onLmbClick(0.5); show();
            tr.onLmbClick(1.0); show();
            std::cout << "click 4s late (t=5.0): chain resets to stage 1\n";
            tr.onLmbClick(5.0); show();
            std::cout << "no click for 3s: timeout kills the chain\n";
            tr.update(8.0);
            std::cout << "  active: " << (tr.active() ? "yes" : "no")
                      << "\n";
            return true;
        }

        if (cmd == "chars") {
            std::cout << g.charReg.count()
                      << " characters registered:\n";
            for (const auto& id : g.charReg.list()) {
                const CharacterDef* d = g.charReg.get(id);
                std::cout << "  '" << id << "' — " << d->displayName
                          << "\n      hp=" << d->maxHp
                          << " speed=" << d->moveSpeed
                          << " stamina=" << d->maxStamina
                          << " combo='" << d->meleeComboId << "'"
                          << " rmb="
                          << (d->rmbAbilityId.empty()
                                  ? heavyAttackKindName(d->rightClick.kind)
                                  : d->rmbAbilityId)
                          << " (" << d->rightClick.name << ")"
                          << " Q/F/R=" << d->qAbility.name << "/"
                          << d->fAbility.name << "/" << d->rAbility.name
                          << "\n";
            }
            for (const auto& e : g.charLoader.loadErrors())
                std::cout << "  load error: " << e << "\n";
            return true;
        }

        if (cmd == "addchar") {
            std::string folder; in >> folder;
            if (folder.empty()) {
                std::cout << "usage: addchar <folder>\n";
                return true;
            }
            if (g.charLoader.hotLoad(folder)) {
                std::string name = folder;
                const size_t slash = name.find_last_of("/\\");
                if (slash != std::string::npos) name = name.substr(slash + 1);
                const CharacterPackage* p = g.charLoader.find(name);
                std::cout << "hot-loaded package '" << name << "'";
                if (p) {
                    std::cout << " -> registered '" << p->def.id << "' ("
                              << p->def.displayName << ")";
                    ValidationReport rep =
                        CharacterValidator::validate(*p);
                    if (!rep.warnings.empty())
                        std::cout << " [" << rep.warnings.size()
                                  << " warning(s)]";
                }
                std::cout << "\n";
            } else {
                std::cout << "hot-load failed: " << folder << "\n";
                for (const auto& e : g.charLoader.loadErrors())
                    std::cout << "  " << e << "\n";
            }
            return true;
        }

        if (cmd == "validate") {
            std::string id; in >> id;
            if (id.empty()) {
                std::cout << "usage: validate <character-id>\n";
                return true;
            }
            const CharacterPackage* found = nullptr;
            for (const auto& p : g.charLoader.packages())
                if (p.def.id == id || p.folderName == id) {
                    found = &p;
                    break;
                }
            if (!found) {
                std::cout << "no package '" << id << "'\n";
                return true;
            }
            std::cout << "package '" << found->folderName << "':\n"
                      << CharacterValidator::validate(*found).summary();
            return true;
        }

        if (cmd == "kda") {
            PlayerStatsTracker st;
            st.setPlayerName(0, "Cthulhu");
            st.setPlayerName(1, "Rival");
            st.setPlayerName(2, "Acolyte");
            // Cthulhu and Acolyte both damage the rival's avatar
            // (entity 9001); Cthulhu lands the kill -> Acolyte assists.
            st.recordDamage(2, 9001, 30.0f, 10.0);
            st.recordDamage(0, 9001, 80.0f, 12.0);
            st.recordKill(0, 1, 9001, 13.0);
            // The rival scores an environmental kill: kill/death only.
            st.recordKill(1, 0);
            std::cout << "  player\tK\tD\tA\n";
            for (const auto& r : st.rows())
                std::cout << "  " << r.name << "\t" << r.kills << "\t"
                          << r.deaths << "\t" << r.assists << "\n";
            std::cout << "(Acolyte assists: damaged entity 9001 within "
                         "the 10s assist window; the killer never "
                         "self-assists)\n";
            return true;
        }

        if (cmd == "interact") {
            InputManager im;
            InputState s;
            s.e.held = true;
            s.e.pressed = true;  // latch E for one frame
            im.update(s, 1.0f / 60.0f);
            Vec3 ap = g.avatar.position();
            std::vector<Interactable> cands = {
                {501, Interactable::Kind::Altar, "E: perform ritual",
                 Vec3{ap.x + 2.0f, 0, ap.z}},
                {502, Interactable::Kind::Relic, "E: claim relic",
                 Vec3{ap.x - 10.0f, 0, ap.z}},
            };
            const Interactable* t = im.findInteractable(ap, cands);
            if (t) {
                im.setInteractTarget(t->entityId);
                std::cout << "E pressed -> target entity " << t->entityId
                          << " (" << t->prompt << "); lastInteractTarget="
                          << im.lastInteractTarget() << "\n";
            } else {
                std::cout << "E pressed -> nothing within "
                          << InputManager::INTERACT_RADIUS << "m\n";
            }
            return true;
        }

        if (cmd == "jump") {
            const float dt = 1.0f / 60.0f;
            std::cout << "jump buffer: Space pressed 0.1s before landing\n";
            {
                InputManager im;
                InputState s;
                im.setGrounded(false);          // airborne...
                s.space.held = true;
                s.space.pressed = true;
                im.update(s, dt);               // ...buffer the press
                s.space.held = false;
                s.space.pressed = false;
                im.update(s, 0.1f);             // 0.1s pass (within 0.15s)
                im.setGrounded(true);         // land
                im.update(s, dt);
                std::cout << "  jumpPressed on landing: "
                          << (im.jumpPressed() ? "yes (fires, buffer "
                                                 "consumed)"
                                               : "no")
                          << "\n";
            }
            std::cout << "coyote time: Space 0.05s after leaving ground\n";
            {
                InputManager im;
                InputState s;
                im.setGrounded(true);
                im.update(s, dt);
                im.setGrounded(false);          // walk off a ledge
                im.update(s, 0.05f);
                s.space.held = true;
                s.space.pressed = true;
                im.update(s, dt);
                std::cout << "  jumpPressed: "
                          << (im.jumpPressed() ? "yes" : "no") << "\n";
            }
            std::cout << "coyote expired: Space 0.5s after leaving ground\n";
            {
                InputManager im;
                InputState s;
                im.setGrounded(true);
                im.update(s, dt);
                im.setGrounded(false);
                im.update(s, 0.5f);             // past the 0.12s window
                s.space.held = true;
                s.space.pressed = true;
                im.update(s, dt);
                std::cout << "  jumpPressed: "
                          << (im.jumpPressed() ? "yes" : "no") << "\n";
            }
            return true;
        }

        if (cmd == "sprint") {
            std::string arg; in >> arg;
            if (arg != "on" && arg != "off") {
                std::cout << "usage: sprint <on|off>\n";
                return true;
            }
            InputManager im;
            InputState s;
            const bool want = (arg == "on");
            if (want) {
                s.shift.held = true;
                s.shift.pressed = true;
            }
            std::cout << "Shift " << arg
                      << ": 1s ticks (drain 18/s, regen 12/s, "
                         "recover at 30)\n";
            for (int i = 0; i < 8; ++i) {
                s.shift.pressed = false;
                im.update(s, 1.0f);
                std::cout << "  t=" << (i + 1) << "s stamina="
                          << static_cast<int>(im.stamina().value)
                          << (im.sprintHeld() ? " sprinting" : " walking")
                          << (im.stamina().exhausted ? " EXHAUSTED" : "")
                          << "\n";
            }
            // Show the regen side: sprinting first exhausts, then resting
            // recovers above the 30-point threshold.
            if (want) {
                s.shift.held = false;
                std::cout << "(shift released — regen)\n";
                for (int i = 0; i < 4; ++i) {
                    im.update(s, 1.0f);
                    std::cout << "  t=+" << (i + 1) << "s stamina="
                              << static_cast<int>(im.stamina().value)
                              << (im.stamina().exhausted ? " EXHAUSTED"
                                                          : " recovered")
                              << "\n";
                }
            }
            return true;
        }

        if (cmd == "rmb") {
            std::string sub; in >> sub;
            fillRmbCtx(g);
            if (sub == "press") {
                // Make sure the wave has something to catch: the wave
                // travels forward along the avatar's facing, 25m.
                Vec3 ap = g.avatar.position();
                const float yaw = g.avatar.facingYaw();
                const Vec3 fwd{std::cos(yaw), 0.0f, std::sin(yaw)};
                bool any = false;
                for (Entity* e : g.waveCtx.entities) {
                    if (!e || !e->alive()) continue;
                    const Vec3 d{e->position().x - ap.x, 0.0f,
                                 e->position().z - ap.z};
                    const float along = d.x * fwd.x + d.z * fwd.z;
                    const float lat =
                        std::abs(d.x * -fwd.z + d.z * fwd.x);
                    if (along > 0.0f && along <= 25.0f && lat <= 3.0f &&
                        WaveOfDomination::isSusceptible(e)) {
                        any = true;
                        break;
                    }
                }
                if (!any) {
                    Vec3 q{ap.x + fwd.x * 6.0f, 0.0f, ap.z + fwd.z * 6.0f};
                    g.world.push_back(std::make_unique<Civilian>(q));
                    g.waveCtx.entities.push_back(g.world.back().get());
                    std::cout << "(a demo civilian wanders into the "
                                 "wave path)\n";
                }
                if (!g.waveAbility.ready()) {
                    std::cout << "Wave of Domination on cooldown ("
                              << g.waveAbility.cooldownRemaining()
                              << "s left)\n";
                    return true;
                }
                g.waveAbility.onPress(g.waveCtx);
                for (int i = 0;
                     i < 30 &&
                     g.waveAbility.phase() == WaveOfDomination::Phase::Wave;
                     ++i)
                    g.waveAbility.update(g.waveCtx, 0.1);
                std::cout << "RMB pressed (wave cast):\n";
                printWaveState(g);
            } else if (sub == "move") {
                float dx = 0.0f, dy = 0.0f;
                in >> dx >> dy;
                if (g.waveAbility.phase() !=
                    WaveOfDomination::Phase::Hold) {
                    std::cout << "nothing held (phase="
                              << wavePhaseName(g.waveAbility.phase())
                              << ") — 'rmb press' first\n";
                    return true;
                }
                g.waveAbility.onMouseMove(g.waveCtx, dx, dy);
                for (int i = 0; i < 5; ++i)
                    g.waveAbility.update(g.waveCtx, 0.1);
                std::cout << "mouse swung the held victims:\n";
                printWaveState(g);
            } else if (sub == "launch") {
                if (g.waveAbility.phase() !=
                    WaveOfDomination::Phase::Hold) {
                    std::cout << "nothing held (phase="
                              << wavePhaseName(g.waveAbility.phase())
                              << ") — 'rmb press' first\n";
                    return true;
                }
                g.waveAbility.onLeftClick(g.waveCtx);
                for (int i = 0; i < 25; ++i)
                    g.waveAbility.update(g.waveCtx, 0.1);
                std::cout << "LMB while held: victims hurled (300 dmg on "
                             "impact):\n";
                printWaveState(g);
            } else if (sub == "release") {
                g.waveAbility.onRelease(g.waveCtx);
                for (int i = 0; i < 10; ++i)
                    g.waveAbility.update(g.waveCtx, 0.1);
                std::cout << "RMB released: victims dropped (gentle -> 1s "
                             "stagger; >8m drop -> lethal):\n";
                printWaveState(g);
            } else if (sub == "stun") {
                uint64_t id = 0;
                in >> id;
                if (id == 0) {
                    Entity* t = g.nearestTarget(g.avatar.position(),
                                                25.0f);
                    if (t) id = t->id();
                }
                if (id == 0) {
                    std::cout << "no target — usage: rmb stun [entity-id]\n";
                    return true;
                }
                if (!g.waveAbility.ready()) {
                    std::cout << "Wave of Domination on cooldown ("
                              << g.waveAbility.cooldownRemaining()
                              << "s left)\n";
                    return true;
                }
                g.waveCtx.targetedEntityId = id;
                g.waveAbility.onPress(g.waveCtx);
                std::cout << "RMB with cursor target " << id
                          << ": direct stun (2.5s) — stunned="
                          << (g.fx.isStunned(id) ? "yes" : "no")
                          << ", cooldown="
                          << g.waveAbility.cooldownRemaining() << "s\n";
            } else {
                std::cout << "usage: rmb "
                             "<press|move <dx> <dy>|launch|release|stun "
                             "[id]>\n";
            }
            return true;
        }
        // ---- end Wave 7 ----

        std::cout << "unknown command. Type 'help'.\n";
    return true;
}

int main() {
    BetaGame g;
    g.setupWorld();
    NetSession nets;

    std::cout << "CULT-ULHU playable beta — free roam\n"
              << "Your eldritch avatar stalks the map. Type 'help'.\n";

    // Wave 7: auto-load character packages. The binary is usually run as
    // ./build/cultulhu_play from the repo root (assets/characters), but
    // also works from the build dir (../assets/characters).
    {
        std::string root = "assets/characters";
        {
            std::ifstream probe(root + "/cthulhu_avatar/character.def");
            if (!probe) root = "../assets/characters";
        }
        const int n = g.charLoader.scanAndLoad(root);
        std::cout << "characters: " << n << " package(s) from " << root
                  << "\n";
        for (const auto& p : g.charLoader.packages()) {
            ValidationReport rep = CharacterValidator::validate(p);
            std::cout << "  " << p.folderName << " -> '" << p.def.id
                      << "': " << (rep.ok ? "OK" : "INVALID");
            if (!rep.warnings.empty())
                std::cout << " (" << rep.warnings.size() << " warning(s))";
            std::cout << "\n";
            for (const auto& w : rep.warnings)
                std::cout << "      ! " << w << "\n";
        }
        for (const auto& e : g.charLoader.loadErrors())
            std::cout << "  load error: " << e << "\n";
    }

    g.printStatus();

    std::string line;
    while (std::cout << "\n> " && std::getline(std::cin, line)) {
        if (!processLine(g, nets, line)) break;
    }

    std::cout << "The dream ends. Power: " << g.power.value() << "\n";
    return 0;
}
