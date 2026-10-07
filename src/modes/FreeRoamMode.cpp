#include "modes/FreeRoamMode.h"

#include "core/RNG.h"

#include <cmath>

namespace cultulhu {

FreeRoamMode::FreeRoamMode(EventBus& bus, GameClock& clock, RNG& rng)
    : GameMode(bus, clock), rng_(rng) {}

void FreeRoamMode::setMapBounds(Vec3 mn, Vec3 mx) {
    minBound_ = mn;
    maxBound_ = mx;
}

void FreeRoamMode::addCivilianSpawn(Vec3 p, float radius) {
    civSpawns_.push_back(SpawnPoint{clampToBounds(p), radius});
}

void FreeRoamMode::addCreatureSpawn(Vec3 p, std::string species,
                                   float radius) {
    creatureSpawns_.push_back(
        CreatureSpawn{SpawnPoint{clampToBounds(p), radius},
                      std::move(species)});
}

void FreeRoamMode::addRelicSpawn(Vec3 p, float amplifier) {
    relics_.push_back(std::make_unique<Relic>(clampToBounds(p), amplifier));
}

double FreeRoamMode::hourOfDay() const {
    if (dayLength_ <= 0.0) return 12.0;
    const double dayFrac =
        std::fmod(clock_.now() / dayLength_, 1.0);
    return dayFrac * 24.0;
}

bool FreeRoamMode::isNight() const {
    const double h = hourOfDay();
    return h >= 22.0 || h < 6.0;
}

void FreeRoamMode::update(double dt) {
    spawnTimer_ += dt;
    if (spawnTimer_ < spawnInterval_) return;
    spawnTimer_ = 0.0;

    if (civilians_.size() < civilianCap_ && !civSpawns_.empty()) {
        const SpawnPoint& s =
            civSpawns_[static_cast<size_t>(
                rng_.intRange(0, static_cast<int>(civSpawns_.size()) - 1))];
        civilians_.push_back(std::make_unique<Civilian>(randomPoint(s)));
    }
    if (creatures_.size() < creatureCap_ && !creatureSpawns_.empty()) {
        const CreatureSpawn& s = creatureSpawns_[static_cast<size_t>(
            rng_.intRange(0, static_cast<int>(creatureSpawns_.size()) - 1))];
        creatures_.push_back(std::make_unique<Creature>(
            FACTION_NEUTRAL, randomPoint(s.point), s.species));
    }
}

Vec3 FreeRoamMode::randomPoint(const SpawnPoint& s) {
    const float a = rng_.uniform(0.0f, 6.2831853f);
    const float r = rng_.uniform(0.0f, s.radius);
    return clampToBounds(
        Vec3{s.pos.x + std::cos(a) * r, s.pos.y, s.pos.z + std::sin(a) * r});
}

Vec3 FreeRoamMode::clampToBounds(Vec3 p) const {
    if (p.x < minBound_.x) p.x = minBound_.x;
    if (p.x > maxBound_.x) p.x = maxBound_.x;
    if (p.z < minBound_.z) p.z = minBound_.z;
    if (p.z > maxBound_.z) p.z = maxBound_.z;
    return p;
}

} // namespace cultulhu
