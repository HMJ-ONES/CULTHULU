// CULT-ULHU micro-dev (2026-10-14): pin radial-menu reachability. Every
// MenuAction must appear as a button in at least one MenuContext — a
// fully-wired action (name + label + dispatch entry) that no context offers
// is a dead click: the player can never reach it. This caught
// MenuAction::BuildAt ("Build here"), which had a name, a label, and a
// dispatch entry queuing an IssuedOrder, but appeared in no context menu
// (now offered on Location clicks, next to BuildAltarAt).
//
// Test-only companion to the CommandMenu.cpp one-liner; no console lines
// (per the Oct 13 micro-dev direction).

#include "ui/CommandMenu.h"

#include <iostream>
#include <set>
#include <string>
#include <vector>

using namespace cultulhu;

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

int main() {
    // Every MenuAction value, enumerated explicitly (mirrors
    // tests_micro_menulabels.cpp). If the enum gains a member, add it here.
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
    static const MenuContext::Kind kKinds[] = {
        MenuContext::Kind::OwnCultist,
        MenuContext::Kind::Location,
        MenuContext::Kind::Enemy,
        MenuContext::Kind::Altar,
        MenuContext::Kind::Captive,
    };
    const size_t kNumActions = sizeof(kActions) / sizeof(kActions[0]);

    CommandMenu menu;

    // A healthy mid-game state: everything that can be enabled is enabled.
    GameStateSummary summary;
    summary.commandableCultists = 3;
    summary.captivesHeld = 2;
    summary.hasAltarNearby = false;
    summary.ritualsEnabled = true;
    summary.activeOperations = 0;
    summary.maxOperations = 4;

    // Union of all actions offered across every context.
    std::set<int> offered;
    for (size_t k = 0; k < sizeof(kKinds) / sizeof(kKinds[0]); ++k) {
        MenuContext ctx;
        ctx.kind = kKinds[k];
        ctx.entityId = 7;
        ctx.pos = Vec3{10.0f, 0.0f, 20.0f};
        const std::vector<MenuButton> buttons = menu.generateMenu(ctx, summary);
        CHECK(!buttons.empty());
        for (size_t b = 0; b < buttons.size(); ++b) {
            offered.insert(static_cast<int>(buttons[b].action));
            // Buttons render directly as click UI: every button needs text.
            CHECK(!buttons[b].label.empty());
        }
        // Cancel is the universal closer: present in every context.
        bool hasCancel = false;
        for (size_t b = 0; b < buttons.size(); ++b)
            if (buttons[b].action == MenuAction::Cancel) hasCancel = true;
        CHECK(hasCancel);
    }

    // Every action must be reachable from at least one context. A fully
    // wired action missing here is a dead click — fail loudly.
    for (size_t i = 0; i < kNumActions; ++i) {
        const int a = static_cast<int>(kActions[i]);
        if (offered.find(a) == offered.end()) {
            std::cout << "  UNREACHABLE MenuAction at index " << i
                      << " (" << menuActionName(kActions[i]) << ")\n";
        }
        CHECK(offered.find(a) != offered.end());
    }
    CHECK(offered.size() == kNumActions);

    // A starved state (no cultists to command) must not crash menu building.
    GameStateSummary empty;
    for (size_t k = 0; k < sizeof(kKinds) / sizeof(kKinds[0]); ++k) {
        MenuContext ctx;
        ctx.kind = kKinds[k];
        const std::vector<MenuButton> buttons = menu.generateMenu(ctx, empty);
        CHECK(!buttons.empty());
    }

    std::cout << "menucoverage: " << checks << " checks, "
              << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
