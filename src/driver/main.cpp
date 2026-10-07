// CULT-ULHU playable beta driver: an interactive console REPL so the game is
// playable headless right now. The engine binding (Unreal/Unity) will replace
// this with real rendering later; all game logic lives in the core library.

#include "ai/AmbientBehavior.h"
#include "animation/AnimationStateMachine.h"
#include "assets/AssetManager.h"
#include "beliefs/BeliefSystem.h"
#include "camera/CameraSystem.h"
#include "chaos/LunaticSystem.h"
#include "combat/Attacks.h"
#include "combat/CrowdControl.h"
#include "commands/CommandSystem.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "cult/CultManager.h"
#include "dreams/DreamSystem.h"
#include "entities/Structures.h"
#include "entities/Units.h"
#include "modes/FreeRoamMode.h"
#include "power/PowerSystem.h"

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
    AmbientDirector ambient{bus, rng, beliefs, cult, 30.0};
    ActiveEffects fx;
    AssetManager assets;
    AnimationStateMachine avatarAnim;

    // Manually spawned world entities (sorcerers, monstrosities, ...).
    std::vector<std::unique_ptr<Entity>> world;

    BetaGame() {
        // Every gameplay event flows through BeliefSystem into Cthulhu's
        // power (the established power pipeline).
        for (int i = 0; i < static_cast<int>(EventType::Count); ++i) {
            bus.subscribe(static_cast<EventType>(i),
                          [this](const GameEvent& e) {
                              power.add(beliefs.onEvent(e));
                          });
        }
        // Narration for the notable moments.
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
        std::cout << "cultists: " << cult.size()
                  << "  resting: " << dreams.restingCount()
                  << "  ambient acts: " << ambient.actionsPerformed() << "\n";
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

int main() {
    BetaGame g;
    g.setupWorld();

    std::cout << "CULT-ULHU playable beta — free roam\n"
              << "Your eldritch avatar stalks the map. Type 'help'.\n";
    g.printStatus();

    std::string line;
    while (std::cout << "\n> " && std::getline(std::cin, line)) {
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
            Entity* t = g.nearestTarget(g.avatar.position());
            if (!t) { std::cout << "no target in range\n"; continue; }
            g.avatarAnim.requestState(AnimationState::Attack);
            float dmg = strikeMelee(g.avatar.id(), EntityType::EldritchAvatar,
                                    *t, 40.0f, g.beliefs, g.bus, g.fx,
                                    EventType::CivilianSlain);
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
                                EventType::CivilianSlain);
                std::cout << "fear cast for " << dmg << "\n";
            } else if (spell == "fireball") {
                dmg = castSpell(g.avatar.id(), EntityType::Sorcerer, *t,
                                35.0f, DamageType::Fire, CCType::Stun, 3.0f,
                                g.beliefs, g.bus, g.fx,
                                EventType::CivilianSlain);
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

        std::cout << "unknown command. Type 'help'.\n";
    }

    std::cout << "The dream ends. Power: " << g.power.value() << "\n";
    return 0;
}
