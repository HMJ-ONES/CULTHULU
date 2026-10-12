// CULT-ULHU playable beta driver: an interactive console REPL so the game is
// playable headless right now. The engine binding (Unreal/Unity) will replace
// this with real rendering later; all game logic lives in the core library.

#include "achievements/AchievementSystem.h"
#include "driver/AntiCheat.h"
#include "ai/AmbientBehavior.h"
#include "ai/RitualCaster.h"
#include "animation/AnimationDirector.h"
#include "animation/AnimationStateMachine.h"
#include "assets/AssetManager.h"
#include "assets/GlbAnimExtractor.h"
#include "assets/GlbInfo.h"
#include "assets/ModelCatalog.h"
#include "beliefs/BeliefSystem.h"
#include "camera/CameraSystem.h"
#include "characters/CharacterPackageLoader.h"
#include "characters/CharacterValidator.h"
#include "characters/AbilityFx.h"
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
#include "driver/ParseUtil.h"
#include "beliefs/InteractionMatrix.h"
#include "breeding/BreedingSystem.h"
#include "combat/KitCaster.h"
#include "discovery/DiscoveryCodex.h"
#include "discovery/LandmarkNames.h"
#include "discovery/RelicNames.h"
#include "core/CommandSuggest.h"
#include "entities/Structures.h"
#include "entities/Units.h"
#include "exertion/ExertionSystem.h"
#include "input/InputManager.h"
#include "modes/FreeRoamMode.h"
#include "modes/Match.h"
#include "modes/MatchLogger.h"
#include "modes/RosterUtils.h"
#include "net/Discovery.h"
#include "net/Lobby.h"
#include "net/Netcode.h"
#include "net/PlayerStats.h"
#include "net/RadminNet.h"
#include "net/Socket.h"
#include "power/PowerSystem.h"
#include "save/SaveSystem.h"
#include "ui/CommandMenu.h"
#include "ui/StatsPanel.h"
#include "world/MapLoader.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
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

