#pragma once

// Per-player kill/death/assist tracking for multiplayer matches (wave 7).
//
// The host simulation feeds damage and kill events into
// PlayerStatsTracker; it produces net::KdaRow snapshots for the Tab stats
// overlay (src/ui/StatsPanel) and the PlayerKda net message.
//
// Assist rule: when recordKill names the victim's entity id, every OTHER
// player who dealt damage to that entity within the last 10 seconds gets
// exactly one assist. The killer never assists on their own kill.

#include "net/Kda.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace cultulhu {

class PlayerStatsTracker {
public:
    // Damage older than this is forgotten for assist purposes.
    static constexpr double kAssistWindowSec = 10.0;

    void setPlayerName(size_t playerIdx, const std::string& name);

    // `dealerPlayerIdx` dealt damage to entity `victimEntityId` at
    // `nowSec`. Only remembered for assist lookup; amount is kept for a
    // future damage-stat extension and ignored today.
    void recordDamage(size_t dealerPlayerIdx, uint64_t victimEntityId,
                      float amount, double nowSec);

    // A kill happened. Prefer the 4-arg form: it also awards assists to
    // other recent damagers of the victim entity. The 2-arg form records
    // kill/death only (use it when the entity id is unknown, e.g.
    // environmental deaths).
    void recordKill(size_t killerPlayerIdx, size_t victimPlayerIdx);
    void recordKill(size_t killerPlayerIdx, size_t victimPlayerIdx,
                    uint64_t victimEntityId, double nowSec);

    // One row per known player (anyone with a name or stats), in
    // player-index order.
    std::vector<net::KdaRow> rows() const;

    void clear();

private:
    struct DamageEvent {
        size_t dealer = 0;
        double time = 0.0;
    };
    struct Stats {
        int kills = 0;
        int deaths = 0;
        int assists = 0;
    };

    std::map<size_t, std::string> names_;
    std::map<size_t, Stats> stats_;
    // entity id -> recent damage events against it (pruned to the window).
    std::map<uint64_t, std::vector<DamageEvent>> damage_;
};

} // namespace cultulhu
