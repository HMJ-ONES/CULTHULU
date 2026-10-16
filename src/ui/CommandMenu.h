#pragma once

// Wave 7 (Worker B): Alt+left-click radial command menu — UI-agnostic data
// model plus dispatch. The engine renders the menu; the trigger
// "Alt+left-click" is an engine concern (see InputManager::altLeftClick()).
//
// Contexts (what was clicked) map to contextual button sets; dispatch()
// maps each button onto either a whole-cult directive (through
// CommandSystem, which rolls obedience and spawns DirectiveExecutor
// follow-through automatically) or a concrete per-unit order queued in the
// menu's IssuedOrder log for the engine binding to consume (move, attack,
// capture, sacrifice, convert, build, scout, necromancy...).

#include "core/Vec3.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cultulhu {

class CommandSystem;
class DirectiveExecutor;

// Every action the radial menu can offer.
enum class MenuAction {
    Follow,          // selected cultist shadows the player
    AttackTarget,    // attack the entity under the cursor
    CaptureOrder,    // capture the enemy alive
    SacrificeOrder,  // sacrifice a captive / own cultist
    ConvertOrder,    // convert the target
    MoveTo,          // move cultist(s) to the clicked location
    RaidAt,          // whole-cult raid on the clicked location
    BuildAltarAt,    // construct an altar at the clicked location
    BuildAt,         // generic construction at the clicked location
    ScoutAt,         // scout the clicked location
    RitualSacrifice, // altar ritual: sacrifice
    RitualConvert,   // altar ritual: conversion
    RitualNecromancy,// altar ritual: necromancy
    Cancel           // close the menu, no order
};

const char* menuActionName(MenuAction a);
const char* menuActionLabel(MenuAction a);

// What the Alt+left-click landed on.
struct MenuContext {
    enum class Kind {
        OwnCultist, // one of our own cultists
        Location,   // empty ground / map point
        Enemy,      // hostile entity
        Altar,      // one of our altars
        Captive    // a captive we hold
    };

    Kind kind = Kind::Location;
    uint64_t entityId = 0; // OwnCultist/Enemy/Captive/Altar id, 0 when N/A
    Vec3 pos;              // click position in world space
};

// Snapshot of game state the menu needs to enable/disable buttons. The
// engine fills this in before calling generateMenu(); the menu itself does
// no world queries, keeping it engine-agnostic.
struct GameStateSummary {
    int commandableCultists = 0; // cultists that can receive orders
    int captivesHeld = 0;        // captives available for sacrifice
    bool hasAltarNearby = false;   // altar already within build radius
    bool ritualsEnabled = true; // false while a ritual site is disrupted
    size_t activeOperations = 0; // live DirectiveExecutor operations
    size_t maxOperations = 4;    // DirectiveExecutor::MAX_OPERATIONS
};

struct MenuButton {
    MenuAction action;
    std::string label;
    bool enabled = true;
    std::string disabledReason; // set when !enabled
};

// A concrete per-unit order that has no whole-cult directive equivalent.
// Dispatch queues these here; the engine binding consumes issuedOrders(),
// applies them to units, then clears the queue with clearIssuedOrders().
struct IssuedOrder {
    MenuAction action;
    uint64_t entityId = 0;
    Vec3 pos;
};

class CommandMenu {
public:
    // Build the contextual button set for this click. Always ends with a
    // Cancel button. Buttons may be disabled with a reason (e.g.
    // BuildAltarAt when hasAltarNearby, RitualSacrifice with no captives).
    std::vector<MenuButton> generateMenu(const MenuContext& ctx,
                                         const GameStateSummary& summary) const;

    // Dispatch a chosen button: issues whole-cult directives for the ones
    // that map (RaidAt -> RaidCity, AttackTarget-on-Enemy -> GoToWar,
    // RitualSacrifice -> MassSacrifice, ConvertOrder/RitualConvert ->
    // ConvertCampaign) and queues IssuedOrders for the per-unit ones.
    // Cancel is a no-op. Disabled buttons are ignored (returned false).
    bool dispatch(const MenuButton& button, const MenuContext& ctx,
                  CommandSystem& cmds, DirectiveExecutor& exec);

    const std::vector<IssuedOrder>& issuedOrders() const { return issued_; }
    void clearIssuedOrders() { issued_.clear(); }

    // ASCII radial listing for driver testing (engine renders the real UI).
    static std::string renderText(const std::vector<MenuButton>& buttons);

private:
    std::vector<IssuedOrder> issued_;
};

} // namespace cultulhu
