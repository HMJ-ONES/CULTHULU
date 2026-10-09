#pragma once

#include "assets/ModelCatalog.h"
#include "entities/Entity.h"

#include <cstdint>
#include <memory>
#include <unordered_set>
#include <vector>

namespace cultulhu {

enum class BuildingState { Intact, Damaged, Destroyed };

// Wave 7: building kinds. Per-type effects (hooks for other systems; the
// core stays engine-agnostic):
//   Altar      ritual site (see Altar below); sacrifice/conversion/necromancy
//   Barracks   trains cultists faster (hook for the recruitment pipeline)
//   Wall       high-HP blocker; no active effect, just hard to destroy
//   Watchtower vision/range hook for the (future) perception system
//   Trap       arms the TrapSystem hook when completed
//   Portal     monster-summoning hook (Breeding belief synergy)
enum class BuildingType {
    Altar,
    Barracks,
    Wall,
    Watchtower,
    Trap,
    Portal
};

inline const char* buildingTypeName(BuildingType t) {
    switch (t) {
        case BuildingType::Altar:      return "Altar";
        case BuildingType::Barracks:   return "Barracks";
        case BuildingType::Wall:       return "Wall";
        case BuildingType::Watchtower: return "Watchtower";
        case BuildingType::Trap:       return "Trap";
        case BuildingType::Portal:     return "Portal";
    }
    return "Unknown";
}

// Wave 11: world-model path per building type, via the ModelCatalog's
// logical names (reconciled with assets/world/MANIFEST.md). Unknown
// types resolve to "" (engine falls back to procedural/blockout
// rendering).
inline std::string modelPathFor(BuildingType t) {
    switch (t) {
        case BuildingType::Altar:      return ModelCatalog::lookup("altar");
        case BuildingType::Barracks:   return ModelCatalog::lookup("shelter");
        case BuildingType::Wall:       return ModelCatalog::lookup("ruined_wall");
        case BuildingType::Watchtower: return ModelCatalog::lookup("collapsed_tower");
        case BuildingType::Trap:       return ModelCatalog::lookup("iron_fence");
        case BuildingType::Portal:     return ModelCatalog::lookup("ruined_arch");
    }
    return {};
}

// Default max HP per building type (tunable; see README wave 7 section).
inline float defaultHpFor(BuildingType t) {
    switch (t) {
        case BuildingType::Altar:      return 800.0f;
        case BuildingType::Barracks:   return 1500.0f;
        case BuildingType::Wall:       return 2500.0f;
        case BuildingType::Watchtower: return 600.0f;
        case BuildingType::Trap:       return 300.0f;
        case BuildingType::Portal:     return 1200.0f;
    }
    return 1000.0f;
}

class Building : public Entity {
public:
    // Legacy ctor keeps working; the type defaults to Wall (a plain
    // high-HP blocker, matching how generic buildings were used before).
    Building(FactionId faction, Vec3 pos, float maxHp = 1000.0f)
        : Entity(EntityType::Building, faction, pos, maxHp),
          buildingType_(BuildingType::Wall) {}

    // Wave 7: typed construction. maxHp <= 0 selects the per-type default.
    Building(FactionId faction, Vec3 pos, BuildingType type,
             float maxHp = 0.0f)
        : Entity(EntityType::Building, faction, pos,
                 maxHp > 0.0f ? maxHp : defaultHpFor(type)),
          buildingType_(type) {}

    BuildingType buildingType() const { return buildingType_; }
    void setBuildingType(BuildingType t) { buildingType_ = t; }

    // Wave 11: world-model path for this building's type (engine-agnostic
    // string; the Unreal/Unity binding resolves it to the actual mesh).
    // Empty when no art is bound for the type.
    std::string modelPath() const { return modelPathFor(buildingType_); }

    BuildingState state() const;
    float rebuildProgress() const { return rebuildProgress_; } // 0..1

    // Reconstruction belief: damaged buildings rebuild over time without
    // mobilizing cultists. Call update(dt) each tick; when autoRebuild is
    // enabled the building repairs itself.
    void setAutoRebuild(bool b) { autoRebuild_ = b; }
    void update(double dt) override;

    bool takeDamage(float amount, bool oneHit = false) override;

private:
    BuildingType buildingType_;
    float rebuildProgress_ = 0.0f;
    bool autoRebuild_ = false;
};

// Wave 7: ritual site for sacrifice / conversion / necromancy rituals.
// Derived from Building (like Monstrosity fixes up its EntityType, the ctor
// resets the type to EntityType::Altar), so altars get damage, destruction
// and Reconstruction auto-rebuild behavior for free while remaining a
// distinct entity kind. This is also what lets ConstructionSite::complete()
// hand back a real Altar as a unique_ptr<Building>.
class Altar : public Building {
public:
    static constexpr int MIN_TIER = 1;
    static constexpr int MAX_TIER = 3;

