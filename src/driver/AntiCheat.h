#pragma once
// Wave 42: anti-cheat system.
//
// Detects macro/automation cheats via input analysis:
//  - Burst: too many commands too fast (inhuman typing).
//  - Regularity: identical commands at machine-precise intervals.
//  - Cooldown spam: hammering abilities that are on cooldown.
//
// Three strikes: each detection = disconnect from match + warning.
// 3 warnings = ban: progression wiped, multiplayer blocked (bot matches only).
//
// State persists in ~/.cultulhu/anticheat.dat (local, never in git).

#include <chrono>
#include <string>
#include <vector>

class AntiCheat {
public:
    AntiCheat();

    // Call for every player-issued command. Returns true if the command
    // is clean, false if a cheat was detected (caller should disconnect
    // and warn).
    bool recordCommand(const std::string& cmd);

    int warnings() const { return warnings_; }
    bool isBanned() const { return banned_; }

    // Issue a warning (disconnect + count). Returns true if this warning
    // triggers a ban (3rd strike).
    bool issueWarning();

    // Human-readable status for the `anticheat` command.
    std::string status() const;

private:
    struct CmdEvent {
        std::string cmd;
        std::chrono::steady_clock::time_point t;
    };
    std::vector<CmdEvent> history_; // last ~60 commands

    int warnings_ = 0;
    bool banned_ = false;

    static constexpr int kMaxWarnings = 3;
    // Burst: more than 15 commands in 3 seconds.
    static constexpr size_t kBurstCount = 15;
    static constexpr auto kBurstWindow = std::chrono::seconds(3);
    // Regularity: 8 identical commands, interval stddev < 50ms.
    static constexpr size_t kRegularCount = 8;
    static constexpr auto kRegularTolerance = std::chrono::milliseconds(50);

    bool checkBurst() const;
    bool checkRegularity() const;

    std::string dataPath() const;
    void load();
    void save() const;
    void wipeProgression();
};
