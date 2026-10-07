#include "cult/CultManager.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"

namespace cultulhu {

namespace {
// A conversion campaign takes ~300s of game time per soul at 1.0 efficiency.
constexpr double CAMPAIGN_SECONDS_PER_SOUL = 300.0;
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
        GameEvent e(EventType::Revolt);
        bus_.publish(e);
        return true;
    }
    return false;
}

} // namespace cultulhu
