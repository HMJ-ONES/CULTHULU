// PlayerStatsTracker implementation.

#include "net/PlayerStats.h"

#include <set>

namespace cultulhu {

void PlayerStatsTracker::setPlayerName(size_t playerIdx,
                                       const std::string& name) {
    names_[playerIdx] = name;
}

void PlayerStatsTracker::recordDamage(size_t dealerPlayerIdx,
                                      uint64_t victimEntityId, float amount,
                                      double nowSec) {
    (void)amount;  // reserved for future damage stats
    auto& evs = damage_[victimEntityId];
    // Keep only events still inside the assist window: the vector stays
    // bounded no matter how long a match runs.
    std::vector<DamageEvent> kept;
    kept.reserve(evs.size() + 1);
    for (const auto& e : evs)
        if (nowSec - e.time <= kAssistWindowSec) kept.push_back(e);
    kept.push_back(DamageEvent{dealerPlayerIdx, nowSec});
    evs = std::move(kept);
}

void PlayerStatsTracker::recordKill(size_t killerPlayerIdx,
                                    size_t victimPlayerIdx) {
    stats_[killerPlayerIdx].kills++;
    stats_[victimPlayerIdx].deaths++;
    // Entity unknown: no assist lookup possible.
}

void PlayerStatsTracker::recordKill(size_t killerPlayerIdx,
                                    size_t victimPlayerIdx,
                                    uint64_t victimEntityId, double nowSec) {
    recordKill(killerPlayerIdx, victimPlayerIdx);
    auto it = damage_.find(victimEntityId);
    if (it == damage_.end()) return;
    // One assist per other recent damager (deduped: "once per kill").
    std::set<size_t> assisters;
    for (const auto& e : it->second) {
        if (e.dealer == killerPlayerIdx) continue;  // never self-assist
        if (nowSec - e.time <= kAssistWindowSec) assisters.insert(e.dealer);
    }
    for (size_t d : assisters) stats_[d].assists++;
    // The entity is dead: its damage history can never assist again.
    damage_.erase(it);
}

std::vector<net::KdaRow> PlayerStatsTracker::rows() const {
    // Union of named players and players with stats, in index order.
    std::set<size_t> idxs;
    for (const auto& [i, n] : names_) idxs.insert(i);
    for (const auto& [i, s] : stats_) idxs.insert(i);
    std::vector<net::KdaRow> out;
    out.reserve(idxs.size());
    for (size_t i : idxs) {
        net::KdaRow r;
        auto ni = names_.find(i);
        r.name = (ni != names_.end()) ? ni->second
                                     : "Player" + std::to_string(i);
        auto si = stats_.find(i);
        if (si != stats_.end()) {
            r.kills = si->second.kills;
            r.deaths = si->second.deaths;
            r.assists = si->second.assists;
        }
        out.push_back(std::move(r));
    }
    return out;
}

void PlayerStatsTracker::clear() {
    names_.clear();
    stats_.clear();
    damage_.clear();
}

} // namespace cultulhu
