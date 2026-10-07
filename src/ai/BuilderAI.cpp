#include "ai/BuilderAI.h"

#include <algorithm>
#include <cmath>

namespace cultulhu {

BuilderAI::BuilderAI(EventBus& bus, RNG& rng, BeliefSystem& beliefs,
                     ExertionSystem& exertion, CultManager& cult)
    : bus_(bus),
      rng_(rng),
      beliefs_(beliefs),
      exertion_(exertion),
      cult_(cult),
      decisionInterval_(BUILD_DECISION_INTERVAL *
                        rng_.uniform(0.75f, 1.25f)) {}

bool BuilderAI::eligible(const Cultist* c) const {
    return c != nullptr && c->alive() &&
           c->state() == CultistState::Loyal &&
           c->devotion() >= LOYALTY_THRESHOLD;
}

bool BuilderAI::assigned(uint64_t cultistId) const {
    return builderToSite_.count(cultistId) != 0;
}

size_t BuilderAI::activeSitesNear(
    const std::vector<std::unique_ptr<ConstructionSite>>& sites,
    const Settlement& s) const {
    size_t n = 0;
    for (const auto& site : sites)
        if (!site->finished() &&
            site->position().distance(s.center) <= s.radius)
            ++n;
    return n;
}

bool BuilderAI::repairQueuedOrActive(
    uint64_t targetId, const Settlement& s,
    const std::vector<std::unique_ptr<ConstructionSite>>& sites) const {
    for (const auto& o : s.queue)
        if (o.repair && o.repairTargetId == targetId) return true;
    for (const auto& site : sites)
        if (site->isRepair() && site->repairTargetId() == targetId &&
            !site->finished())
            return true;
    return false;
}

void BuilderAI::emitBuild(EventType type, const ConstructionSite& site,
                          float amount) {
    GameEvent e(type);
    e.sourceId = site.id();
    e.tag = buildingTypeName(site.targetType());
    e.amount = amount;
    e.pos = site.position();
    bus_.publish(e);
}

BuildingType BuilderAI::pickBuildingType() {
    // Index by BuildingType order: Altar, Barracks, Wall, Watchtower, Trap,
    // Portal.
    float w[6] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    auto ex = [&](Belief b) { return exertion_.exertion(b); };
    if (ex(Belief::War) >= HIGH_EXERTION) {
        w[2] += 3.0f;
        w[1] += 3.0f; // Walls, Barracks
    }
    if (ex(Belief::Magic) >= HIGH_EXERTION) w[0] += 4.0f; // Altars
    if (ex(Belief::Breeding) >= HIGH_EXERTION) w[5] += 4.0f; // Portals
    if (ex(Belief::Trickery) >= HIGH_EXERTION) w[4] += 3.0f; // Traps
    if (ex(Belief::Reconstruction) >= HIGH_EXERTION) w[2] += 1.0f;
    // The cult's currently active beliefs sharpen the focus a little more.
    for (Belief b : beliefs_.active()) {
        switch (b) {
            case Belief::War: w[2] += 1.0f; w[1] += 1.0f; break;
            case Belief::Magic: w[0] += 1.0f; break;
            case Belief::Breeding: w[5] += 1.0f; break;
            case Belief::Trickery: w[4] += 1.0f; break;
            case Belief::Reconstruction: w[2] += 1.0f; break;
            default: break;
        }
    }
    float total = 0.0f;
    for (float x : w) total += x;
    float r = rng_.uniform(0.0f, total);
    for (int i = 0; i < 6; ++i) {
        r -= w[i];
        if (r <= 0.0f) return static_cast<BuildingType>(i);
    }
    return BuildingType::Wall;
}

void BuilderAI::decisionPass(
    std::vector<Cultist*>& cultists,
    std::vector<std::unique_ptr<ConstructionSite>>& sites,
    std::vector<Settlement>& settlements,
    const std::vector<Building*>& buildings) {
    // Index cultists by id for the release step.
    std::unordered_map<uint64_t, Cultist*> byId;
    for (Cultist* c : cultists)
        if (c) byId[c->id()] = c;

    // Release builders who died, turned, or lost heart.
    for (auto it = builderToSite_.begin(); it != builderToSite_.end();) {
        auto cit = byId.find(it->first);
        Cultist* c = (cit == byId.end()) ? nullptr : cit->second;
        if (!eligible(c)) {
            for (auto& site : sites)
                if (site->id() == it->second) {
                    site->removeBuilder(it->first);
                    break;
                }
            it = builderToSite_.erase(it);
        } else {
            ++it;
        }
    }

    // Unrest halts all new construction (sites already running stall when
    // their builders walk off, handled above).
    if (cult_.insurrectionRisk() >= UNREST_HALT_RISK) return;

    const float recon = exertion_.exertion(Belief::Reconstruction);
    const bool reconHigh = recon >= HIGH_EXERTION;

    for (Settlement& s : settlements) {
        // Idle, loyal-enough labor pool.
        std::vector<Cultist*> idle;
        for (Cultist* c : cultists)
            if (eligible(c) && !assigned(c->id())) idle.push_back(c);

        // Damaged friendly buildings inside this settlement.
        std::vector<Building*> damaged;
        for (Building* b : buildings)
            if (b && b->alive() && b->faction() == FACTION_CTHULHU &&
                b->state() == BuildingState::Damaged &&
                b->position().distance(s.center) <= s.radius)
                damaged.push_back(b);

        // Reconstruction burning hot: repairs jump the queue and new
        // builds wait until the damage is handled.
        if (reconHigh && !damaged.empty()) {
            for (Building* b : damaged) {
                if (repairQueuedOrActive(b->id(), s, sites)) continue;
                BuildOrder o;
                o.repair = true;
                o.repairTargetId = b->id();
                o.pos = b->position();
                o.type = b->buildingType();
                o.priority = 2.0f;
                s.queue.insert(s.queue.begin(), o);
            }
        } else if (!damaged.empty() && rng_.chance(0.25f)) {
            // Background upkeep even when Reconstruction is quiet.
            Building* b = damaged[static_cast<size_t>(
                rng_.intRange(0, static_cast<int>(damaged.size()) - 1))];
            if (!repairQueuedOrActive(b->id(), s, sites)) {
                BuildOrder o;
                o.repair = true;
                o.repairTargetId = b->id();
                o.pos = b->position();
                o.type = b->buildingType();
                s.queue.push_back(o);
            }
        }

        const bool damagePending = reconHigh && !damaged.empty();
        const size_t active = activeSitesNear(sites, s);

        // New build orders appear at random intervals while idle labor
        // exists and the settlement isn't already saturated.
        if (!damagePending && !idle.empty() &&
            s.queue.size() + active < MAX_ACTIVE_SITES_PER_SETTLEMENT &&
            rng_.chance(0.5f)) {
            BuildOrder o;
            o.type = pickBuildingType();
            const float ang = rng_.uniform(0.0f, 6.2831853f);
            const float rad =
                std::sqrt(rng_.uniform(0.0f, 1.0f)) * s.radius;
            o.pos = Vec3(s.center.x + std::cos(ang) * rad, s.center.y,
                         s.center.z + std::sin(ang) * rad);
            s.queue.push_back(o);
        }

        // Break ground on queued orders (needs at least one idle cultist).
        while (!s.queue.empty() &&
               activeSitesNear(sites, s) < MAX_ACTIVE_SITES_PER_SETTLEMENT &&
               !idle.empty()) {
            // Highest priority first; FIFO among equals.
            auto it = s.queue.begin();
            for (auto jt = s.queue.begin() + 1; jt != s.queue.end(); ++jt)
                if (jt->priority > it->priority) it = jt;
            BuildOrder o = *it;
            s.queue.erase(it);

            auto site = std::make_unique<ConstructionSite>(
                o.faction, o.pos, o.type, o.repairTargetId);
            emitBuild(EventType::BuildStarted, *site, 0.0f);
            progressDecile_[site->id()] = 0;

            Cultist* first = idle.back();
            idle.pop_back();
            site->addBuilder(first->id());
            builderToSite_[first->id()] = site->id();
            sites.push_back(std::move(site));
        }

        // Top up builders on running sites.
        for (auto& site : sites) {
            if (site->finished() || idle.empty()) break;
            if (site->position().distance(s.center) > s.radius) continue;
            while (site->builderCount() < MAX_BUILDERS_PER_SITE &&
                   !idle.empty()) {
                Cultist* c = idle.back();
                idle.pop_back();
                site->addBuilder(c->id());
                builderToSite_[c->id()] = site->id();
            }
        }
    }
}

void BuilderAI::pollSites(
    std::vector<std::unique_ptr<ConstructionSite>>& sites,
    const std::vector<Building*>& buildings) {
    std::vector<uint64_t> done;
    for (auto& site : sites) {
        const int decile =
            static_cast<int>(site->progress() * 10.0f + 0.5f);
        int& last = progressDecile_[site->id()]; // creates 0 on first sight
        if (decile > last && !site->finished()) {
            last = decile;
            emitBuild(EventType::BuildProgress, *site, site->progress());
        }
        if (!site->finished()) continue;
        if (last < 10) {
            last = 10;
            emitBuild(EventType::BuildProgress, *site, 1.0f);
        }
        emitBuild(EventType::BuildCompleted, *site, 1.0f);
        if (site->isRepair()) {
            for (Building* b : buildings) {
                if (b && b->id() == site->repairTargetId()) {
                    if (b->alive())
                        b->heal(b->maxHp());
                    else
                        b->revive(b->maxHp());
                    break;
                }
            }
        } else if (auto built = site->complete()) {
            finishedBuildings_.push_back(std::move(built));
        }
        for (auto it = builderToSite_.begin();
             it != builderToSite_.end();) {
            if (it->second == site->id())
                it = builderToSite_.erase(it);
            else
                ++it;
        }
        progressDecile_.erase(site->id());
        done.push_back(site->id());
    }
    if (!done.empty()) {
        sites.erase(
            std::remove_if(sites.begin(), sites.end(),
                           [&](const std::unique_ptr<ConstructionSite>& s) {
                               return std::find(done.begin(), done.end(),
                                                s->id()) != done.end();
                           }),
            sites.end());
    }
}

void BuilderAI::update(double dt, std::vector<Cultist*>& cultists,
                       std::vector<std::unique_ptr<ConstructionSite>>& sites,
                       std::vector<Settlement>& settlements,
                       const std::vector<Building*>& buildings) {
    // Advance running sites with the real dt first.
    for (auto& site : sites) site->update(dt);
    pollSites(sites, buildings);

    decisionTimer_ += dt;
    if (decisionTimer_ >= decisionInterval_) {
        decisionTimer_ = 0.0;
        decisionInterval_ =
            BUILD_DECISION_INTERVAL * rng_.uniform(0.75f, 1.25f);
        decisionPass(cultists, sites, settlements, buildings);
    }
}

std::vector<std::unique_ptr<Building>> BuilderAI::takeFinishedBuildings() {
    std::vector<std::unique_ptr<Building>> out;
    out.swap(finishedBuildings_);
    return out;
}

} // namespace cultulhu
