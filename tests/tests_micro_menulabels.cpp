// CULT-ULHU micro-dev (2026-10-14): pin the radial command menu's
// engine-facing strings. The engine renders menuActionLabel() directly as
// the clickable button text, so every MenuAction must resolve to a real
// label — a future action added to the enum without a switch entry would
// otherwise silently render as "Unknown" in the click UI. Labels must also
// be distinct: two buttons with the same text are unclickable apart.
//
// Test-only change; no console lines (per the Oct 13 micro-dev direction).

#include "ui/CommandMenu.h"

#include <cstring>
#include <iostream>
#include <set>
#include <string>

using namespace cultulhu;

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

int main() {
    // Every MenuAction value, enumerated explicitly. If the enum gains a
    // member, add it here: the CHECKs below pin the whole set.
    static const MenuAction kActions[] = {
        MenuAction::Follow,
        MenuAction::AttackTarget,
        MenuAction::CaptureOrder,
        MenuAction::SacrificeOrder,
        MenuAction::ConvertOrder,
        MenuAction::MoveTo,
        MenuAction::RaidAt,
        MenuAction::BuildAltarAt,
        MenuAction::BuildAt,
        MenuAction::ScoutAt,
        MenuAction::RitualSacrifice,
        MenuAction::RitualConvert,
        MenuAction::RitualNecromancy,
        MenuAction::Cancel,
    };

    std::set<std::string> labels;
    std::set<std::string> names;
    for (size_t i = 0; i < sizeof(kActions) / sizeof(kActions[0]); ++i) {
        const MenuAction a = kActions[i];
        const char* name = menuActionName(a);
        const char* label = menuActionLabel(a);
        CHECK(name != nullptr);
        CHECK(label != nullptr);
        if (name && std::strcmp(name, "Unknown") == 0)
            std::cout << "  UNNAMED MenuAction at index " << i << "\n";
        if (label && std::strcmp(label, "Unknown") == 0)
            std::cout << "  UNLABELED MenuAction at index " << i << "\n";
        CHECK(std::strcmp(name, "Unknown") != 0);
        CHECK(std::strcmp(label, "Unknown") != 0);
        CHECK(std::strlen(label) > 0);
        // Labels are the clickable text: no duplicates.
        CHECK(labels.insert(label).second);
        CHECK(names.insert(name).second);
    }

    if (failures == 0)
        std::cout << "micro menulabels: " << checks << " checks passed\n";
    return failures == 0 ? 0 : 1;
}
