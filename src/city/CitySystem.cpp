#include "city/CitySystem.h"
#include "core/EventBus.h"

#include <utility>

namespace cultulhu {

float CityDistrict::ruin() const {
    float total = 0.0f, destroyed = 0.0f;
    for (const auto& b : buildings) {
        total += b.maxHp();
        destroyed += b.maxHp() - b.hp();
    }
    return total > 0.0f ? destroyed / total : 1.0f;
}

City::City(std::string name, Vec3 pos)
    : name_(std::move(name)), pos_(pos) {}

CityDistrict& City::addDistrict(std::string name, int population,
                                int buildings, float buildingHp) {
    CityDistrict d;
    d.name = std::move(name);
    d.population = population;
    for (int i = 0; i < buildings; ++i)
        d.buildings.emplace_back(FACTION_NEUTRAL, pos_, buildingHp);
    districts_.push_back(std::move(d));
    return districts_.back();
}

float City::totalRuin() const {
    float total = 0.0f, destroyed = 0.0f;
    for (const auto& d : districts_)
        for (const auto& b : d.buildings) {
            total += b.maxHp();
            destroyed += b.maxHp() - b.hp();
        }
    return total > 0.0f ? destroyed / total : 1.0f;
}

int City::population() const {
    int n = 0;
    for (const auto& d : districts_) n += d.population;
    return n;
}

CitySystem::CitySystem(EventBus& bus, float casualtyRate)
    : bus_(bus), casualtyRate_(casualtyRate) {}

City& CitySystem::foundCity(std::string name, Vec3 pos) {
    cities_.emplace_back(std::move(name), pos);
    return cities_.back();
}

void CitySystem::damageBuilding(City& city, size_t district, size_t building,
                                float dmg) {
    auto& ds = city.districts();
    if (district >= ds.size()) return;
    CityDistrict& d = ds[district];
    if (building >= d.buildings.size()) return;
    Building& b = d.buildings[building];
    if (!b.alive()) return;

    const float ruinBefore = d.ruin();
    const bool destroyed = b.takeDamage(dmg);
    const float ruinAfter = d.ruin();

    // Raids terrorize the city: destruction feeds the Fear belief.
    GameEvent raid(EventType::RaidPerformed);
    raid.amount = ruinAfter - ruinBefore; // 0..1 destruction from this hit
    raid.pos = b.position();
    bus_.publish(raid);

    if (destroyed) {
        // Collateral: part of the district's per-building population share
        // dies in the collapse; the rest flee.
        const int share = d.buildings.empty()
            ? 0
            : d.population / static_cast<int>(d.buildings.size());
        const int casualties =
            static_cast<int>(share * casualtyRate_);
        d.population -= casualties;
        if (d.population < 0) d.population = 0;

        if (casualties > 0) {
            GameEvent slain(EventType::CivilianSlain);
            slain.amount = static_cast<float>(casualties);
            slain.pos = b.position();
            bus_.publish(slain);
        }

        GameEvent e(EventType::CityBuildingDestroyed);
        e.amount = ruinAfter;
        e.pos = b.position();
        e.tag = d.name;
        bus_.publish(e);
    }

    if (d.razed() && !d.razedAnnounced) {
        d.razedAnnounced = true;
        d.population = 0; // nothing left to live in the rubble
        GameEvent e(EventType::DistrictRazed);
        e.tag = city.name() + "/" + d.name;
        e.pos = b.position();
        bus_.publish(e);
    }
}

} // namespace cultulhu
