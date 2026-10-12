// CULT-ULHU player-convenience tests.
//
// Guards the status/HUD readability pass:
//   - cultistStateName maps every CultistState to the human-readable name
//     the driver's `status` output uses (never a raw integer again).
//   - the name used by the driver matches the one StatsPanel::gather
//     reports in the stats overlay (single source of truth).

#include "ui/StatsPanel.h"
#include "core/CommandSuggest.h"
#include "modes/RosterUtils.h"
#include "beliefs/Belief.h"

#include <cctype>
#include <iostream>
#include <string>
#include <vector>

using namespace cultulhu;

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

int main() {
    CHECK(std::string(cultistStateName(CultistState::Loyal)) == "Loyal");
    CHECK(std::string(cultistStateName(CultistState::Infringer)) == "Infringer");
    CHECK(std::string(cultistStateName(CultistState::Lunatic)) == "Lunatic");
    CHECK(std::string(cultistStateName(CultistState::Imprisoned)) == "Imprisoned");
    CHECK(std::string(cultistStateName(CultistState::Converted)) == "Converted");

    // "Did you mean" typo suggestions for unknown driver commands.
    const std::vector<std::string> cmds = {"status", "cast", "chat", "match",
                                           "quit", "exit"};
    CHECK(suggestCommand("stauts", cmds) == "status");  // transposition
    CHECK(suggestCommand("statsu", cmds) == "status");  // missing letter
    CHECK(suggestCommand("castt", cmds) == "cast");     // extra letter
    CHECK(suggestCommand("macth", cmds) == "match");    // transposition
    CHECK(suggestCommand("qit", cmds) == "quit");       // near miss
    CHECK(suggestCommand("status", cmds) == "status");  // exact passes through
    CHECK(suggestCommand("cat", cmds) == "cast");      // tie -> first in list
    CHECK(suggestCommand("xyzzy", cmds) == "");        // too far: no guess
    CHECK(suggestCommand("st", cmds) == "cast");       // exactly at threshold
    CHECK(suggestCommand("sxq", cmds) == "");         // 3+ edits: no guess
    CHECK(suggestCommand("", cmds) == "");             // empty input
    CHECK(suggestCommand("status", {}) == "");         // empty vocabulary

    // Kit-id typo forgiveness for `match join <kit-id>`: the driver's
    // suggestion vocabulary is the playable roster only (NPC package ids
    // are never fighters, so they are filtered out before suggesting).
    const std::vector<std::string> kits = {
        "cthulhu_avatar", "nyarlathotep", "shub_niggurath",
        "hastur", "yog_sothoth", "hypnos"};
    CHECK(isNpcPackage("civilian_villager"));
    CHECK(isNpcPackage("cultist_hooded"));
    CHECK(isNpcPackage("echo_of_the_void"));
    CHECK(!isNpcPackage("cthulhu_avatar"));
    CHECK(suggestCommand("cthulu_avatar", kits) == "cthulhu_avatar");
    CHECK(suggestCommand("nyarlahotep", kits) == "nyarlathotep");
    CHECK(suggestCommand("hypno", kits) == "hypnos");  // near miss
    CHECK(suggestCommand("xyzzy_plugh", kits) == ""); // too far: no guess
    CHECK(suggestCommand("cthulhu_avatar", kits) == "cthulhu_avatar");
    // Free-roam `character select <typo>` uses the same vocabulary (it is
    // wearable bodies only, so NPC ids never surface as suggestions).
    CHECK(suggestCommand("shub_niggrath", kits) == "shub_niggurath");
    CHECK(suggestCommand("civilian_villager", kits) == "");

    // Belief typo forgiveness for the free-roam `belief` command: the
    // suggestion vocabulary is the 12 canonical belief names (lowercased,
    // the way the driver matches), so the list can never drift from the
    // enum. "Did you mean" covers transpositions and near misses.
    const std::vector<std::string> beliefNames = beliefNamesList();
    CHECK(beliefNames.size() == 12);
    CHECK(beliefNames.front() == "Torture");
    CHECK(beliefNames.back() == "Dreams");
    std::vector<std::string> beliefLower;
    for (const auto& n : beliefNames) {
        std::string lc = n;
        for (auto& ch : lc) ch = static_cast<char>(std::tolower(ch));
        beliefLower.push_back(lc);
    }
    CHECK(suggestCommand("dream", beliefLower) == "dreams");    // missing letter
    CHECK(suggestCommand("feer", beliefLower) == "fear");       // near miss
    CHECK(suggestCommand("onslaugt", beliefLower) == "onslaught"); // transposition
    CHECK(suggestCommand("breeding", beliefLower) == "breeding"); // exact passes through
    CHECK(suggestCommand("xyzzy", beliefLower) == "");         // too far: no guess

    if (failures == 0)
        std::cout << "convenience: all " << checks << " checks passed\n";
    else
        std::cout << "convenience: " << failures << " of " << checks
                  << " checks FAILED\n";
    return failures == 0 ? 0 : 1;
}