// Wave 29: killing-blow flavor — text-driver game feel.
const char* killFlavor(RNG& rng) {
    static const char* lines[] = {
        "— unmade.", "— scattered to the winds.", "— undone.",
        "— returned to the dark.", "— utterly unraveled.",
        "— a red mist, then silence.",
    };
    return lines[static_cast<size_t>(
        rng.intRange(0, static_cast<int>(sizeof(lines) / sizeof(lines[0])) -
                     1))];
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

// Micro-dev: typo-tolerant help for the `belief` command's failure paths.
// Suggests the closest valid belief and prints the full valid list so a
// mistyped belief never dead-ends the player with a bare "unknown belief".
void printUnknownBelief(const std::string& bad, bool toReplace) {
    std::string q = bad;
    for (auto& ch : q) ch = static_cast<char>(std::tolower(ch));
    const std::vector<std::string> names = beliefNamesList();
    std::vector<std::string> lower;
    for (const auto& n : names) {
        std::string lc = n;
        for (auto& ch : lc) ch = static_cast<char>(std::tolower(ch));
        lower.push_back(lc);
    }
    std::cout << "unknown belief" << (toReplace ? " to replace" : "")
              << " '" << bad << "'.";
    const std::string sug = suggestCommand(q, lower);
    if (!sug.empty()) {
        for (size_t i = 0; i < lower.size(); ++i)
            if (lower[i] == sug) {
                std::cout << " Did you mean '" << names[i] << "'?";
                break;
            }
    }
    std::cout << " try: ";
    for (size_t i = 0; i < names.size(); ++i) {
        if (i) std::cout << " | ";
        std::cout << names[i];
    }
    std::cout << "\n";
}

// Wave 5b: entity type ids in a save file are trusted only after this check.
// Wave 23: range extended to EntityType::Rival (newest enumerator; keep last).
bool validEntityType(int t) {
    return t >= static_cast<int>(EntityType::GreatOldOne) &&
           t <= static_cast<int>(EntityType::Rival);
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

// Wave 42 fix: resolve tools/push_match_log.py by walking up from the
// working directory to the repo root. The Windows build guide launches
// the exe from inside build/ (or build/Release), where a CWD-relative
// "tools/..." path resolves wrong and the upload silently never happens.
static std::string findUploaderScript() {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path dir = fs::current_path(ec);
    for (int i = 0; i < 6 && !ec; ++i) {
        const fs::path cand = dir / "tools" / "push_match_log.py";
        if (fs::exists(cand, ec)) return cand.string();
        if (!dir.has_parent_path()) break;
        dir = dir.parent_path();
    }
    return {};
}

// Wave 42 fix: stock Python.org Windows installs put `python` (and `py`)
// on PATH, not `python3`. Probe for a working interpreter instead of
// hardcoding one.
static std::string findPython() {
#ifdef _WIN32
    constexpr const char* nullDev = "NUL";
#else
    constexpr const char* nullDev = "/dev/null";
#endif
    for (const char* py : {"python3", "python"}) {
        const std::string probe =
            std::string(py) + " --version >" + nullDev + " 2>&1";
        if (std::system(probe.c_str()) == 0) return py;
    }
    return {};
}

// Micro-dev: free-roam `status` names the in-flight RMB wave phase
// (defined near the `rmb` demo, below the BetaGame struct).
static const char* wavePhaseName(WaveOfDomination::Phase p);

struct BetaGame {
    EventBus bus;
    GameClock clock;
    RNG rng{1234};
    // Wave 16: achievements (progression). Subscribes to the bus on
    // construction; persists via buildSaveState/applySaveState.
    AchievementSystem achievements{bus};
    PowerSystem power;
    BeliefSystem beliefs{bus, clock};
    CultManager cult{bus, clock, rng};
    DreamSystem dreams{bus, rng, beliefs, cult};
    LunaticSystem lunatics{bus, rng, beliefs, cult};
    FreeRoamMode freeroam{bus, clock, rng};
    // Wave 26: the Discovery Codex — first finds are logged forever and
    // grant power. Knowledge is power, literally.
    DiscoveryCodex codex{bus};
    // Wave 21: active 5v5 match (null when none). Ticked in tickSecond();
    // run it with `match domination|dotgoo` — the free-roam world keeps ticking
    // underneath on the same clock, harmlessly.
    std::unique_ptr<Match> match;
    uint64_t humanMatchId = 0; // local human's match player (0 = not joined)
    // Wave 42: action log recorder — pushed to GitHub at match end.
    MatchLogger matchLogger;
    uint64_t matchSeed = 0; // RNG seed for the log filename
    bool matchLogUploaded = false; // guard: upload once per match
    // Wave 42: character select countdown — when the current match started
    // (steady clock). The select screen shows the 30s pick window.
    std::chrono::steady_clock::time_point matchStartTime{};
    // Wave 42: anti-cheat — macro detection, 3-strike warnings, ban.
    AntiCheat anticheat;
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
    // Wave 29: breeding is playable now — the Breeding belief's core
    // mechanic, wired to exertion skill and christened births.
    BreedingSystem breeding{bus, rng};
    ActiveEffects fx;
    // Beauty pass: the destruction FX catalog. ability_fx.def ships a
    // `raze` preset that fxForEvent() resolves for destruction events,
    // so the headless driver announces which collapse visual the
    // engine binding should play.
    FxLibrary fxLib;
    AssetManager assets;
    AnimationStateMachine avatarAnim;
    // Wave 18: bridges gameplay events to per-entity animation state
    // (brawls, sacrifices, mauls). Entities are tracked as they spawn;
    // tickSecond() advances all tracked machines.
    AnimationDirector animDirector{bus};

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
    // Wave 27: the active kit — `character select <id>` chooses whose
    // Q/F/R the avatar casts. Cooldowns + timed buffs live here.
    std::string activeKitId = "cthulhu_avatar";
    std::map<std::string, double> kitCooldowns_; // spell id -> ready time
    float kitBuffMult_ = 1.0f;
    double kitBuffExpiry_ = 0.0;

    // Wave 7: RMB state-machine demo (Cthulhu Avatar's Wave of Domination).
    // The ability is persistent; the context is re-pointed at the current
    // world for each `rmb` subcommand.
    WaveOfDomination waveAbility;
    RmbContext waveCtx{bus, fx, beliefs, rng};

    BetaGame() {
        // Beauty pass: load the FX catalog early so event narration can
        // name the destruction preset. Works whether the driver runs
        // from the repo root or from build/.
        {
            std::ifstream probe("assets/fx/ability_fx.def");
            const std::string prefix = probe ? "" : "../";
            const FxLoadResult fxRes =
                loadFxLibrary(prefix + "assets/fx/ability_fx.def");
            if (fxRes.ok) fxLib = fxRes.library;
        }
        // Wave 18: the avatar's anim machine is tracked so event hooks
        // (brawl/sacrifice/maul) can pose it, and its machine ticks with
        // every other tracked entity in tickSecond().
        animDirector.track(avatar);
        // The exertion system now owns the power pipeline (event -> belief
        // power rules -> synergy multipliers -> PowerSystem). Narration for
        // the notable moments.
        dreams.setExertion(&exertion);
        ambient.setExertion(&exertion); // wave 9c: Dreams/Reconstruction
                                       // synergies for omen-reading/tending
        bus.subscribe(EventType::Revolt, [](const GameEvent& e) {
            std::cout << "\n!! THE CULT REVOLTS !!";
            if (e.amount >= 1.0f)
                std::cout << " " << static_cast<int>(e.amount)
                          << " cultist(s) desert";
            std::cout << "\n";
        });
        bus.subscribe(EventType::Nightmare, [](const GameEvent& e) {
            std::cout << "[nightmare] cultist " << e.sourceId
                      << " woke up Lunatic\n";
        });
        bus.subscribe(EventType::DirectiveResolved, [](const GameEvent& e) {
            std::cout << "[directive] " << e.tag
                      << " (obedience was " << e.amount << ")\n";
        });
        // Wave 42: kill streak announcements — juice for the bloodshed.
        bus.subscribe(EventType::KillStreak, [](const GameEvent& e) {
            const int s = static_cast<int>(e.amount);
            const char* label = (s == 2)   ? "DOUBLE KILL"
                                : (s == 3) ? "TRIPLE KILL"
                                : (s == 4) ? "QUAD KILL"
                                           : "RAMPAGE";
            std::cout << "*** " << e.tag << " — " << label << " (" << s
                      << ") ***\n";
        });
        // 11:30 AM convenience pass: AbilityUnlocked (DOTGOO draft picks,
        // the level-6 ult) was published and logged but never printed, so
        // a human not staring at `match status` missed their earned pick.
        bus.subscribe(EventType::AbilityUnlocked, [this](const GameEvent& e) {
            const Match::Player* p =
                match ? match->findPlayer(e.sourceId) : nullptr;
            std::string who = p ? (p->name + " (team " +
                                   std::to_string(p->team) + ")")
                                : ("player " + std::to_string(e.sourceId));
            std::cout << "[unlock] " << who << " unlocked " << e.tag << "\n";
        });
        // 2 PM build (2026-10-12): levels were completely silent — the
        // tiered-invocation gates at 10/20/30 only work if the player
        // notices crossing them. tag carries the player name.
        bus.subscribe(EventType::PlayerLeveledUp, [](const GameEvent& e) {
            const int lv = static_cast<int>(e.amount);
            std::cout << "[level] " << e.tag << " reached level " << lv;
            if (lv == 10 || lv == 20 || lv == 30)
                std::cout << " — invocation tier "
                          << (lv / 10) << " unlocked";
            std::cout << "\n";
        });
        bus.subscribe(EventType::DirectiveProgress, [](const GameEvent& e) {
            std::cout << "[directive] " << e.tag << " "
                      << static_cast<int>(e.amount * 100.0f + 0.5f)
                      << "%\n";
        });
        bus.subscribe(EventType::DirectiveCompleted, [](const GameEvent& e) {
            std::cout << "[directive] " << e.tag << " completed\n";
        });
        // Wave 27: the world narrates itself — conversions, losses, and
        // rising dread are no longer silent between ticks.
        bus.subscribe(EventType::ConversionPerformed, [](const GameEvent& e) {
            if (e.amount >= 1.0f)
                std::cout << "[cult] " << static_cast<int>(e.amount)
                          << " soul(s) kneel before the dream\n";
        });
        bus.subscribe(EventType::CultistLost, [](const GameEvent& e) {
            std::cout << "[cult] a cultist is lost to the dark ("
                      << e.tag << ")\n";
            (void)e;
        });
        bus.subscribe(EventType::InsurrectionRiskUp, [](const GameEvent& e) {
            if (e.amount >= 5.0f)
                std::cout << "[dread] insurrection risk stirs (+"
                          << static_cast<int>(e.amount) << ")\n";
        });
        // Wave 29: births are announced — named, or feral and rampaging.
        bus.subscribe(EventType::MonstrosityBred, [](const GameEvent& e) {
            std::cout << "[breeding] " << (e.tag.empty() ? "a monstrosity"
                                                        : e.tag)
                      << " is born\n";
        });
        bus.subscribe(EventType::FeralRampage, [](const GameEvent& e) {
            std::cout << "[breeding] IT IS FERAL — it turns on the cult!\n";
            (void)e;
        });
        // Wave 29: lunacy is visible fuel — every act feeds Chaos exertion,
        // so the player sees madness as a weapon, not just a tax.
        bus.subscribe(EventType::LunaticActed, [](const GameEvent& e) {
            std::cout << "[chaos] a lunatic " << e.tag
                      << " — madness feeds the storm (+Chaos exertion)\n";
        });
        bus.subscribe(EventType::DreamWhisper, [](const GameEvent& e) {
            std::cout << "[dream] a distant civilian stirs in their sleep "
                         "(cultist " << e.sourceId << " dreaming)\n";
        });
        bus.subscribe(EventType::DistrictRazed, [this](const GameEvent& e) {
            std::cout << "[city] a district lies in ruins";
            const FxPreset* p = fxForEvent(EventType::DistrictRazed, fxLib);
            if (p)
                std::cout << " — fx: " << p->id << " (" << p->displayName
                          << ")";
            std::cout << "\n";
            (void)e;
        });
        // Wave 26: the Discovery Codex — first finds are logged forever.
        // Wave 31: discoveries grant no power and can't be renamed; the
        // game invents R'lyehian names for its wonders.
        bus.subscribe(EventType::DiscoveryMade, [this](const GameEvent& e) {
            const Discovery* d = codex.find(e.tag);
            const std::string name = d ? d->name : e.tag;
            const std::string flavor = d ? d->flavor : "";
            const bool night = e.faction == 1;
            std::cout << "\n  ✦ DISCOVERY — " << name << "\n";
            if (!flavor.empty()) std::cout << "    \"" << flavor << "\"\n";
            if (night) std::cout << "    (found under starlight)\n";
            std::cout << "\n";
        });
        // Afternoon build (2026-10-10): the two silent events get their
        // voice. Achievement narration moved here out of the sim so every
        // announcement lives in one place; the tier-3 relic's "global
        // announcement" (wave 34 design) finally prints.
        bus.subscribe(EventType::AchievementUnlocked, [this](const GameEvent& e) {
            const AchievementDef* def = nullptr;
            for (const auto& d : achievements.defs())
                if (d.id == e.tag) { def = &d; break; }
            std::cout << "* The dark takes note: " << (def ? def->name : e.tag);
            if (def && !def->description.empty())
                std::cout << " — " << def->description;
            std::cout << " *\n";
        });
        bus.subscribe(EventType::RelicManifested, [](const GameEvent& e) {
            std::cout << "\n!! A MYTHIC RELIC MANIFESTS — " << e.tag
                      << " (tier " << static_cast<int>(e.amount) << ") !!\n\n";
        });
    }

    void setupWorld() {
        freeroam.setMapBounds(Vec3{-500, 0, -500}, Vec3{500, 0, 500});
        freeroam.addCivilianSpawn(Vec3{60, 0, 40});
        freeroam.addCivilianSpawn(Vec3{-80, 0, 120});
        freeroam.addCivilianSpawn(Vec3{150, 0, -60});
        freeroam.addCreatureSpawn(Vec3{-120, 0, -90}, "ghoul");
        freeroam.addCreatureSpawn(Vec3{200, 0, 80}, "deep one");
        freeroam.addRelicSpawn(Vec3{90, 0, -40}, 0.25f,
                               generateRelicName(rng));
        freeroam.addRelicSpawn(Vec3{-150, 0, 60}, 0.15f,
                               generateRelicName(rng));

        // Wave 26: landmarks — named places worth walking toward. First
        // visits are logged in the Discovery Codex. Wave 31: the game
        // invents R'lyehian names for them (unique per landmark).
        {
            const std::vector<std::pair<Vec3, std::string>> seeds = {
                {Vec3{220, 0, -140}, "Where the old court fell, the stones still kneel."},
                {Vec3{-200, 0, 180}, "It tolls for ships that never sailed home."},
                {Vec3{40, 0, 230}, "The monoliths sweat black water at dusk."},
                {Vec3{-90, 0, -220}, "The wind here knows your name. Do not answer."},
            };
            std::vector<std::string> used;
            for (const auto& s : seeds) {
                std::string name;
                for (int tries = 0; tries < 20; ++tries) {
                    name = generateLandmarkName(rng);
                    bool dup = false;
                    for (const auto& u : used)
                        if (u == name) { dup = true; break; }
                    if (!dup) break;
                }
                used.push_back(name);
                freeroam.addLandmark(name, s.first, 18.0f, s.second);
            }
        }

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

    // Wave 26: poll the world for first-discovery moments — landmarks,
    // new species, and relic claims. The codex dedups; repeats are free.
    void pollDiscoveries() {
        const Vec3 ap = avatar.position();
        const double now = clock.now();
        const bool night = freeroam.isNight();
        for (const auto& lm : freeroam.landmarks()) {
            if (ap.distance(lm.pos) <= lm.radius)
                codex.discover(DiscoveryKind::Landmark, lm.name, lm.name,
                               lm.flavor, lm.pos, now, night);
        }
        for (const auto& cr : freeroam.creatures()) {
            if (!cr->alive()) continue;
            if (ap.distance(cr->position()) <= 30.0f) {
                const std::string& sp = cr->species();
                codex.discover(DiscoveryKind::Species, sp, prettySpecies(sp),
                               speciesFlavor(sp), cr->position(), now, night);
            }
        }
        float amp = 0.0f;
        std::string rname;
        if (freeroam.claimRelicNear(ap, 6.0f, amp, rname)) {
            if (rname.empty()) rname = generateRelicName(rng);
            GameEvent e(EventType::RelicClaimed);
            e.sourceId = avatar.id();
            e.amount = amp;
            e.tag = rname;
            e.pos = ap;
            bus.publish(e);
            codex.discover(DiscoveryKind::Relic, rname, rname,
                           "Claimed by the dreaming god.", ap, now, night);
            const float blessing = 30.0f * (1.0f + amp);
            power.add(blessing);
            std::cout << "[relic] claimed " << rname << " (+" << blessing
                      << " power)\n";
        }
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
        // Wave 32: type() + static_cast instead of dynamic_cast — the
        // per-tick RTTI walk was pure overhead (type_ is a plain enum).
        std::vector<Sorcerer*> sorcs;
        std::vector<Civilian*> civs;
        for (const auto& e : world) {
            if (e->type() == EntityType::Sorcerer)
                sorcs.push_back(static_cast<Sorcerer*>(e.get()));
            else if (e->type() == EntityType::Civilian)
                civs.push_back(static_cast<Civilian*>(e.get()));
        }
        for (const auto& c : freeroam.civilians()) civs.push_back(c.get());
        rituals.setSorcerers(sorcs);
        rituals.setCivilians(civs);
        rituals.update(1.0);
        // Wave 29: breeding follows the belief; skill follows exertion.
        breeding.setBreedingActive(beliefs.isActive(Belief::Breeding));
        breeding.setBreedingSkill(exertion.exertion(Belief::Breeding) /
                                  100.0f);
        // Wave 5b: directive follow-through operations tick here; their
        // events feed the exertion/power pipeline like any other events.
        executor.update(1.0);
        freeroam.setAvatar(&avatar); // civilian flee / rival-bot targeting
        freeroam.update(1.0);
        pollDiscoveries(); // wave 26: landmarks, species, relic claims
        fx.tick(1.0);
        avatarAnim.update(1.0);
        // Micro-dev: the RMB ability (Wave of Domination) ticks with game
        // time too. Its 7 s cooldown used to decay only inside the `rmb`
        // demo's inline updates, so it froze between demo casts while the
        // rest of the world moved on. The free-roam `status` line shows the
        // rmb cooldown, which now stays truthful.
        waveAbility.update(waveCtx, 1.0);
        animDirector.tick(1.0); // wave 18: advance tracked entity anims
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
        // Wave 21: advance the active 5v5 match (bots, combat, mode rules).
        // Wave 42: the action logger polls each tick; when the match ends
        // the log is pushed to GitHub (never stored locally).
        if (match) {
            match->update(1.0);
            matchLogger.poll(*match);
            if (match->isOver() && !matchLogUploaded) {
                matchLogUploaded = true;
                // Wave 42: AFK rewards — no rewards if AFK the whole match;
                // partial AFK (e.g. 5 min away, came back) still earns.
                if (humanMatchId != 0) {
                    double frac = match->afkFraction(humanMatchId);
                    if (frac >= 0.95) {
                        std::cout << "[rewards] You were AFK for the whole "
                                     "match — no rewards earned. No ban; "
                                     "the bot pilot thanks you.\n";
                    } else if (frac > 0.0) {
                        std::cout << "[rewards] You were away for "
                                  << static_cast<int>(frac * 100)
                                  << "% of the match — rewards earned.\n";
                    }
                }
                uploadMatchLog();
            }
        }
    }

    // Wave 42: render the action log and push it to GitHub match-logs/.
    // The log lives only on GitHub (30-day retention); nothing is written
    // to the player's PC. Without a token, the log prints to the console.
    void uploadMatchLog() {
        if (!match) return;
        const std::string log = matchLogger.render(*match);
        const std::string mode = match->modeName() == "capture" ? "domination"
                                 : match->modeName() == "moba"  ? "dotgoo"
                                                                : match->modeName();
        const std::string path = MatchLogger::logPath(mode, matchSeed);
        const char* token = std::getenv("CULTHULU_GITHUB_TOKEN");
        if (!token || !*token) {
            std::cout << "match over — set CULTHULU_GITHUB_TOKEN to upload "
                         "the action log to GitHub.\n";
            return;
        }
        // Wave 42 fix: the uploader script used to be resolved CWD-relative
        // ("tools/push_match_log.py"), which breaks when the exe is
        // launched from inside build/ (the Windows guide's Part 4 does
        // exactly that). Walk up to the repo root instead.
        const std::string script = findUploaderScript();
        if (script.empty()) {
            std::cout << "match log upload skipped (tools/push_match_log.py "
                         "not found above the working directory).\n";
            return;
        }
        // Wave 42 fix: stock Python.org Windows installs ship `python`,
        // not `python3` — probe instead of hardcoding the interpreter.
        const std::string py = findPython();
        if (py.empty()) {
            std::cout << "match log upload skipped (no python3/python on "
                         "PATH).\n";
            return;
        }
        // Pipe the log to the uploader (avoids writing it to disk).
        std::string cmd = py + " \"" + script + "\" \"" + path + "\" 2>&1";
        FILE* pipe = popen(cmd.c_str(), "w");
        if (!pipe) {
            std::cout << "match log upload failed (could not launch uploader).\n";
            return;
        }
        std::fwrite(log.data(), 1, log.size(), pipe);
        const int rc = pclose(pipe);
        if (rc == 0)
            std::cout << "match log uploaded to GitHub: " << path << "\n";
        else
            std::cout << "match log upload failed (see above).\n";
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
        for (const auto& b : freeroam.bots()) {
            if (!b->alive()) continue;
            const float d = dist(from, b->position());
            if (d < bestD) { bestD = d; best = b.get(); }
        }
        for (const auto& e : world) {
            if (!e->alive() || e->faction() == FACTION_CTHULHU) continue;
            const float d = dist(from, e->position());
            if (d < bestD) { bestD = d; best = e.get(); }
        }
        return best;
    }

    // Camera focus: the local human's match player while in a match,
    // otherwise the free-roam avatar. Works in every mode, FP or TP —
    // switching mid-match never breaks because CameraSystem is stateless.
    void cameraFocus(Vec3& pos, float& yaw) const {
        if (match && match->started() && humanMatchId != 0) {
            if (const auto* p = match->findPlayer(humanMatchId)) {
                pos = p->pos;
                yaw = p->facingYaw;
                return;
            }
        }
        pos = avatar.position();
        yaw = avatar.facingYaw();
    }

    void printStatus() {
        Vec3 fpos; float fyaw;
        cameraFocus(fpos, fyaw);
        CameraPose pose = camera.poseFor(fpos, fyaw);
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
        // Wave 28: surface the live synergies/conflicts — emergence the
        // player can see.
        {
            float ex[static_cast<int>(Belief::Count)];
            for (int i = 0; i < static_cast<int>(Belief::Count); ++i)
                ex[i] = exertion.exertion(static_cast<Belief>(i));
            std::string live;
            for (int i = 0; i < synergyPairCount(); ++i) {
                const BeliefPair& p = synergyPairs()[i];
                if (synergyActive(p.a, p.b, ex))
                    live += std::string("  ✦ ") + p.name + " (" +
                            beliefName(p.a) + "×" + beliefName(p.b) + ")";
            }
            for (int i = 0; i < conflictPairCount(); ++i) {
                const BeliefPair& p = conflictPairs()[i];
                if (conflictActive(p.a, p.b, ex))
                    live += std::string("  ⚡ ") + p.name + " (" +
                            beliefName(p.a) + "×" + beliefName(p.b) +
                            " clash)";
            }
            if (!live.empty()) std::cout << "\nresonance:" << live;
        }
        std::cout << "\n  combatPower x" << exertion.stats().combatPowerMult
                  << "  loyaltyDrift " << exertion.stats().loyaltyDriftPerSec
                  << "/s  convertChance "
                  << exertion.stats().conversionChance << "\n";
        std::cout << "cultists: " << cult.size()
                  << "  resting: " << dreams.restingCount()
                  << " (dreaming: " << dreams.restingCount()
                  << " -> +"
                  << dreams.restingCount() * dreams.powerPerSec()
                  << " power/s)"
                  << "  ambient acts: " << ambient.actionsPerformed() << "\n";
        std::cout << "directive ops: " << executor.activeCount() << " active"
                  << (executor.defenseActive() ? " (DEFENDING)" : "") << "\n";
        // At-a-glance kit cooldowns for the active character (q/f/r) —
        // no need to burn a cast to learn something is on cooldown.
        std::cout << "kit: ";
        {
            const CharacterDef* kit = charReg.get(activeKitId);
            if (!kit) {
                std::cout << "none";
            } else {
                const double now = clock.now();
                const char* keys[3] = {"q", "f", "r"};
                const SpellDef* spells[3] = {&kit->qAbility,
                                             &kit->fAbility, &kit->rAbility};
                for (int i = 0; i < 3; ++i) {
                    if (i > 0) std::cout << "  ";
                    const SpellDef* sp = spells[i];
                    if (sp->id.empty()) {
                        std::cout << keys[i] << " -";
                        continue;
                    }
                    auto cdIt = kitCooldowns_.find(sp->id);
                    if (cdIt == kitCooldowns_.end() ||
                        cdIt->second <= now) {
                        std::cout << keys[i] << " READY";
                    } else {
                        std::cout << keys[i] << " "
                                  << static_cast<int>(cdIt->second - now + 0.5)
                                  << "s";
                    }
                }
            }
            // Micro-dev: RMB (Wave of Domination) is the fourth button —
            // it was invisible here, so you had to burn a cast to learn it
            // was on cooldown. (Its cooldown now ticks in tickSecond, so
            // this number stays truthful over game time.)
            std::cout << "  rmb ";
            if (waveAbility.ready()) {
                std::cout << "READY";
            } else if (waveAbility.cooldownRemaining() > 0.0f) {
                std::cout << static_cast<int>(
                                  waveAbility.cooldownRemaining() + 0.5)
                          << "s";
            } else {
                // Cooldown spent but the wave is still out (caught, held,
                // or flying victims).
                std::cout << wavePhaseName(waveAbility.phase());
            }
        }
        std::cout << "\n";
        for (size_t i = 0; i < cult.size(); ++i) {
            const Cultist& c = cult.at(i);
            std::cout << "  [" << i << "] id=" << c.id()
                      << " hp=" << c.hp()
                      << " state=" << cultistStateName(c.state())
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
        achievements.saveTo(s); // wave 16
        codex.saveTo(s);          // wave 26
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
        achievements.loadFrom(s); // wave 16
        codex.loadFrom(s);          // wave 26
        cult.clear();
        world.clear();
        // Wave 18: the old entities are gone; drop their anim tracking
        // (the avatar persists, so re-track it).
        animDirector.clear();
        animDirector.track(avatar);
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
                c.setAnimPackDir( // wave 18: package .canim clips
                    "assets/characters/cultist_hooded/animations");
                bindEntityClips(c, "assets");
                animDirector.track(c);
                continue;
            }
            auto e = entityFromRecord(r);
            if (e) {
                // Wave 18: restore the package link by type (saved games
                // don't persist it) so .canim clips rebind on load.
                if (e->type() == EntityType::Civilian)
                    e->setAnimPackDir(
                        "assets/characters/civilian_villager/animations");
                bindEntityClips(*e, "assets");
                animDirector.track(*e);
                world.push_back(std::move(e));
            }
        }
    }

    void printHelp() {
        std::cout <<
            "FIRST STEPS — the dreaming god wakes:\n"
            "  1. look                  see what surrounds you\n"
            "  2. move e 10            walk toward the nearest wonder\n"
            "  3. character select <id> choose whose body you wear (see 'chars')\n"
            "  4. cast q               unleash your first ability\n"
            "  5. command raid         send the cult to raid a city\n"
            "  6. codex               browse everything you have discovered\n"
            "\n"
            "commands:\n"
            "  move <n|s|e|w|ne|nw|se|sw> [steps]  walk the avatar\n"
            "  camera <fp|tp|switch>               switch camera (any mode, anytime)\n"
            "  look                                survey surroundings\n"
            "  cast <q|f|r>                        cast the active kit's ability\n"
            "  cast <fireball|fear>                legacy sorcerer spells\n"
            "  character <list|validate|select> [id]  kits & who you walk as\n"
            "  spawn <cultist|civilian|monstrosity|sorcerer|bot> [n]\n"
            "  spawn <monstrosity|creature> [species] [n]  (see 'bestiary')\n"
            "  belief <name> [replace <old>]       adopt a belief\n"
            "  beliefs                             list active beliefs\n"
            "  rest <i>                            toggle rest for cultist i\n"
            "  rest <all|wake>                     rest/wake the whole cult\n"
            "  breed <sp> <sp> [name]              breed a monstrosity (Breeding)\n"
            "  ritual <target|clear>               aim the next sorcerer ritual\n"
            "  command <raid|war|convert|sacrifice|defend|relic|dream|rebuild>\n"
            "  attack                              melee the nearest target\n"
            "  tick <n>                            advance n game-seconds\n"
            "  save <file>                         save game to file\n"
            "  load <file>                         load game from file\n"
            "  myip                                show adapters (Radmin IP)\n"
            "  discover [secs]                     find hosts on Radmin LAN\n"
            "  host <port> [name]                  host a lobby\n"
            "  join <ip> <port> <name>              join a lobby\n"
            "  ready | players | startgame [force] [domination|dotgoo|freeroam]\n"
            "  chat <msg> | netent | leave\n"
            "  match <domination|dotgoo|join [team]|addbot <team>|status|end>\n"
            "    (domination = 5v5 capture point, dotgoo = Defense of the Great Old Ones)\n"
            "  select                              pick your fighter by clicking\n"
            "                                      (opens tools/match_select.html)\n"
            "  mcast <q|f|r|rmb> [yaw-degrees]  cast your kit ability (after match join)\n"
            "  mpick <q|f>                     draft an ability (DOTGOO level-up picks)\n"
            "                                          run a local 5v5 match\n"
            "  build <wall|barracks|watchtower|trap|portal|altar>\n"
            "                                          start construction\n"
            "  menu [cultist|location|enemy|altar]  radial command menu demo\n"
            "  mapinfo                             map art: placements, file\n"
            "                                          check, asset budget\n"
            "  bestiary                            creature/NPC models\n"
            "  ambient [action]                    trigger cultist ambient\n"
            "                                          action (see 'ambient' help)\n"
            "  dungeon [seed] | dungeon pnath [seed]\n"
            "                                          dungeon map / Vale of Pnath\n"
            "  combo                               melee combo chain demo\n"
            "  chars | addchar <folder> | validate <id>\n"
            "                                          character packages\n"
            "  character <list|validate> [id]      deep package report\n"
            "  achievements [setplayer <id> [faction] [team]]\n"
            "                                          deeds & unlocks\n"
            "  codex                               discovery log\n"
            "  kda                                 demo KDA tracking\n"
            "  interact                            E-interact demo\n"
            "  jump | sprint <on|off>              input state demos\n"
            "  rmb <press|move|launch|release|stun> Wave of Domination demo\n"
            "  status                              dump game state\n"
            "  help | quit\n";
    }
};

// Wave 27: one-line arrival survey after every `move` (defined near main).
void printArrivalSurvey(BetaGame& g);

// Wave 9c driver commands (`ambient`, `dungeon`): header-inline module;
// the implementation compiles into this TU via the include below.
#include "driver/wave9_content_cmds.h"

// Wave 9b driver commands (`directive assassinate|blight|summon|zones`) + wave 15 (`directive dream|rebuild`):
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
    if (n == "dream") return DirectiveType::OneiricHarvest;
    if (n == "rebuild") return DirectiveType::RebuildSanctum;
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
        // Avatar + up to 32 cultists, built at 20 Hz on the host: reserve
        // once so the 33 push_backs below never reallocate.
        out.reserve(33);
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
// Driver command vocabulary for the "did you mean" typo suggestion.
// Keep in sync with the dispatch in processLine below plus the wave-9
// content commands (ambient, dungeon) and wave-9b/15 directives (directive).
static const std::vector<std::string> kDriverCommands = {
    "achievements", "addchar", "ambient", "attack", "belief", "beliefs",
    "bestiary", "breed", "build", "camera", "cast", "character", "chars",
    "chat", "codex", "combo", "command", "directive", "discover", "dungeon",
    "exit", "help", "host", "interact", "join", "jump", "kda", "leave",
    "load", "look", "mapinfo", "match", "mcast", "menu", "move", "mpick",
    "myip", "netent", "players", "quit", "ready", "rest", "ritual", "rmb",
    "save", "spawn", "sprint", "startgame", "status", "tick", "validate",
};

static bool processLine(BetaGame& g, NetSession& nets,
                        const std::string& line) {
        nets.poll(g);  // pump multiplayer (non-blocking)
        std::istringstream in(line);
        std::string cmd;
        in >> cmd;
        if (cmd.empty()) return true;

        // Wave 42: anti-cheat — macro/automation detection. Exempt the
        // status command itself and harmless meta commands.
        if (cmd != "anticheat" && cmd != "help" && cmd != "quit" &&
            cmd != "exit") {
            if (!g.anticheat.recordCommand(line)) {
                bool banned = g.anticheat.issueWarning();
                // Disconnect from the match immediately.
                if (g.match && g.match->started() && g.humanMatchId != 0) {
                    g.humanMatchId = 0;
                    std::cout << "[anticheat] Disconnected from the match.\n";
                }
                if (banned) {
                    std::cout
                        << "[anticheat] 3rd strike — ACCOUNT BANNED.\n"
                        << "  Progression wiped. Multiplayer blocked.\n"
                        << "  You may still self-host bot matches.\n";
                    g.achievements.reset();
                } else {
                    std::cout << "[anticheat] Cheat detected (macro/automation). "
                              << "Warning " << g.anticheat.warnings()
                              << "/3. 3 warnings = ban.\n";
                }
                return true;
            }
        }

        // Wave 42: banned players cannot use multiplayer (host/join).
        // Bot matches (match domination/dotgoo) still work.
        if (g.anticheat.isBanned() &&
            (cmd == "host" || cmd == "join" || cmd == "discover" ||
             cmd == "startgame")) {
            std::cout << "[anticheat] Multiplayer blocked — account banned. "
                         "You may still self-host bot matches.\n";
            return true;
        }

        // Wave 9c content commands (ambient/dungeon); one registration line.
        if (registerWave9ContentCommands(g, cmd, in)) return true;

        // Wave 9b directive commands (directive assassinate|blight|summon|zones) + wave 15 (dream|rebuild);
        // one registration line.
        if (registerWave9DirectiveCommands(g, cmd, in)) return true;

        if (cmd == "quit" || cmd == "exit") return false;
        if (cmd == "help") { g.printHelp(); return true; }
        if (cmd == "status") { g.printStatus(); return true; }
        if (cmd == "anticheat") {
            std::cout << g.anticheat.status() << "\n";
            return true;
        }
        // Wave 42: visual character select — clickable portrait grid.
        // The page talks back to the game over localhost:8471, so
        // clicking JOIN MATCH puts you straight into the match.
        if (cmd == "select") {
            std::cout
                << "open this in your browser, click a fighter, then click\n"
                << "JOIN MATCH — you'll drop straight into the game:\n"
                << "  tools/match_select.html\n"
                << "(start a match first: match domination | match dotgoo)\n";
            return true;
        }
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
            // In a match with a joined human, move the match player —
            // combat is automatic (same as bots), positioning is yours.
            if (g.match && g.match->started() && g.humanMatchId != 0) {
                const auto* hp = g.match->findPlayer(g.humanMatchId);
                if (!hp || !hp->alive) {
                    std::cout << "you are dead (respawning)\n";
                    return true;
                }
                Vec3 dir = d.normalized();
                for (int i = 0; i < steps; ++i) {
                    const auto* cur = g.match->findPlayer(g.humanMatchId);
                    if (!cur || !cur->alive) break;
                    g.match->movePlayer(g.humanMatchId,
                                        cur->pos + dir * Match::kPlayerSpeed);
                    g.match->setFacingYaw(g.humanMatchId,
                                           std::atan2(dir.z, dir.x));
                    g.tickSecond();
                }
                const auto* now = g.match->findPlayer(g.humanMatchId);
                if (now)
                    std::cout << "moved to (" << now->pos.x << ", "
                              << now->pos.z << ")\n";
                return true;
            }
            if (g.match && g.match->started())
                std::cout << "(spectating: join the match first — "
                             "`match join`)\n";
            for (int i = 0; i < steps; ++i) {
                g.avatar.move(d, 1.0, 8.0f);
                g.avatar.setFacingYaw(std::atan2(d.z, d.x));
                g.tickSecond();
            }
            g.avatarAnim.requestState(steps > 3 ? AnimationState::Run
                                                : AnimationState::Walk);
            std::cout << "moved to (" << g.avatar.position().x << ", "
                      << g.avatar.position().z << ")\n";
            // Wave 27: every arrival surveys the surroundings — no more
            // coordinate voids.
            printArrivalSurvey(g);
            return true;
        }

        if (cmd == "camera") {
            std::string m; in >> m;
            if (m == "fp") g.camera.setMode(CameraMode::FirstPerson);
            else if (m == "tp") g.camera.setMode(CameraMode::ThirdPerson);
            else if (m == "switch") g.camera.switchCamera();
            else {
                std::cout << "usage: camera <fp|tp|switch>\n";
                return true;
            }
            // Works in every mode: follows the joined match player in a
            // match, the avatar otherwise. Safe to flip at any time.
            Vec3 fpos; float fyaw;
            g.cameraFocus(fpos, fyaw);
            CameraPose p = g.camera.poseFor(fpos, fyaw);
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
            // spawn <cultist|civilian|sorcerer|bot> [n]
            // spawn <monstrosity|creature> [species] [n]  (species optional;
            //   a bare number is treated as the count, default species
            //   "spawned")
            //
            // Count parsing is overflow-safe (ParseUtil::safeStoi): huge
            // digit strings clamp instead of throwing std::out_of_range.
            using cultulhu::driver::safeStoi;
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
                            n = safeStoi(tok);
                        } else {
                            species = tok;
                            std::string tok2;
                            if (in >> tok2 && isNum(tok2))
                                n = safeStoi(tok2);
                        }
                    } else if (isNum(tok)) {
                        n = safeStoi(tok);
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
                    // Wave 18: clip binding + anim event tracking
                    // (brawls/sacrifices). The hooded-cultist package's
                    // 16 KayKit .canim clips win over embedded/procedural.
                    c.setAnimPackDir(
                        "assets/characters/cultist_hooded/animations");
                    bindEntityClips(c, "assets");
                    g.animDirector.track(c);
                } else if (what == "sorcerer") {
                    auto s = std::make_unique<Sorcerer>(FACTION_CTHULHU, q);
                    bindEntityClips(*s, "assets");
                    g.animDirector.track(*s);
                    g.world.push_back(std::move(s));
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
                    // Wave 18: embedded glTF clips for dagon_spawn /
                    // risen_dead / wraith, procedural fallbacks otherwise.
                    bindEntityClips(*m, "assets");
                    g.animDirector.track(*m);
                    g.world.push_back(std::move(m));
                } else if (what == "civilian") {
                    auto c = std::make_unique<Civilian>(q);
                    // Wave 18: villager package .canim clips (fearrun!)
                    // win over procedural.
                    c->setAnimPackDir(
                        "assets/characters/civilian_villager/animations");
                    bindEntityClips(*c, "assets");
                    g.animDirector.track(*c);
                    g.world.push_back(std::move(c));
                } else if (what == "bot") {
                    // Wave 23: free-roam rival bot — AI hunter that
                    // wanders, engages the avatar/creatures, flees when
                    // hurt. Player-vs-bot and bot-vs-bot in free roam.
                    uint64_t id = g.freeroam.spawnBot(q);
                    if (i == 0)
                        std::cout << "spawned " << n
                                  << " rival bot(s) (id " << id << "+)\n";
                } else {
                    std::cout << "unknown: " << what
                              << " (try: cultist | civilian | sorcerer |"
                                 " monstrosity [species] | bot)\n";
                    return true;
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
                printUnknownBelief(name, false);
                return true;
            }
            Belief out = Belief::Count;
            if (rw == "replace") {
                out = beliefByName(old);
                if (out == Belief::Count) {
                    printUnknownBelief(old, true);
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
            std::string arg; in >> arg;
            // Wave 28: bulk rest — no more per-cultist micromanagement.
            if (arg == "all" || arg == "wake") {
                const bool wake = (arg == "wake");
                size_t n = 0;
                for (size_t i = 0; i < g.cult.size(); ++i) {
                    Cultist& c = g.cult.at(i);
                    if (!c.alive()) continue;
                    if (wake) {
                        if (g.dreams.isResting(c.id())) {
                            g.dreams.endRest(c.id());
                            ++n;
                        }
                    } else if (!g.dreams.isResting(c.id())) {
                        g.dreams.startRest(c.id());
                        ++n;
                    }
                }
                std::cout << n << " cultist(s) " << (wake ? "wake" : "rest")
                          << " (dream-visions)\n";
                return true;
            }
            size_t i = 0;
            try { i = static_cast<size_t>(std::stoul(arg)); }
            catch (...) {
                std::cout << "usage: rest <i|all|wake>\n";
                return true;
            }
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

        // Wave 29: breeding is playable — cross two species, name the birth.
        if (cmd == "breed") {
            auto parseSpecies = [](const std::string& s) {
                if (s == "human") return Species::Human;
                if (s == "deepone" || s == "deep_one" || s == "deep-one")
                    return Species::DeepOne;
                if (s == "ghoul") return Species::Ghoul;
                if (s == "beast") return Species::Beast;
                if (s == "horror") return Species::Horror;
                return Species::Count;
            };
            std::string sa, sb;
            in >> sa >> sb;
            std::string name;
            std::getline(in, name);
            name.erase(0, name.find_first_not_of(" \t"));
            Species a = parseSpecies(sa), b = parseSpecies(sb);
            if (a == Species::Count || b == Species::Count) {
                std::cout << "usage: breed <human|deepone|ghoul|beast|horror> "
                             "<species> [name]\n";
                return true;
            }
            if (!g.breeding.breedingActive()) {
                std::cout << "the Breeding belief sleeps — adopt it first "
                             "(belief breeding)\n";
                return true;
            }
            if (!BreedingSystem::compatible(a, b)) {
                std::cout << "those bloodlines refuse to mingle\n";
                return true;
            }
            Vec3 pos{g.avatar.position().x + 4.0f, 0.0f,
                     g.avatar.position().z};
            auto m = g.breeding.breed(a, b, FACTION_CTHULHU, pos, name);
            if (m) {
                std::cout << (m->feral() ? "feral! " : "")
                          << "feral chance was "
                          << static_cast<int>(
                                 g.breeding.effectiveFeralChance() * 100.0f)
                          << "%\n";
                g.world.push_back(std::move(m));
            }
            g.tickSecond();
            return true;
        }

        // Wave 30: direct your sorcerers' rituals — freedom to aim your magic.
        if (cmd == "ritual") {
            std::string sub; in >> sub;
            if (sub == "target") {
                Entity* t = g.nearestTarget(g.avatar.position(), 200.0f);
                Vec3 p = t ? t->position() : g.avatar.position();
                g.rituals.setDirectedPoint(p);
                std::cout << "the next ritual will seek souls near ("
                          << p.x << ", " << p.z << ")\n";
                return true;
            }
            if (sub == "clear") {
                g.rituals.clearDirectedPoint();
                std::cout << "rituals return to their own whims\n";
                return true;
            }
            std::cout << "rituals: " << g.rituals.ritualsAttempted()
                      << " attempted, " << g.rituals.ritualsSucceeded()
                      << " souls turned"
                      << (g.rituals.hasDirectedPoint() ? " (aimed)"
                                                       : " (unaimed)")
                      << "\n  usage: ritual target | ritual clear\n";
            return true;
        }

        if (cmd == "command") {            std::string what; in >> what;
            DirectiveType d = directiveByName(what);
            if (d == DirectiveType::Count) {
                std::cout << "usage: command "
                             "<raid|war|convert|sacrifice|defend|relic|dream|rebuild>\n";
                return true;
            }
            // Wave 28: show the odds and the why BEFORE the dice land.
            ObediencePreview pv =
                g.commands.previewObedience(d, g.avatar.position());
            std::cout << "the cult will likely "
                      << (pv.chance >= 0.75f ? "obey" :
                          pv.chance >= 0.45f ? "waver" : "refuse")
                      << " (" << static_cast<int>(pv.chance * 100.0f + 0.5f)
                      << "%)";
            if (!pv.reasons.empty()) {
                std::cout << " — ";
                for (size_t i = 0; i < pv.reasons.size(); ++i) {
                    if (i) std::cout << "; ";
                    std::cout << pv.reasons[i];
                }
            }
            std::cout << "\n";
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
                      << ")";
            if (!t->alive()) std::cout << " " << killFlavor(g.rng);
            std::cout << "\n";
            g.tickSecond();
            return true;
        }

        if (cmd == "cast") {
            std::string spell; in >> spell;
            // Wave 27: cast the active kit's abilities — q/f/r resolve
            // through the character registry with real cooldowns.
            if (spell == "q" || spell == "f" || spell == "r") {
                const CharacterDef* kit = g.charReg.get(g.activeKitId);
                if (!kit) {
                    std::cout << "no active kit (character select <id>)\n";
                    return true;
                }
                const SpellDef* sp = spell == "q" ? &kit->qAbility
                                     : spell == "f" ? &kit->fAbility
                                                   : &kit->rAbility;
                if (sp->id.empty()) {
                    std::cout << kit->displayName << " has no "
                              << spell << " ability\n";
                    return true;
                }
                const double now = g.clock.now();
                auto cdIt = g.kitCooldowns_.find(sp->id);
                if (cdIt != g.kitCooldowns_.end() && cdIt->second > now) {
                    std::cout << sp->name << " is not ready ("
                              << static_cast<int>(cdIt->second - now + 0.5)
                              << "s)\n";
                    return true;
                }
                // Timed damage buffs (from buff-kind casts) apply here.
                if (g.kitBuffMult_ != 1.0f && g.kitBuffExpiry_ <= now)
                    g.kitBuffMult_ = 1.0f;
                Entity* t = g.nearestTarget(g.avatar.position());
                std::vector<Entity*> foes;
                for (const auto& b : g.freeroam.bots())
                    if (b->alive()) foes.push_back(b.get());
                for (const auto& c : g.freeroam.creatures())
                    if (c->alive() &&
                        c->faction() != FACTION_CTHULHU)
                        foes.push_back(c.get());
                g.avatarAnim.requestState(AnimationState::Cast);
                KitCastResult r = castKitSpell(
                    *sp, g.avatar, t, foes, g.beliefs, g.bus, g.fx,
                    g.exertion.stats().combatPowerMult * g.kitBuffMult_);
                std::cout << r.message << "\n";
                if (r.ok) {
                    g.kitCooldowns_[sp->id] = now + sp->cooldownSec;
                    if (r.buffSeconds > 0.0f) {
                        g.kitBuffMult_ = r.buffMult;
                        g.kitBuffExpiry_ = now + r.buffSeconds;
                    }
                    for (const auto& s : r.summons) {
                        for (int i = 0; i < s.second; ++i) {
                            auto c = std::make_unique<Creature>(
                                FACTION_CTHULHU,
                                Vec3{g.avatar.position().x +
                                         (i * 2 - s.second),
                                     0.0f,
                                     g.avatar.position().z + 3.0f},
                                s.first, 120.0f);
                            std::cout << "  a " << s.first
                                      << " answers the call\n";
                            g.world.push_back(std::move(c));
                        }
                    }
                    if (r.dash.x != 0.0f || r.dash.z != 0.0f) {
                        g.avatar.setPosition(Vec3{
                            g.avatar.position().x + r.dash.x, 0.0f,
                            g.avatar.position().z + r.dash.z});
                        std::cout << "  moved to ("
                                  << g.avatar.position().x << ", "
                                  << g.avatar.position().z << ")\n";
                    }
                    if (t) {
                        std::cout << "target hp " << t->hp();
                        if (!t->alive())
                            std::cout << " " << killFlavor(g.rng);
                        std::cout << "\n";
                    }
                }
                g.tickSecond();
                return true;
            }
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
                std::cout << "usage: cast <q|f|r> | cast <fireball|fear>\n"
                          << "  q/f/r casts the active kit's ability "
                             "(character select <id>)\n";
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
            bool force = false;
            std::string mode = "freeroam";
            // Wave 21: startgame [force] [domination|dotgoo|freeroam].
            // Old forms ("startgame", "startgame force") keep working.
            if (f == "force") {
                force = true;
                in >> f;
            }
            if (f == "domination") f = "capture";
            if (f == "dotgoo") f = "moba";
            if (f == "capture" || f == "moba" || f == "freeroam") mode = f;
            if (nets.role != NetSession::Role::Hosting || !nets.host) {
                std::cout << "only the host can start the game\n";
                return true;
            }
            nets.host->startGame(force, mode);
            std::cout << "game starting in mode: " << mode << "\n";
            return true;
        }

        // ---- Wave 21: local 5v5 match simulation ----
        if (cmd == "match") {
            std::string sub;
            in >> sub;
            // Wave 39: mode names — Domination (5v5 capture) and DOTGOO
            // (Defense of the Great Old Ones). Old names still work.
            std::string modeArg = sub;
            if (sub == "domination") modeArg = "capture";
            if (sub == "dotgoo") modeArg = "moba";
            if (sub == "capture" || sub == "moba" || sub == "domination" ||
                sub == "dotgoo") {
                g.match = std::make_unique<Match>(g.bus, g.clock, g.rng);
                if (!g.match->start(modeArg)) {
                    g.match.reset();
                    std::cout << "unknown mode\n";
                    return true;
                }
                g.humanMatchId = 0;
                g.match->botfill();
                // Wave 42: start the action log recorder.
                g.matchSeed = static_cast<uint64_t>(std::time(nullptr));
                g.matchLogUploaded = false;
                g.matchLogger.attach(*g.match);
                // Wave 42: character select countdown starts now (30s).
                g.matchStartTime = std::chrono::steady_clock::now();
                std::string displayName = (modeArg == "capture")
                                              ? "Domination"
                                              : "DOTGOO (Defense of the Great Old Ones)";
                std::cout << "5v5 " << displayName << " match started (10 bots)\n";
                g.match->printStatus(std::cout);
            } else if (sub == "join") {
                if (!g.match || !g.match->started()) {
                    std::cout << "no active match (match domination|dotgoo first)\n";
                    return true;
                }
                if (g.humanMatchId != 0) {
                    std::cout << "already joined as a player\n";
                    return true;
                }
                // Wave 42: pick your fighter — `match join [team] <kit-id>`.
                // Optional; without a kit you inherit the bot's kit.
                // Micro-dev: the team is optional too, so a non-numeric
                // first token is the kit id — `match join cthulhu_avatar`
                // picks the fighter on an auto-chosen team instead of
                // silently dropping the kit (old `in >> team` failed and
                // swallowed the argument).
                int team = -1;
                std::string wantKit;
                std::string first;
                in >> first;
                if (!first.empty() &&
                    first.find_first_not_of("-0123456789") ==
                        std::string::npos &&
                    first != "-") {
                    team = std::stoi(first);
                    in >> wantKit;
                } else {
                    wantKit = first;
                }
                uint64_t id = g.match->joinAsHuman(team);
                if (id == 0) {
                    std::cout << "no bot slot to take over on that team\n";
                    return true;
                }
                if (!wantKit.empty()) {
                    if (isNpcPackage(wantKit)) {
                        std::cout << "'" << wantKit
                                  << "' is an NPC, not a playable kit — "
                                     "keeping the bot's kit.\n";
                    } else if (!g.charReg.get(wantKit)) {
                        // Micro-dev: Match::setPlayerKit never rejects an
                        // unknown kit id (it only fails on a bad player
                        // id), so a typo would silently leave the human
                        // kit-less (kitOf -> nullptr). Reject unknown ids
                        // up front and forgive the typo: suggest the
                        // closest playable kit instead. NPC package ids are
                        // excluded from the vocabulary (never fighters).
                        std::cout << "no kit '" << wantKit
                                  << "' — keeping the bot's kit.";
                        std::vector<std::string> playable;
                        for (const auto& kid : g.charReg.list())
                            if (!isNpcPackage(kid))
                                playable.push_back(kid);
                        const std::string sug =
                            suggestCommand(wantKit, playable);
                        if (!sug.empty())
                            std::cout << " Did you mean '" << sug << "'?";
                        std::cout << "\n";
                    } else {
                        g.match->setPlayerKit(id, wantKit);
                    }
                }
                g.humanMatchId = id;
                const auto* p = g.match->findPlayer(id);
                std::cout << "joined team " << p->team
                          << " as \"" << p->name << "\" <" << p->kitId << ">"
                          << " — move with: move <dir> [steps]; "
                             "combat is automatic. camera fp|tp|switch "
                             "works anytime.\n";
                // Micro-dev: name the kit's buttons — a human who just
                // picked (or inherited) a fighter should know what
                // `mcast` does. Matters most for Cthulhu's RMB: a rock
                // with down/launch verbs in Domination, but the
                // mind-control gaze in DOTGOO.
                if (const CharacterDef* kd = g.charReg.get(p->kitId)) {
                    std::cout << "  your kit: Q " << kd->qAbility.name
                              << " | F " << kd->fAbility.name
                              << " | R " << kd->rAbility.name
                              << " | RMB " << kd->rightClick.name << "\n"
                              << "  cast with: mcast <q|f|r|rmb> "
                                 "[yaw-degrees]\n";
                    if (p->kitId == "cthulhu_avatar" &&
                        g.match->modeName() == "capture")
                        std::cout
                            << "  (Domination: your RMB is the rock — "
                               "'mcast rmb down' rips it from the ground, "
                               "'mcast rmb launch' hurls it)\n";
                }
            } else if (sub == "addbot") {
                if (!g.match || !g.match->started()) {
                    std::cout << "no active match (match domination|dotgoo first)\n";
                    return true;
                }
                int team = -1;
                in >> team;
                uint64_t id = g.match->addBot(team);
                if (id == 0) std::cout << "roster full (10)\n";
                else {
                    const auto* p = g.match->findPlayer(id);
                    std::cout << "added " << p->name << " to team "
                              << p->team << "\n";
                }
            } else if (sub == "status") {
                if (!g.match || !g.match->started()) {
                    std::cout << "no active match\n";
                    return true;
                }
                g.match->printStatus(std::cout);
            } else if (sub == "end") {
                // Wave 42: upload the action log even for aborted matches.
                if (g.match && g.match->started() && !g.matchLogUploaded) {
                    g.matchLogUploaded = true;
                    g.uploadMatchLog();
                }
                if (g.match) std::cout << "match aborted\n";
                g.match.reset();
                g.humanMatchId = 0;
            } else {
                std::cout << "usage: match <domination|dotgoo|join [team]|addbot <team>|status|end>\n";
            }
            return true;
        }

        // Wave 40: human kit casting in matches — directional, like the bots.
        // mcast <q|f|r|rmb> [yaw-degrees]: casts your kit ability toward yaw
        // (default: your current facing). Requires `match join` first.
        // Wave 41: abilities must be unlocked first (DOTGOO draft).
        if (cmd == "mcast") {
            if (!g.match || !g.match->started() || g.humanMatchId == 0) {
                std::cout << "join a match first (match domination|dotgoo, then match join)\n";
                return true;
            }
            std::string slot;
            in >> slot;
            int slotIdx = -1;
            if (slot == "q") slotIdx = 0;
            else if (slot == "f") slotIdx = 1;
            else if (slot == "r") slotIdx = 2;
            else if (slot == "rmb") slotIdx = 3;
            if (slotIdx < 0) {
                std::cout << "usage: mcast <q|f|r|rmb> [yaw-degrees] [down|launch]\n"
                             "  (Cthulhu in Domination: 'rmb down' rips a rock from the ground,\n"
                             "   'rmb launch' hurls the held rock)\n";
                return true;
            }
            const auto* hp = g.match->findPlayer(g.humanMatchId);
            if (hp && slotIdx < 3) {
                const bool unlocked = (slotIdx == 0) ? hp->qUnlocked
                                      : (slotIdx == 1) ? hp->fUnlocked
                                                       : hp->rUnlocked;
                if (!unlocked) {
                    std::cout << slot << " is not unlocked yet"
                              << (hp->pendingPicks > 0
                                      ? " — you have a draft pick: mpick <q|f>"
                                      : "")
                              << ".\n";
                    return true;
                }
            }
            float yaw = hp ? hp->facingYaw : 0.0f;
            // Wave 42: Cthulhu's rock — `mcast rmb down` rips it from the
            // ground, `mcast rmb launch` hurls it. Plain `mcast rmb` swings
            // the held rock, or stuns without it.
            Match::RmbAim aim = Match::RmbAim::Normal;
            // Micro-dev: parse words before numbers. The old order read
            // the next token as a float first, so `mcast rmb down` set
            // failbit on "down" and the verb never reached `aim` — the
            // rock could never be ripped or hurled from the driver.
            std::string tok;
            if (in >> tok) {
                if (tok == "down") aim = Match::RmbAim::Down;
                else if (tok == "launch") aim = Match::RmbAim::Launch;
                else {
                    try {
                        yaw = std::stof(tok) * 3.14159265f / 180.0f;
                    } catch (const std::exception&) {
                        // unknown token — keep current facing
                    }
                    // Optional verb after an explicit yaw: `mcast rmb 90 launch`.
                    if (in >> tok) {
                        if (tok == "down") aim = Match::RmbAim::Down;
                        else if (tok == "launch") aim = Match::RmbAim::Launch;
                    }
                }
            }
            if (g.match->humanCast(g.humanMatchId, slotIdx, yaw, aim)) {
                // Micro-dev: name the ability — "cast q!" is dead air for
                // the game's loudest moment. Cthulhu's rock-RMB gets its
                // own verbs because rightClick.name is the DOTGOO
                // mind-control gaze.
                const bool rock = hp && hp->kitId == "cthulhu_avatar" &&
                                  slotIdx == 3 &&
                                  g.match->modeName() == "capture";
                std::string what = slot;
                double cd = 0.0;
                if (hp && !rock) {
                    if (const CharacterDef* kd = g.charReg.get(hp->kitId)) {
                        if (slotIdx == 3)
                            what = kd->rightClick.name;
                        else {
                            const SpellDef* sp =
                                slotIdx == 0   ? &kd->qAbility
                                : slotIdx == 1 ? &kd->fAbility
                                               : &kd->rAbility;
                            what = sp->name;
                        }
                    }
                }
                if (hp)
                    cd = slotIdx == 0   ? hp->qCd
                       : slotIdx == 1 ? hp->fCd
                       : slotIdx == 2 ? hp->rCd
                                      : std::max(hp->rmbCd, hp->swingCd);
                const std::string verb =
                    rock ? (aim == Match::RmbAim::Down     ? "rock ripped from the ground"
                            : aim == Match::RmbAim::Launch ? "rock hurled"
                                                           : "rock swung")
                         : what + " unleashed";
                std::cout << verb << "!"
                          << (cd > 0.0 ? " (ready in " +
                                                 std::to_string(static_cast<int>(cd + 0.5)) +
                                                 "s)"
                                       : "")
                          << "\n";
            } else {
                // Micro-dev: say WHY the cast failed instead of one vague
                // fizzle — down, held fast, charging, or nothing in the cone.
                const auto* hpf = g.match->findPlayer(g.humanMatchId);
                if (!hpf)
                    std::cout << slot << " fizzled.\n";
                else if (!hpf->alive)
                    std::cout << "you're down — respawning in "
                              << hpf->respawnTimer << "s.\n";
                else if (hpf->ccTimer > 0.0)
                    std::cout << "held fast — " << hpf->ccTimer
                              << "s of crowd control left.\n";
                else {
                    // Rock swings/throws run on the swing cooldown, not
                    // rmbCd — count whichever is longer for slot 3.
                    const double cd =
                        (slotIdx == 0) ? hpf->qCd
                        : (slotIdx == 1) ? hpf->fCd
                        : (slotIdx == 2) ? hpf->rCd
                        : std::max(hpf->rmbCd, hpf->swingCd);
                    if (cd > 0.0)
                        std::cout << slot << " on cooldown (" << cd
                                  << "s left)\n";
                    else
                        std::cout << slot
                                  << " fizzled — nothing caught by the cast. "
                                     "Aim with `mcast " << slot
                                  << " <yaw-degrees>`.\n";
                }
            }
            return true;
        }

        // Wave 41: DOTGOO ability draft — spend a pending pick.
        if (cmd == "mpick") {
            if (!g.match || !g.match->started() || g.humanMatchId == 0) {
                std::cout << "join a match first (match domination|dotgoo, then match join)\n";
                return true;
            }
            std::string which;
            in >> which;
            int slot = (which == "q") ? 0 : (which == "f") ? 1 : -1;
            if (slot < 0) {
                std::cout << "usage: mpick <q|f>\n";
                return true;
            }
            if (g.match->draftAbility(g.humanMatchId, slot))
                std::cout << "unlocked " << which << "!\n";
            else
                std::cout << "no draft pick available (or already unlocked).\n";
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
            // Wave 42: tag the NPC packages so the 20 playable roster
            // kits are distinguishable at a glance.
            std::cout << g.charReg.count()
                      << " characters registered:\n";
            for (const auto& id : g.charReg.list()) {
                const CharacterDef* d = g.charReg.get(id);
                std::cout << "  '" << id << "'"
                          << (isNpcPackage(id) ? " [NPC]" : "")
                          << " — " << d->displayName
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

        // Second shift: `character` command group. `character validate`
        // runs the deep validator (schema, model probe, rig diagnostics,
        // clip coverage); `character list` shows the registry.
        if (cmd == "character") {
            std::string sub; in >> sub;
            if (sub == "list") {
                std::cout << g.charReg.count()
                          << " characters registered:\n";
                for (const auto& id : g.charReg.list()) {
                    const CharacterDef* d = g.charReg.get(id);
                    std::cout << "  '" << id << "'"
                              << (isNpcPackage(id) ? " [NPC]" : "")
                              << " — " << d->displayName << "\n";
                }
                return true;
            }
            if (sub == "validate") {
                std::string id; in >> id;
                if (id.empty()) {
                    std::cout << "usage: character validate "
                                 "<character-id>\n";
                    return true;
                }
                const CharacterPackage* found = nullptr;
                for (const auto& p : g.charLoader.packages())
                    if (p.def.id == id || p.folderName == id) {
                        found = &p;
                        break;
                    }
                if (!found) {
                    std::cout << "no package '" << id << "'";
                    // Micro-dev: forgive a typo the same way `character
                    // select <typo>` does — suggest the closest known
                    // package id/folder. Unlike `character select`, the
                    // vocabulary covers ALL packages (NPC ones included —
                    // they get deep-validated too, they're just not
                    // wearable bodies).
                    std::vector<std::string> pkgIds;
                    for (const auto& p : g.charLoader.packages()) {
                        pkgIds.push_back(p.def.id);
                        if (p.folderName != p.def.id)
                            pkgIds.push_back(p.folderName);
                    }
                    const std::string sug = suggestCommand(id, pkgIds);
                    if (!sug.empty())
                        std::cout << " Did you mean '" << sug << "'?";
                    std::cout << "\nhint: drop a folder in "
                                 "assets/characters/<name>/ with a "
                                 "character.def, then 'addchar <folder>'\n";
                    return true;
                }
                std::cout << "deep validation of '" << found->folderName
                          << "' (" << found->folderPath << "):\n"
                          << CharacterValidator::validateDeep(*found)
                                 .summary();
                return true;
            }
            if (sub == "select") {
                // Wave 27: choose whose kit the avatar casts.
                // Wave 42: only the 20 playable roster entities — NPCs
                // (civilians, cultists, echo) are not wearable bodies.
                std::string id; in >> id;
                const CharacterDef* d = g.charReg.get(id);
                if (!d) {
                    // Micro-dev: forgive a typo the same way `match join
                    // <typo-kit>` does — suggest the closest playable kit
                    // (NPC packages are excluded: never wearable bodies)
                    // and point at `chars` for the full roster.
                    std::cout << "no character '" << id << "'";
                    std::vector<std::string> playable;
                    for (const auto& kid : g.charReg.list())
                        if (!isNpcPackage(kid))
                            playable.push_back(kid);
                    const std::string sug = suggestCommand(id, playable);
                    if (!sug.empty())
                        std::cout << " Did you mean '" << sug << "'?";
                    std::cout << " (see 'chars' for the 20 roster kits)\n";
                    return true;
                }
                if (isNpcPackage(id)) {
                    std::cout << "'" << id
                              << "' is an NPC, not a playable entity "
                                 "(see 'chars' for the 20 roster kits)\n";
                    return true;
                }
                g.activeKitId = id;
                g.kitCooldowns_.clear();
                std::cout << "you now walk as " << d->displayName << "\n"
                          << "  Q: " << d->qAbility.name << "  F: "
                          << d->fAbility.name << "  R: " << d->rAbility.name
                          << "\n  cast with: cast q | cast f | cast r\n";
                return true;
            }
            std::cout << "usage: character <list|validate|select> "
                         "[character-id]\n";
            return true;
        }

        if (cmd == "achievements") {
            std::string sub; in >> sub;
            if (sub == "setplayer") {
                // Wave 16: identify the local player for multiplayer
                // achievements (entity id, faction, team).
                uint64_t id = 0; int faction = 0, team = -1;
                in >> id;
                if (in >> faction) in >> team;
                g.achievements.setLocalPlayer(id, faction, team);
                std::cout << "local player: entity " << id
                          << " faction " << faction << " team " << team
                          << "\n";
                return true;
            }
            const auto& defs = g.achievements.defs();
            size_t unlocked = 0;
            for (const auto& d : defs) {
                if (g.achievements.isUnlocked(d.id)) {
                    ++unlocked;
                    std::cout << "  [x] " << d.name << " — "
                              << d.description << "\n";
                }
            }
            for (const auto& d : defs) {
                if (g.achievements.isUnlocked(d.id)) continue;
                std::cout << "  [ ] " << d.name << " — " << d.description;
                auto pr = g.achievements.progress(d.id);
                if (pr.first >= 0.0) {
                    std::cout << "  (" << pr.first << "/" << pr.second
                              << " " << d.progressUnit << ")";
                }
                std::cout << "\n";
            }
            std::cout << unlocked << "/" << defs.size()
                      << " deeds recorded in the dark.\n";
            return true;
        }

        // Wave 26: the Discovery Codex (wave 31: no renaming — the
        // game's names stand).
        if (cmd == "codex") {
            const auto& all = g.codex.all();
            if (all.empty()) {
                std::cout << "the codex is blank. Walk the world — "
                             "landmarks, beasts, and relics wait to be "
                             "found.\n";
                return true;
            }
            std::cout << "── the Discovery Codex (" << all.size()
                      << ") ──\n";
            for (const auto& d : all) {
                std::cout << "  [" << discoveryKindName(d.kind) << "] "
                          << d.name << "  <" << d.id << ">"
                          << (d.night ? " ✦" : "") << "\n";
                if (!d.flavor.empty())
                    std::cout << "      \"" << d.flavor << "\"\n";
            }
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
            // Wave 30: real interaction — the nearest civilian shares a
            // rumor pointing at the nearest UNDISCOVERED landmark, closing
            // the NMS loop (rumor -> horizon -> discovery -> codex).
            const Vec3 ap = g.avatar.position();
            const Civilian* near = nullptr;
            float nearD = 8.0f; // talk radius (generous; they're skittish)
            auto consider = [&](const Civilian* c) {
                if (!c || !c->alive()) return;
                const float d = dist(ap, c->position());
                if (d < nearD) { nearD = d; near = c; }
            };
            for (const auto& c : g.freeroam.civilians()) consider(c.get());
            for (const auto& e : g.world)
                if (e->type() == EntityType::Civilian)
                    consider(static_cast<const Civilian*>(e.get()));
            if (!near) {
                std::cout << "no one within earshot — the wind answers "
                             "nothing\n";
                return true;
            }
            // Nearest landmark the codex hasn't logged yet.
            const FreeRoamMode::Landmark* target = nullptr;
            float targetD = 1e9f;
            for (const auto& lm : g.freeroam.landmarks()) {
                if (g.codex.find(DiscoveryCodex::idFor(
                        DiscoveryKind::Landmark, lm.name)))
                    continue; // already discovered
                const float d = dist(ap, lm.pos);
                if (d < targetD) { targetD = d; target = &lm; }
            }
            std::cout << "a civilian glances up, trembling:\n";
            if (target) {
                std::cout << "  \"they say " << target->name << " lies "
                          << static_cast<int>(targetD + 0.5f) << "m "
                          << (target->pos.x >= ap.x ? "east" : "west") << "-"
                          << (target->pos.z >= ap.z ? "south" : "north")
                          << ". " << target->flavor << "\"\n";
            } else {
                static const char* done[] = {
                    "\"you've seen all the wonders. What did they cost you?\"",
                    "\"the dark is quiet tonight. Enjoy it.\"",
                    "\"they say the god walks among us. I believe them now.\"",
                };
                std::cout << "  " << done[g.rng.intRange(
                    0, static_cast<int>(sizeof(done) / sizeof(done[0])) -
                           1)]
                          << "\n";
            }
            g.tickSecond();
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

        std::cout << "unknown command '" << cmd << "'.";
        const std::string suggestion = suggestCommand(cmd, kDriverCommands);
        if (!suggestion.empty()) std::cout << " Did you mean '" << suggestion << "'?";
        std::cout << " Type 'help'.\n";
    return true;
}

// Wave 27: one-line arrival survey after every `move`.
void printArrivalSurvey(BetaGame& g) {
    const Vec3 ap = g.avatar.position();
    std::string bits;
    const FreeRoamMode::Landmark* near = nullptr;
    float nearD = 1e9f;
    for (const auto& lm : g.freeroam.landmarks()) {
        const float d = dist(ap, lm.pos);
        if (d < nearD) { nearD = d; near = &lm; }
    }
    if (near) {
        std::ostringstream ss;
        ss << "  nearest wonder: " << near->name << " ("
           << static_cast<int>(nearD + 0.5f) << "m "
           << (near->pos.x >= ap.x ? "east" : "west") << "-"
           << (near->pos.z >= ap.z ? "south" : "north") << ")\n";
        bits += ss.str();
    }
    int relics = 0, civs = 0, beasts = 0;
    for (const auto& r : g.freeroam.relics())
        if (r->alive() && dist(ap, r->position()) < 30.0f) ++relics;
    for (const auto& c : g.freeroam.civilians())
        if (c->alive() && dist(ap, c->position()) < 40.0f) ++civs;
    for (const auto& c : g.freeroam.creatures())
        if (c->alive() && dist(ap, c->position()) < 40.0f) ++beasts;
    if (relics)
        bits += "  " + std::to_string(relics) +
                " relic(s) glint nearby — walk close to claim\n";
    if (civs) bits += "  " + std::to_string(civs) + " civilians mill about\n";
    if (beasts)
        bits += "  " + std::to_string(beasts) + " wild creature(s) stir\n";
    if (!bits.empty()) std::cout << bits;
}

#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <cctype>
#include <chrono>
#ifndef _WIN32
#include <sys/select.h>
#include <unistd.h>
#endif

// ---------- Wave 42: visual select server ----------
// The clickable character screen (tools/match_select.html) talks to the
// game through a tiny localhost HTTP server. Click "Join Match" in the
// browser -> POST /join -> the driver joins the match. No copy/paste.
namespace {

constexpr uint16_t kSelectPort = 8471;

struct JoinRequest {
    std::string kit;
    int team = -1; // -1 = auto
};

std::queue<JoinRequest> g_joinQueue;
std::mutex g_joinMutex;
std::atomic<bool> g_selectRunning{false};

// Wave 42: select-screen status (polled by the HTML page).
// - select_open: within the 30s pick window and the human hasn't joined.
// - player_joined / player_afk: for the REJOIN button.
struct SelectStatus {
    bool matchActive = false;
    bool selectOpen = false;
    int selectSecsLeft = 0;
    bool playerJoined = false;
    bool playerAfk = false;
    std::string mode;
};
SelectStatus g_status;
std::mutex g_statusMutex;
void setSelectStatus(const SelectStatus& s) {
    std::lock_guard<std::mutex> lk(g_statusMutex);
    g_status = s;
}
SelectStatus getSelectStatus() {
    std::lock_guard<std::mutex> lk(g_statusMutex);
    return g_status;
}

// Minimal JSON field extraction (no dependency): finds "key": "value"
// or "key": number.
std::string jsonStrField(const std::string& body, const std::string& key) {
    std::string pat = "\"" + key + "\"";
    size_t p = body.find(pat);
    if (p == std::string::npos) return "";
    p = body.find(':', p + pat.size());
    if (p == std::string::npos) return "";
    ++p;
    while (p < body.size() && std::isspace((unsigned char)body[p])) ++p;
    if (p < body.size() && body[p] == '"') {
        size_t e = body.find('"', p + 1);
        if (e == std::string::npos) return "";
        return body.substr(p + 1, e - p - 1);
    }
    size_t e = p;
    while (e < body.size() && (std::isdigit((unsigned char)body[e]) || body[e] == '-'))
        ++e;
    return body.substr(p, e - p);
}

void handleSelectConn(cultulhu::net::TcpSocket conn) {
    // Read the request (small; single recv is fine for our tiny client).
    char buf[4096];
    long n = conn.recvSome(buf, sizeof(buf) - 1, 2000);
    if (n <= 0) return;
    buf[n] = '\0';
    std::string req(buf, static_cast<size_t>(n));

    // CORS preflight / simple headers for file:// origins.
    const char* cors =
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n";

    auto sendJson = [&](int code, const std::string& json) {
        std::string resp = "HTTP/1.1 " + std::to_string(code) +
                           (code == 200 ? " OK" : " Bad Request") + "\r\n" +
                           cors + "Content-Type: application/json\r\n" +
                           "Content-Length: " + std::to_string(json.size()) +
                           "\r\nConnection: close\r\n\r\n" + json;
        conn.sendAll(resp.data(), resp.size());
    };

    if (req.rfind("OPTIONS", 0) == 0) {
        sendJson(200, "{}");
        return;
    }
    if (req.rfind("POST /join", 0) != 0) {
        if (req.rfind("GET /status", 0) == 0) {
            SelectStatus st = getSelectStatus();
            std::string json = "{\"ok\":true,\"matchActive\":" +
                std::string(st.matchActive ? "true" : "false") +
                ",\"selectOpen\":" +
                std::string(st.selectOpen ? "true" : "false") +
                ",\"selectSecsLeft\":" + std::to_string(st.selectSecsLeft) +
                ",\"playerJoined\":" +
                std::string(st.playerJoined ? "true" : "false") +
                ",\"playerAfk\":" +
                std::string(st.playerAfk ? "true" : "false") +
                ",\"mode\":\"" + st.mode + "\"}";
            sendJson(200, json);
            return;
        }
        sendJson(404, "{\"ok\":false,\"error\":\"unknown path\"}");
        return;
    }
    // Body starts after the blank line.
    size_t bh = req.find("\r\n\r\n");
    std::string body = (bh == std::string::npos) ? "" : req.substr(bh + 4);
    std::string kit = jsonStrField(body, "kit");
    std::string teamS = jsonStrField(body, "team");
    if (kit.empty()) {
        sendJson(400, "{\"ok\":false,\"error\":\"missing kit\"}");
        return;
    }
    JoinRequest jr;
    jr.kit = kit;
    jr.team = teamS.empty() ? -1 : std::stoi(teamS);
    {
        std::lock_guard<std::mutex> lk(g_joinMutex);
        g_joinQueue.push(std::move(jr));
    }
    sendJson(200, "{\"ok\":true}");
}

void selectServerThread() {
    cultulhu::net::TcpListener listener;
    if (!listener.listen(kSelectPort)) return;
    g_selectRunning = true;
    while (g_selectRunning) {
        auto conn = listener.accept(500 /*ms*/);
        if (conn) handleSelectConn(std::move(*conn));
    }
}

// Drain pending join requests (called from the REPL loop).
bool pollJoinRequest(JoinRequest& out) {
    std::lock_guard<std::mutex> lk(g_joinMutex);
    if (g_joinQueue.empty()) return false;
    out = std::move(g_joinQueue.front());
    g_joinQueue.pop();
    return true;
}

} // namespace

int main() {
    BetaGame g;
    g.setupWorld();
    NetSession nets;

    // Wave 42: start the select-screen server (browser -> game bridge).
    std::thread selectThread(selectServerThread);
    selectThread.detach();

    std::cout << "CULT-ULHU playable beta — free roam\n"
              << "Your eldritch avatar stalks the map.\n"
              << "  first steps: look | move e 10 | character select "
                 "cthulhu_avatar | cast q\n"
              << "  (type 'help' for the full grimoire)\n";

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
        size_t warnTotal = 0, invalidTotal = 0;
        for (const auto& p : g.charLoader.packages()) {
            ValidationReport rep = CharacterValidator::validate(p);
            warnTotal += rep.warnings.size();
            if (!rep.ok) ++invalidTotal;
        }
        // Wave 27: collapse boot noise — one line, details on demand.
        std::cout << "characters: " << n << " kit(s) loaded";
        if (invalidTotal) std::cout << ", " << invalidTotal << " INVALID";
        if (warnTotal)
            std::cout << " (" << warnTotal
                      << " warnings — 'character validate <id>' for details)";
        std::cout << "\n";
        for (const auto& e : g.charLoader.loadErrors())
            std::cout << "  load error: " << e << "\n";
    }

    g.printStatus();

    // Wave 42: REPL loop with select-screen join polling. stdin is polled
    // via select() (100ms) so browser "Join Match" clicks land instantly,
    // even when the player isn't typing.
    //
    // Wave 42: AFK takeover. If the human is in a match and gives no input
    // for 3 minutes, a bot pilots their fighter (match never pauses, no ban).
    // Any input returns control. AFK time accrues for end-of-match rewards.
    auto lastInput = std::chrono::steady_clock::now();
    constexpr auto kAfkTimeout = std::chrono::minutes(3);
    // Wave 42: character select countdown — 30s from match start to pick
    // a fighter. Miss it and you're back in free-roam (menu).
    constexpr auto kSelectWindow = std::chrono::seconds(30);
    bool selectTimeoutAnnounced = false;
    std::string line;
    bool running = true;
    while (running) {
        // Wave 42: publish select-screen status for the /status endpoint.
        {
            SelectStatus st;
            st.matchActive = g.match && g.match->started();
            st.playerJoined = g.humanMatchId != 0;
            st.playerAfk = st.playerJoined && g.match->isPlayerAfk(g.humanMatchId);
            if (st.matchActive) {
                st.mode = g.match->modeName();
                auto elapsed = std::chrono::steady_clock::now() - g.matchStartTime;
                if (!st.playerJoined && elapsed < kSelectWindow) {
                    st.selectOpen = true;
                    st.selectSecsLeft =
                        static_cast<int>((kSelectWindow - elapsed).count() / 1000000000);
                }
            }
            setSelectStatus(st);

            // Select timeout: no pick in 30s -> back to free-roam (menu).
            if (st.matchActive && !st.playerJoined && !selectTimeoutAnnounced) {
                auto elapsed = std::chrono::steady_clock::now() - g.matchStartTime;
                if (elapsed >= kSelectWindow) {
                    selectTimeoutAnnounced = true;
                    std::cout << "\n[select] 30s pick window closed — you're in "
                                 "free-roam. Start another match when ready, "
                                 "or `match join` to jump into this one.\n";
                }
            }
            // Reset the timeout flag when a new match starts.
            if (!st.matchActive) selectTimeoutAnnounced = false;
        }

        // Drain select-screen join requests first.
        JoinRequest jr;
        while (pollJoinRequest(jr)) {
            lastInput = std::chrono::steady_clock::now();
            std::string cmd = "match join";
            if (jr.team >= 0) cmd += " " + std::to_string(jr.team);
            cmd += " " + jr.kit;
            std::cout << "\n[select screen] " << cmd << "\n";
            if (!processLine(g, nets, cmd)) { running = false; break; }
        }
        if (!running) break;

        // AFK check (only when the human is in an active match).
        if (g.match && g.match->started() && g.humanMatchId != 0 &&
            !g.match->isPlayerAfk(g.humanMatchId) &&
            std::chrono::steady_clock::now() - lastInput > kAfkTimeout) {
            g.match->setPlayerAfk(g.humanMatchId, true);
            std::cout << "\n[afk] 3 minutes with no input — a bot is piloting "
                         "your fighter. Type anything to take back control. "
                         "The match goes on.\n";
        }

#ifndef _WIN32
        // Non-blocking stdin check (POSIX): select-screen join clicks
        // land instantly even when the player isn't typing.
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        struct timeval tv{0, 100000}; // 100ms
        int r = select(STDIN_FILENO + 1, &rfds, nullptr, nullptr, &tv);
        if (r < 0) break; // error
        if (r == 0) continue; // timeout: loop back to check joins
#else
        // Windows: select() doesn't work on stdin, so the join queue is
        // drained above and lands when the player next presses Enter.
#endif
        std::cout << "\n> " << std::flush;
        if (!std::getline(std::cin, line)) break;
        lastInput = std::chrono::steady_clock::now();
        // Returning from AFK: take the fighter back from the bot pilot.
        if (g.match && g.match->started() && g.humanMatchId != 0 &&
            g.match->isPlayerAfk(g.humanMatchId)) {
            g.match->setPlayerAfk(g.humanMatchId, false);
            std::cout << "[back] You're piloting your fighter again.\n";
        }
        if (!processLine(g, nets, line)) break;
    }

    std::cout << "The dream ends. Power: " << g.power.value() << "\n";
    return 0;
}
