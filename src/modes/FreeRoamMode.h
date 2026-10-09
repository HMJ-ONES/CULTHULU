#pragma once

#include "modes/GameMode.h"
#include "ai/RivalBot.h"
#include "entities/Structures.h"
#include "entities/Units.h"

#include <memory>
#include <string>
#include <vector>

namespace cultulhu {

class RNG;

// Free roam: an open explorable map with no objectives. The player's eldritch
// avatar roams freely; civilians and creatures spawn at spawn points, relics
// wait at relic sites, and a day/night clock drives ambient hooks (night
// boosts prayer/rest theming in the AI layer).
class FreeRoamMode : public GameMode {
public:
    struct SpawnPoint {
        Vec3 pos;
        float radius = 10.0f;
    };

    FreeRoamMode(EventBus& bus, GameClock& clock, RNG& rng);

    void setMapBounds(Vec3 mn, Vec3 mx);
    void addCivilianSpawn(Vec3 p, float radius = 10.0f);
    void addCreatureSpawn(Vec3 p, std::string species, float radius = 10.0f);
    void addRelicSpawn(Vec3 p, float amplifier);

    // Day/night clock: hourOfDay in [0,24), derived from game time.
    double hourOfDay() const;
    bool isNight() const; // 22:00 - 06:00
    void setDayLength(double seconds) { dayLength_ = seconds; }

    // Spawn tuning.
    void setCivilianCap(size_t n) { civilianCap_ = n; }
    void setCreatureCap(size_t n) { creatureCap_ = n; }
    void setSpawnInterval(double s) { spawnInterval_ = s; }

    void update(double dt) override;
    bool isOver() const override { return false; } // free roam never ends

    // The player's avatar, for civilian flee / rival-bot targeting.
    // Not owned. Set by the driver each tick (null = no player present).
    void setAvatar(Entity* a) { avatar_ = a; }

    // Spawn a rival bot (free-roam "bot" AI) at pos. Returns its id.
    uint64_t spawnBot(Vec3 pos);

    // Beta-owned world population.
    const std::vector<std::unique_ptr<Civilian>>& civilians() const {
        return civilians_;
    }
    const std::vector<std::unique_ptr<Creature>>& creatures() const {
        return creatures_;
    }
    const std::vector<std::unique_ptr<RivalBot>>& bots() const { return bots_; }
    const std::vector<std::unique_ptr<Relic>>& relics() const { return relics_; }
    size_t civilianSpawnCount() const { return civSpawns_.size(); }
    size_t creatureSpawnCount() const { return creatureSpawns_.size(); }

private:
    RNG& rng_;
    Vec3 minBound_{-500.0f, 0.0f, -500.0f};
    Vec3 maxBound_{500.0f, 0.0f, 500.0f};
    std::vector<SpawnPoint> civSpawns_;

    struct CreatureSpawn {
        SpawnPoint point;
        std::string species;
    };
    std::vector<CreatureSpawn> creatureSpawns_;

    std::vector<std::unique_ptr<Civilian>> civilians_;
    std::vector<std::unique_ptr<Creature>> creatures_;
    std::vector<std::unique_ptr<RivalBot>> bots_;
    std::vector<std::unique_ptr<Relic>> relics_;
    Entity* avatar_ = nullptr; // not owned; set by the driver

    double dayLength_ = 600.0; // one full day = 10 game-minutes by default
    double spawnTimer_ = 0.0;
    double spawnInterval_ = 20.0;
    size_t civilianCap_ = 30;
    size_t creatureCap_ = 12;

    Vec3 randomPoint(const SpawnPoint& s);
    Vec3 clampToBounds(Vec3 p) const;
    void updateCivilians(double dt);
    void updateBots(double dt);
    void updateCreatures(double dt); // retaliation vs rival bots
};

} // namespace cultulhu
