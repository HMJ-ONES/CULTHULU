#include "modes/FreeRoamMode.h"

#include "ai/AI.h"
#include "core/RNG.h"

#include <algorithm>
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

void FreeRoamMode::addRelicSpawn(Vec3 p, float amplifier, std::string name) {
    relics_.push_back(
        std::make_unique<Relic>(clampToBounds(p), amplifier, std::move(name)));
}

// Wave 26: landmarks — named places worth walking toward.
void FreeRoamMode::addLandmark(std::string name, Vec3 pos, float radius,
                               std::string flavor) {
    Landmark lm;
    lm.name = std::move(name);
    lm.flavor = std::move(flavor);
    lm.pos = clampToBounds(pos);
    lm.radius = radius;
    landmarks_.push_back(std::move(lm));
}

// Wave 26: claim (remove) the first live relic within radius of pos.
bool FreeRoamMode::claimRelicNear(Vec3 pos, float radius, float& amplifierOut,
                                  std::string& nameOut) {
    for (auto it = relics_.begin(); it != relics_.end(); ++it) {
        if (!(*it)->alive()) continue;
        if ((*it)->position().distance(pos) <= radius) {
            amplifierOut = (*it)->amplifier();
            nameOut = (*it)->name();
            relics_.erase(it);
            return true;
        }
    }
    return false;
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

uint64_t FreeRoamMode::spawnBot(Vec3 pos) {
    auto b = std::make_unique<RivalBot>(clampToBounds(pos), rng_);
    uint64_t id = b->id();
    bots_.push_back(std::move(b));
    return id;
}

void FreeRoamMode::updateCivilians(double dt) {
    for (auto& c : civilians_) {
        if (!c->alive()) continue;
        // Nearest threat: the player's avatar or a rival bot.
        Vec3 threatPos{0, 0, 0};
        bool hasThreat = false;
        float bestD = 12.0f;
        if (avatar_ && avatar_->alive()) {
            float d = c->position().distance(avatar_->position());
            if (d < bestD) {
                bestD = d;
                threatPos = avatar_->position();
                hasThreat = true;
            }
        }
        for (const auto& b : bots_) {
            if (!b->alive()) continue;
            float d = c->position().distance(b->position());
            if (d < bestD) {
                bestD = d;
                threatPos = b->position();
                hasThreat = true;
            }
        }
        if (hasThreat) {
            c->setFleeing(true);
            ai::civilianFlee(*c, threatPos, dt);
            continue;
        }
        if (c->fleeing()) {
            c->setFleeing(false);
            ai::civilianCalm(*c);
        }
        // Wander: stroll to a nearby point, idle, repeat.
        if (c->idleTimer() > 0.0) {
            c->setIdleTimer(c->idleTimer() - dt);
            continue;
        }
        if (!c->hasWanderTarget()) {
            c->setWanderTarget(clampToBounds(
                c->position() + Vec3(rng_.uniform(-30.0f, 30.0f), 0.0f,
                                     rng_.uniform(-30.0f, 30.0f))));
        }
        if ((c->wanderTarget() - c->position()).length() < 2.0f) {
            c->clearWanderTarget();
            c->setIdleTimer(rng_.uniform(2.0, 6.0));
        } else {
            ai::moveToward(*c, c->wanderTarget(), dt, 2.5f);
        }
    }
    // Prune the dead.
    civilians_.erase(
        std::remove_if(civilians_.begin(), civilians_.end(),
                       [](const std::unique_ptr<Civilian>& c) {
                           return !c->alive();
                       }),
        civilians_.end());
}

void FreeRoamMode::updateBots(double dt) {
    RivalBot::Context ctx;
    ctx.avatar = avatar_;
    for (const auto& cr : creatures_)
        if (cr->alive()) ctx.creatures.push_back(cr.get());
    for (auto& b : bots_) b->update(dt, ctx);
    bots_.erase(std::remove_if(bots_.begin(), bots_.end(),
                               [](const std::unique_ptr<RivalBot>& b) {
                                   return !b->alive();
                               }),
                bots_.end());
}

void FreeRoamMode::updateCreatures(double dt) {
    // Wild creatures maul rival bots that get too close (bot-vs-bot).
    for (auto& cr : creatures_) {
        if (!cr->alive()) continue;
        for (const auto& b : bots_) {
            if (!b->alive()) continue;
            if (cr->position().distance(b->position()) < 4.0f) {
                ai::moveToward(*cr, b->position(), dt, 3.0f);
                b->takeDamage(10.0f * static_cast<float>(dt));
                break;
            }
        }
    }
}

void FreeRoamMode::update(double dt) {
    spawnTimer_ += dt;
    if (spawnTimer_ >= spawnInterval_) {
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
    // Wave 23: the free-roam population actually lives now — civilians
    // wander and flee, rival bots hunt, creatures retaliate.
    updateCivilians(dt);
    updateBots(dt);
    updateCreatures(dt);
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
