#include "cult/CultManager.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"

#include <algorithm>
#include <vector>

namespace cultulhu {

namespace {
// A conversion campaign takes ~300s of game time per soul at 1.0 efficiency.
constexpr double CAMPAIGN_SECONDS_PER_SOUL = 300.0;
// Wave 10 (R3): organic insurrection fuel. Insurrection previously only
// fired via punishment; a disaffected flock now stokes risk by itself.
// Kept a slow burn (+0.2/s): a revolt after ~4 min of sustained neglect in
// the worst case, not a chained alarm (the wave-9 sim flagged +0.5/s-class
// rates as revolt-chaining).
constexpr float LOW_DEVOTION_RISK_PER_SEC = 0.2f;
constexpr float DISAFFECTED_DEVOTION = 20.0f;
}

CultManager::CultManager(EventBus& bus, GameClock& clock, RNG& rng)
    : bus_(bus), clock_(clock), rng_(rng) {
    bus_.subscribe(EventType::InsurrectionRiskUp,
                   [this](const GameEvent& e) { addRisk(e.amount); });
}

Cultist& CultManager::recruit() {
    cultists_.push_back(std::make_unique<Cultist>(FACTION_CTHULHU, Vec3()));
    return *cultists_.back();
}

void CultManager::dismissDead() {
    for (auto it = cultists_.begin(); it != cultists_.end();) {
        if (!(*it)->alive()) it = cultists_.erase(it);
        else ++it;
    }
}

void CultManager::clear() {
    cultists_.clear();
    campaignActive_ = false;
    campaignTarget_ = 0;
    campaignRemaining_ = 0.0;
}

bool CultManager::denyDeath(Cultist& dying) {
    if (dying.faction() != FACTION_CTHULHU || dying.alive()) return false;

    Cultist* volunteer = nullptr;
    for (auto& c : cultists_) {
        if (c->state() == CultistState::Converted && c->alive()) {
            volunteer = c.get();
            break;
        }
    }
    if (!volunteer) return false;

    // The converted individual serves as the sacrifice.
    volunteer->takeDamage(volunteer->maxHp(), true);
    dying.revive(dying.maxHp() * 0.25f); // the death is denied

    GameEvent e(EventType::DeathDenied);
    e.sourceId = volunteer->id();
    e.targetId = dying.id();
    e.amount = 1.0f;
    bus_.publish(e);
    return true;
}

void CultManager::addRisk(float r) {
    risk_ += r;
    if (risk_ > 100.0f) risk_ = 100.0f;
    if (risk_ < 0.0f) risk_ = 0.0f;
}

void CultManager::startConversionCampaign(int targetConverts) {
    if (campaignActive_ || targetConverts <= 0) return;
    campaignActive_ = true;
    campaignTarget_ = targetConverts;
    campaignRemaining_ = CAMPAIGN_SECONDS_PER_SOUL * targetConverts / conversionEff_;
}

bool CultManager::update(double dt) {
    // Wave 10 (R3): disaffected cultists (devotion < 20) slowly stoke
    // insurrection risk on their own — once per tick, not per cultist.
    {
        bool disaffected = false;
        for (const auto& c : cultists_) {
            if (c->alive() && c->devotion() < DISAFFECTED_DEVOTION) {
                disaffected = true;
                break;
            }
        }
        if (disaffected)
            addRisk(LOW_DEVOTION_RISK_PER_SEC * static_cast<float>(dt));
    }
    if (campaignActive_) {
        campaignRemaining_ -= dt;
        if (campaignRemaining_ <= 0.0) {
            campaignActive_ = false;
            for (int i = 0; i < campaignTarget_; ++i) recruit();
            GameEvent e(EventType::ConversionPerformed);
            e.amount = static_cast<float>(campaignTarget_);
            e.tag = "campaign";
            bus_.publish(e);
        }
    }
    if (risk_ >= REVOLT_THRESHOLD) {
        risk_ = 30.0f; // the revolt burns itself out (tunable)
        // Wave 28: revolt has teeth — the least devoted quarter of the
        // cult deserts (lowest devotion first), each publishing CultistLost
        // so the belief power rules react naturally.
        std::vector<Cultist*> roster;
        for (auto& c : cultists_)
            if (c->alive() && c->state() != CultistState::Converted)
                roster.push_back(c.get());
        std::sort(roster.begin(), roster.end(),
                  [](const Cultist* a, const Cultist* b) {
                      return a->devotion() < b->devotion();
                  });
        size_t deserters = roster.size() / 4;
        if (!roster.empty() && deserters == 0) deserters = 1;
        for (size_t i = 0; i < deserters && i < roster.size(); ++i) {
            roster[i]->takeDamage(roster[i]->hp() + 1.0f); // gone, not dead
            GameEvent lost(EventType::CultistLost);
            lost.sourceId = roster[i]->id();
            lost.tag = "deserted in the revolt";
            bus_.publish(lost);
        }
        dismissDead();
        GameEvent e(EventType::Revolt);
        e.amount = static_cast<float>(deserters);
        bus_.publish(e);
        return true;
    }
    return false;
}

} // namespace cultulhu
