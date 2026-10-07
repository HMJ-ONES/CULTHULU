#pragma once

#include "entities/Entity.h"

namespace cultulhu {

enum class BuildingState { Intact, Damaged, Destroyed };

class Building : public Entity {
public:
    Building(FactionId faction, Vec3 pos, float maxHp = 1000.0f)
        : Entity(EntityType::Building, faction, pos, maxHp) {}

    BuildingState state() const;
    float rebuildProgress() const { return rebuildProgress_; } // 0..1

    // Reconstruction belief: damaged buildings rebuild over time without
    // mobilizing cultists. Call update(dt) each tick; when autoRebuild is
    // enabled the building repairs itself.
    void setAutoRebuild(bool b) { autoRebuild_ = b; }
    void update(double dt) override;

    bool takeDamage(float amount, bool oneHit = false) override;

private:
    float rebuildProgress_ = 0.0f;
    bool autoRebuild_ = false;
};

class Relic : public Entity {
public:
    // amplifier: e.g. 0.25 => +25% power gain while held/in territory.
    Relic(Vec3 pos, float amplifier)
        : Entity(EntityType::Relic, FACTION_NEUTRAL, pos, 50.0f),
          amplifier_(amplifier) {}

    float amplifier() const { return amplifier_; }

private:
    float amplifier_;
};

class Artifact : public Entity {
public:
    // Cursed artifacts can be left armed as traps for opponents.
    Artifact(Vec3 pos, bool cursed)
        : Entity(EntityType::Artifact, FACTION_NEUTRAL, pos, 50.0f),
          cursed_(cursed) {}

    bool cursed() const { return cursed_; }
    bool armed() const { return armed_; }
    void setArmed(bool a) { armed_ = a; }

private:
    bool cursed_ = false;
    bool armed_ = false;
};

} // namespace cultulhu
