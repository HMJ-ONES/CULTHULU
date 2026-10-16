#pragma once

#include "core/Events.h"
#include "entities/Structures.h"

#include <cstddef>
#include <string>
#include <vector>

namespace cultulhu {

class EventBus;

// A district of a city under siege: its civilian population, its buildings,
// and how much of it lies in ruins (0..1 destroyed-HP fraction).
struct CityDistrict {
    std::string name;
    int population = 0; // civilians still living here
    std::vector<Building> buildings;

    float ruin() const; // 0..1 fraction of total building HP destroyed
    bool razed() const { return ruin() >= 1.0f; }
    bool razedAnnounced = false; // DistrictRazed fires once per district
};

// A neutral city the player's forces can raid and destroy. City buildings
// do not auto-rebuild: destruction is permanent, as befits a cosmic horror.
class City {
public:
    City(std::string name, Vec3 pos);

    const std::string& name() const { return name_; }
    const std::vector<CityDistrict>& districts() const { return districts_; }
    std::vector<CityDistrict>& districts() { return districts_; }

    // Add a district with 'buildings' city structures of 'buildingHp' each.
    CityDistrict& addDistrict(std::string name, int population,
                             int buildings, float buildingHp = 1000.0f);

    float totalRuin() const; // ruin across all districts, 0..1
    int population() const;  // civilians remaining across all districts

    bool destroyedAnnounced = false; // CityDestroyed fires once per city

private:
    std::string name_;
    Vec3 pos_;
    std::vector<CityDistrict> districts_;
};

// Tracks siege damage against cities. Damaging city buildings:
//  - publishes RaidPerformed (destruction = district ruin), which the Fear
//    belief turns into fear level and Cthulhu power;
//  - publishes CivilianSlain (casualties), which the Onslaught belief
//    converts into power;
//  - publishes CityBuildingDestroyed and, once per district, DistrictRazed.
class CitySystem {
public:
    // casualtyRate: fraction of a district's population killed per destroyed
    // building share (0.5 = half the per-building share dies, rest flee).
    explicit CitySystem(EventBus& bus, float casualtyRate = 0.5f);

    City& foundCity(std::string name, Vec3 pos);

    // Deal siege damage to one building. No-ops on bad indices or if the
    // building is already destroyed.
    void damageBuilding(City& city, size_t district, size_t building,
                        float dmg);

    size_t cityCount() const { return cities_.size(); }

private:
    EventBus& bus_;
    float casualtyRate_;
    std::vector<City> cities_;
};

} // namespace cultulhu
