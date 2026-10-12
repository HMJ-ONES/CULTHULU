// CULT-ULHU anti-cheat regression tests.
//
// The wave-42 anti-cheat (macro detection: burst, regularity, cooldown
// spam; three strikes -> ban) shipped with zero coverage, and recent
// experience says fresh untested paths are where the bugs live.
// These pin the player-relevant behavior:
//
// 1. A fresh player is clean; a few varied commands never trigger.
// 2. Burst: 15+ commands inside 3 seconds is flagged (inhuman typing).
// 3. Regularity: 8 identical machine-timed commands are flagged.
// 4. Three strikes escalate: warning 1, warning 2, then BAN (third
//    strike returns true, isBanned() flips, status names the ban).
// 5. A banned player records no further detections.
// 6. Warnings + ban persist across instances (the ~/.cultulhu file).
//
// HOME is redirected to a temp dir so the real player's
// ~/.cultulhu/anticheat.dat is never touched.

#include "driver/AntiCheat.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <unistd.h>

static int failures = 0;
#define CHECK(cond)                                        \
    do {                                                   \
        if (!(cond)) {                                     \
            std::cout << "FAIL line " << __LINE__ << ": "  \
                      << #cond << "\n";                     \
            ++failures;                                    \
        }                                                  \
    } while (0)

static void isolate_home(char* dir, std::string& oldHome) {
    const char* h = std::getenv("HOME");
    oldHome = h ? h : "";
    setenv("HOME", dir, 1);
}

static void restore_home(const std::string& oldHome) {
    if (oldHome.empty()) unsetenv("HOME");
    else setenv("HOME", oldHome.c_str(), 1);
}

static void anticheat_checks() {
    char tmpl[] = "/tmp/cultulhu_ac_test_XXXXXX";
    CHECK(mkdtemp(tmpl) != nullptr);
    std::string oldHome;
    isolate_home(tmpl, oldHome);

    // ---------- fresh player: clean, no false positives ----------
    {
        AntiCheat ac;
        CHECK(!ac.isBanned());
        CHECK(ac.warnings() == 0);
        CHECK(ac.recordCommand("status"));
        CHECK(ac.status().find("clean") != std::string::npos);
        // A handful of varied commands: below every threshold.
        bool allClean = true;
        const char* cmds[] = {"move n", "cast q", "look", "status",
                              "belief fear", "move s", "cast f", "chars"};
        for (const char* c : cmds) allClean &= ac.recordCommand(c);
        CHECK(allClean);
        CHECK(!ac.isBanned());
    }

    // ---------- burst: 15+ commands inside 3 seconds flags ----------
    {
        AntiCheat ac;
        bool flagged = false;
        for (int i = 0; i < 20; ++i) {
            // Distinct commands so regularity can't be the cause.
            if (!ac.recordCommand("move " + std::to_string(i))) {
                flagged = true;
                break;
            }
        }
        CHECK(flagged); // a tight loop of 20 commands is inhuman
    }

    // ---------- regularity: 8 identical machine-timed commands flag ----------
    {
        AntiCheat ac;
        bool flagged = false;
        for (int i = 0; i < 8; ++i) {
            if (!ac.recordCommand("cast q")) {
                flagged = true;
                break;
            }
        }
        CHECK(flagged); // ~0ms gaps are faster than any human
    }

    // ---------- three strikes: warn, warn, ban ----------
    {
        AntiCheat ac;
        CHECK(!ac.issueWarning()); // strike 1: warn, not banned
        CHECK(ac.warnings() == 1);
        CHECK(!ac.isBanned());
        CHECK(ac.status().find("1/3") != std::string::npos);
        CHECK(!ac.issueWarning()); // strike 2
        CHECK(ac.warnings() == 2);
        CHECK(ac.status().find("2/3") != std::string::npos);
        CHECK(ac.issueWarning()); // strike 3: ban
        CHECK(ac.isBanned());
        CHECK(ac.status().find("BANNED") != std::string::npos);
        // Banned players record no further detections.
        CHECK(ac.recordCommand("cast q"));
    }

    // ---------- persistence: warnings + ban survive across instances ----------
    {
        // The previous block saved warnings=3, banned=1 into tmpl.
        AntiCheat ac;
        CHECK(ac.warnings() == 3);
        CHECK(ac.isBanned());
    }

    restore_home(oldHome);
}

int main() {
    anticheat_checks();
    if (failures == 0) std::cout << "anticheat: all checks passed\n";
    else std::cout << "anticheat: " << failures << " checks FAILED\n";
    return failures == 0 ? 0 : 1;
}
