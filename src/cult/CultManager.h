#pragma once

#include "core/Events.h"
#include "entities/Units.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace cultulhu {

class EventBus;
class GameClock;
class RNG;

// Owns the follower roster, runs conversion campaigns, and tracks
// insurrection risk (0..100). At high risk the cult revolts.
class CultManager {
public:
    static constexpr float REVOLT_THRESHOLD = 80.0f;

    CultManager(EventBus& bus, GameClock& clock, RNG& rng);

    Cultist& recruit();                 // add a loyal cultist
    // Remove the fallen. onErase (optional) fires once per erased cultist
    // with its id BEFORE the heap object is destroyed — the driver uses it
    // to untrack the cultist from the AnimationDirector, which holds raw
    // Entity pointers. Without this, a revolt (which kills and dismisses
    // inside update(), between director ticks) leaves the director with a
    // dangling pointer that its next tick() would dereference.
    void dismissDead(
        const std::function<void(uint64_t)>& onErase = nullptr);
    // Save/load support: drop the whole roster and cancel any active
    // campaign. Insurrection risk is left untouched (restore it separately).
    void clear();
    size_t size() const { return cultists_.size(); }
    Cultist& at(size_t i) { return *cultists_.at(i); }

    void markInfringer(Cultist& c) { c.setState(CultistState::Infringer); }

    float insurrectionRisk() const { return risk_; }
    void addRisk(float r);

    void setConversionEfficiency(float e) { conversionEff_ = e; }

    // Abstract conversion campaign: converts souls from the populace over
    // game time, then publishes ConversionPerformed. Efficiency scales the
    // campaign duration (Conversion belief).
    void startConversionCampaign(int targetConverts);
    bool campaignActive() const { return campaignActive_; }

    // Sacrifice belief: a Converted individual can deny the death of one of
    // Cthulhu's cultists by serving as a sacrifice in their place. 'dying'
    // must be an own cultist at 0 HP; it is restored to 25% HP while the
    // volunteer dies. Publishes DeathDenied. Returns false when no converted
    // volunteer is available.
    bool denyDeath(Cultist& dying);

    // Returns true if a revolt triggered this tick.
    bool update(double dt);

private:
    EventBus& bus_;
    GameClock& clock_;
    RNG& rng_;

    std::vector<std::unique_ptr<Cultist>> cultists_;
    float risk_ = 0.0f;
    float conversionEff_ = 1.0f;

    bool campaignActive_ = false;
    int campaignTarget_ = 0;
    double campaignRemaining_ = 0.0;
};

} // namespace cultulhu