    Altar(FactionId faction, Vec3 pos)
        : Building(faction, pos, BuildingType::Altar) {
        // NOTE: type stays Building for the base ctor; fix it up here.
        type_ = EntityType::Altar;
    }

    int tier() const { return tier_; }

    // Raise the tier by one (clamped to MAX_TIER). Returns the new tier.
    // Publishes nothing by itself; the caller (or BuilderAI/driver) emits
    // AltarUpgraded.
    int upgrade() {
        if (tier_ < MAX_TIER) ++tier_;
        return tier_;
    }

    // Ritual effect multiplier: tier 1 -> 1.0x, tier 2 -> 1.5x, tier 3 -> 2.0x.
    float ritualPowerMult() const {
        return 1.0f + 0.5f * static_cast<float>(tier_ - MIN_TIER);
    }

    // Escort support: cultists can be ordered to escort captives to this
    // altar (the escort behavior itself lives in a later AI layer; the
    // altar just tracks who is assigned to it).
    void assignCaptive(uint64_t captiveId) { captives_.push_back(captiveId); }
    void unassignCaptive(uint64_t captiveId);
    const std::vector<uint64_t>& assignedCaptives() const { return captives_; }
    void clearCaptives() { captives_.clear(); }

private:
    int tier_ = MIN_TIER;
    std::vector<uint64_t> captives_;
};

// Wave 7: an in-progress building. Builders (cultist ids) attach to the
// site; update(dt) advances progress, and complete() hands back the
// finished Building once progress reaches 1. Repair sites (constructed
// with a repairTargetId) heal an existing building instead of spawning a
// new one: complete() returns nullptr for those and the caller applies the
// repair. The site itself is bus-free; BuilderAI polls progress and emits
// the BuildStarted/BuildProgress/BuildCompleted events.
class ConstructionSite : public Entity {
public:
    // One builder finishes a site in BUILD_TIME_SECONDS (tunable).
    static constexpr double BUILD_TIME_SECONDS = 120.0;

    ConstructionSite(FactionId faction, Vec3 pos, BuildingType target,
                     uint64_t repairTargetId = 0)
        : Entity(EntityType::Building, faction, pos, 200.0f),
          target_(target),
          repairTargetId_(repairTargetId) {}

    BuildingType targetType() const { return target_; }
    bool isRepair() const { return repairTargetId_ != 0; }
    uint64_t repairTargetId() const { return repairTargetId_; }

    float progress() const { return progress_; } // 0..1
    bool finished() const { return progress_ >= 1.0f; }

    void addBuilder(uint64_t cultistId) { builders_.insert(cultistId); }
    void removeBuilder(uint64_t cultistId) { builders_.erase(cultistId); }
    size_t builderCount() const { return builders_.size(); }
    bool hasBuilder(uint64_t cultistId) const {
        return builders_.count(cultistId) != 0;
    }
    // Wave 18: enumerate the attached builders (read-only). The animation
    // hook consumer resolves site ids from BuildStarted/Completed events
    // back to their builders through this.
    const std::unordered_set<uint64_t>& builderIds() const {
        return builders_;
    }

    // Progress scales linearly with builder count: n builders finish n
    // times faster than one. Sites with no builders stall.
    void update(double dt) override {
        if (finished() || builders_.empty()) return;
        progress_ += static_cast<float>(
            (static_cast<double>(builders_.size()) * dt) / BUILD_TIME_SECONDS);
        if (progress_ > 1.0f) progress_ = 1.0f;
    }

    // Build the finished entity. Returns nullptr for repair sites (the
    // caller heals repairTargetId instead) and after the first call.
    std::unique_ptr<Building> complete() {
        if (!finished() || completed_ || isRepair()) return nullptr;
        completed_ = true;
        if (target_ == BuildingType::Altar)
            return std::unique_ptr<Building>(new Altar(faction(), position()));
        return std::make_unique<Building>(faction(), position(), target_);
    }

private:
    BuildingType target_;
    uint64_t repairTargetId_ = 0;
    float progress_ = 0.0f;
    bool completed_ = false;
    std::unordered_set<uint64_t> builders_;
};

class Relic : public Entity {
public:
    // amplifier: e.g. 0.25 => +25% power gain while held/in territory.
    // Wave 26: relics carry names ("the Chalice of Gnawing Shadows") so
    // claiming one is a discovery moment, not a stat pickup.
    Relic(Vec3 pos, float amplifier, std::string name = "")
        : Entity(EntityType::Relic, FACTION_NEUTRAL, pos, 50.0f),
          amplifier_(amplifier),
          name_(std::move(name)) {}

    float amplifier() const { return amplifier_; }
    const std::string& name() const { return name_; }
    void setName(std::string n) { name_ = std::move(n); }

private:
    float amplifier_;
    std::string name_;
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
