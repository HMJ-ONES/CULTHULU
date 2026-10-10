#include "achievements/AchievementSystem.h"

#include "save/SaveSystem.h"

namespace cultulhu {

const char* AchievementSystem::kConvTotal = "conv_total";
const char* AchievementSystem::kConvDream = "conv_dream";
const char* AchievementSystem::kDistrictsRazed = "districts_razed";
const char* AchievementSystem::kBuildingsDestroyed = "buildings_destroyed";
const char* AchievementSystem::kCitiesDestroyed = "cities_destroyed";
const char* AchievementSystem::kRelicsClaimed = "relics_claimed";
const char* AchievementSystem::kValeRelics = "vale_relics";
const char* AchievementSystem::kChampionsSummoned = "champions_summoned";
const char* AchievementSystem::kDholesSlain = "dholes_slain";
const char* AchievementSystem::kPvpKillsLifetime = "pvp_kills_lifetime";
const char* AchievementSystem::kMatchPvpKills = "match_pvp_kills";
const char* AchievementSystem::kMatchDeaths = "match_deaths";
const char* AchievementSystem::kDiscoveries = "discoveries";
const char* AchievementSystem::kNightDiscoveries = "night_discoveries";

namespace {

std::vector<AchievementDef> buildDefs() {
    return {
        // ---- single-player: lifetime ----
        {"first_flesh", "First Flesh",
         "Convert your first cultist. The congregation begins.",
         "souls", 1.0, false},
        {"flock_multiplies", "The Flock Multiplies",
         "Gather 25 souls beneath your shadow.",
         "souls", 25.0, false},
        {"ashes_of_man", "Ashes of Man",
         "Raze your first city district to the ground.",
         "districts", 1.0, false},
        {"dreamthief", "Dreamthief",
         "Steal 50 souls through dream-visions.",
         "dream-souls", 50.0, false},
        {"price_of_power", "The Price of Power",
         "Claim your first cursed relic.",
         "relics", 1.0, false},
        {"architect_of_desolation", "Architect of Desolation",
         "Destroy 25 buildings.",
         "buildings", 25.0, false},
        {"burrowers_bane", "Burrower's Bane",
         "Slay a Dhole in the lightless Vale.",
         "", 0.0, false},
        {"what_lies_beneath", "What Lies Beneath",
         "Claim the relic sealed in the Vale's vault.",
         "", 0.0, false},
        {"the_stirring", "The Stirring",
         "Complete the Grand Summoning.",
         "", 0.0, false},
        {"unmaker_of_worlds", "Unmaker of Worlds",
         "Raze 5 cities. Nothing remains.",
         "cities", 5.0, false},
        // ---- wave 26: exploration ----
        {"first_wonder", "First Wonder",
         "Log your first discovery in the codex.",
         "discoveries", 1.0, false},
        {"cartographer", "Cartographer",
         "Log 10 discoveries in the codex.",
         "discoveries", 10.0, false},
        {"lorekeeper", "Lorekeeper",
         "Log 25 discoveries in the codex.",
         "discoveries", 25.0, false},
        {"night_pilgrim", "Night Pilgrim",
         "Make a discovery under starlight.",
         "", 0.0, false},
        // ---- multiplayer ----
        {"first_blood", "First Blood",
         "Draw first blood against another player.",
         "", 0.0, true},
        {"reaper", "Reaper",
         "Claim 10 player kills in a single match.",
         "kills this match", 10.0, true},
        {"unbroken", "Unbroken",
         "Win a match without dying.",
         "", 0.0, true},
        {"standard_bearer", "Standard-Bearer",
         "Seize a capture point.",
         "", 0.0, true},
        {"godslayer", "Godslayer",
         "Strike the killing blow against an enemy Great Old One.",
         "", 0.0, true},
        {"turncoat", "Turncoat",
         "Turn an enemy cultist to your cause.",
         "", 0.0, true},
        {"dominion", "Dominion",
         "Lead your cult to victory in a 5v5 match.",
         "", 0.0, true},
    };
}

} // namespace

AchievementSystem::AchievementSystem(EventBus& bus)
    : bus_(bus), defs_(buildDefs()) {
    for (const auto& d : defs_) unlocked_[d.id] = false;
    bus_.subscribe(EventType::ConversionPerformed,
                   [this](const GameEvent& e) { onEvent(e); });
    bus_.subscribe(EventType::DistrictRazed,
                   [this](const GameEvent& e) { onEvent(e); });
    bus_.subscribe(EventType::CityBuildingDestroyed,
                   [this](const GameEvent& e) { onEvent(e); });
    bus_.subscribe(EventType::CityDestroyed,
                   [this](const GameEvent& e) { onEvent(e); });
    bus_.subscribe(EventType::RelicClaimed,
                   [this](const GameEvent& e) { onEvent(e); });
    bus_.subscribe(EventType::ValeRelicClaimed,
                   [this](const GameEvent& e) { onEvent(e); });
    bus_.subscribe(EventType::ChampionSummoned,
                   [this](const GameEvent& e) { onEvent(e); });
    bus_.subscribe(EventType::MonstrositySlain,
                   [this](const GameEvent& e) { onEvent(e); });
    bus_.subscribe(EventType::PlayerKilled,
                   [this](const GameEvent& e) { onEvent(e); });
    bus_.subscribe(EventType::PointCaptured,
                   [this](const GameEvent& e) { onEvent(e); });
    bus_.subscribe(EventType::GreatOldOneSlain,
                   [this](const GameEvent& e) { onEvent(e); });
    bus_.subscribe(EventType::MatchStarted,
                   [this](const GameEvent& e) { onEvent(e); });
    bus_.subscribe(EventType::MatchEnded,
                   [this](const GameEvent& e) { onEvent(e); });
    bus_.subscribe(EventType::DiscoveryMade,
                   [this](const GameEvent& e) { onEvent(e); });
}

void AchievementSystem::setLocalPlayer(uint64_t entityId, int faction,
                                      int team) {
    localId_ = entityId;
    localFaction_ = faction;
    localTeam_ = team;
}

bool AchievementSystem::isUnlocked(const std::string& id) const {
    auto it = unlocked_.find(id);
    return it != unlocked_.end() && it->second;
}

std::vector<std::string> AchievementSystem::unlockedIds() const {
    std::vector<std::string> out;
    for (const auto& d : defs_)
        if (isUnlocked(d.id)) out.push_back(d.id);
    return out;
}

double AchievementSystem::counter(const char* name) const {
    auto it = counters_.find(name);
    return it != counters_.end() ? it->second : 0.0;
}

void AchievementSystem::bump(const char* name, double amount) {
    counters_[name] = counter(name) + amount;
}

std::pair<double, double> AchievementSystem::progress(
    const std::string& id) const {
    const AchievementDef* def = nullptr;
    for (const auto& d : defs_)
        if (d.id == id) { def = &d; break; }
    if (!def || def->progressUnit.empty()) return {-1.0, -1.0};
    double cur = 0.0;
    if (id == "first_flesh" || id == "flock_multiplies") cur = counter(kConvTotal);
    else if (id == "ashes_of_man") cur = counter(kDistrictsRazed);
    else if (id == "dreamthief") cur = counter(kConvDream);
    else if (id == "price_of_power") cur = counter(kRelicsClaimed);
    else if (id == "architect_of_desolation") cur = counter(kBuildingsDestroyed);
    else if (id == "unmaker_of_worlds") cur = counter(kCitiesDestroyed);
    else if (id == "reaper") cur = counter(kMatchPvpKills);
    else if (id == "first_wonder" || id == "cartographer" ||
             id == "lorekeeper")
        cur = counter(kDiscoveries);
    if (cur > def->progressTarget) cur = def->progressTarget;
    return {cur, def->progressTarget};
}

void AchievementSystem::unlock(const std::string& id) {
    if (isUnlocked(id)) return;
    unlocked_[id] = true;
    // Narration lives in the driver (it subscribes to
    // EventType::AchievementUnlocked) — the sim stays output-clean.
    GameEvent e(EventType::AchievementUnlocked);
    e.tag = id;
    bus_.publish(e);
}

void AchievementSystem::onEvent(const GameEvent& e) {
    switch (e.type) {
    case EventType::ConversionPerformed: {
        bump(kConvTotal, e.amount);
        if (e.tag == "dream_whisper") bump(kConvDream, e.amount);
        // Turncoat: the turned soul belonged to a rival faction.
        if (e.faction != FACTION_NEUTRAL && e.faction != localFaction_)
            unlock("turncoat");
        if (counter(kConvTotal) >= 1.0) unlock("first_flesh");
        if (counter(kConvTotal) >= 25.0) unlock("flock_multiplies");
        if (counter(kConvDream) >= 50.0) unlock("dreamthief");
        break;
    }
    case EventType::DistrictRazed:
        bump(kDistrictsRazed, 1.0);
        if (counter(kDistrictsRazed) >= 1.0) unlock("ashes_of_man");
        break;
    case EventType::CityBuildingDestroyed:
        bump(kBuildingsDestroyed, 1.0);
        if (counter(kBuildingsDestroyed) >= 25.0)
            unlock("architect_of_desolation");
        break;
    case EventType::CityDestroyed:
        bump(kCitiesDestroyed, 1.0);
        if (counter(kCitiesDestroyed) >= 5.0) unlock("unmaker_of_worlds");
        break;
    case EventType::RelicClaimed:
    case EventType::ValeRelicClaimed:
        bump(kRelicsClaimed, 1.0);
        if (e.type == EventType::ValeRelicClaimed) bump(kValeRelics, 1.0);
        if (counter(kRelicsClaimed) >= 1.0) unlock("price_of_power");
        if (counter(kValeRelics) >= 1.0) unlock("what_lies_beneath");
        break;
    case EventType::ChampionSummoned:
        bump(kChampionsSummoned, 1.0);
        if (counter(kChampionsSummoned) >= 1.0) unlock("the_stirring");
        break;
    case EventType::MonstrositySlain:
        if (e.tag == "dhole") {
            bump(kDholesSlain, 1.0);
            if (counter(kDholesSlain) >= 1.0) unlock("burrowers_bane");
        }
        break;
    case EventType::PlayerKilled: {
        if (localId_ != 0 && e.sourceId == localId_) {
            bump(kPvpKillsLifetime, 1.0);
            bump(kMatchPvpKills, 1.0);
            if (counter(kPvpKillsLifetime) >= 1.0) unlock("first_blood");
            if (counter(kMatchPvpKills) >= 10.0) unlock("reaper");
        }
        if (localId_ != 0 && e.targetId == localId_) bump(kMatchDeaths, 1.0);
        break;
    }
    case EventType::PointCaptured:
        if (localTeam_ >= 0 && e.faction == localTeam_)
            unlock("standard_bearer");
        break;
    case EventType::GreatOldOneSlain:
        if (localId_ != 0 && e.sourceId == localId_) unlock("godslayer");
        break;
    case EventType::MatchStarted:
        counters_[kMatchPvpKills] = 0.0;
        counters_[kMatchDeaths] = 0.0;
        break;
    case EventType::DiscoveryMade:
        bump(kDiscoveries, 1.0);
        if (e.faction == 1) {
            bump(kNightDiscoveries, 1.0);
            unlock("night_pilgrim");
        }
        if (counter(kDiscoveries) >= 1.0) unlock("first_wonder");
        if (counter(kDiscoveries) >= 10.0) unlock("cartographer");
        if (counter(kDiscoveries) >= 25.0) unlock("lorekeeper");
        break;
    case EventType::MatchEnded:
        if (localTeam_ >= 0 && e.faction == localTeam_) {
            unlock("dominion");
            if (counter(kMatchDeaths) <= 0.0) unlock("unbroken");
        }
        break;
    default:
        break;
    }
}

void AchievementSystem::saveTo(GameState& s) const {
    s.unlockedAchievements.clear();
    for (const auto& d : defs_)
        if (isUnlocked(d.id)) s.unlockedAchievements.push_back(d.id);
    s.achievementProgress = counters_;
}

void AchievementSystem::loadFrom(const GameState& s) {
    for (const auto& d : defs_) unlocked_[d.id] = false;
    for (const auto& id : s.unlockedAchievements) unlocked_[id] = true;
    counters_ = s.achievementProgress;
}

} // namespace cultulhu
