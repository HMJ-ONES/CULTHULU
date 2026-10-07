#include "ui/CommandMenu.h"

#include "commands/CommandSystem.h"
#include "commands/DirectiveExecutor.h"

#include <sstream>

namespace cultulhu {

const char* menuActionName(MenuAction a) {
    switch (a) {
        case MenuAction::Follow:          return "Follow";
        case MenuAction::AttackTarget:    return "AttackTarget";
        case MenuAction::CaptureOrder:    return "CaptureOrder";
        case MenuAction::SacrificeOrder:  return "SacrificeOrder";
        case MenuAction::ConvertOrder:    return "ConvertOrder";
        case MenuAction::MoveTo:          return "MoveTo";
        case MenuAction::RaidAt:          return "RaidAt";
        case MenuAction::BuildAltarAt:    return "BuildAltarAt";
        case MenuAction::BuildAt:         return "BuildAt";
        case MenuAction::ScoutAt:         return "ScoutAt";
        case MenuAction::RitualSacrifice: return "RitualSacrifice";
        case MenuAction::RitualConvert:   return "RitualConvert";
        case MenuAction::RitualNecromancy:return "RitualNecromancy";
        case MenuAction::Cancel:          return "Cancel";
    }
    return "Unknown";
}

const char* menuActionLabel(MenuAction a) {
    switch (a) {
        case MenuAction::Follow:          return "Follow me";
        case MenuAction::AttackTarget:    return "Attack target";
        case MenuAction::CaptureOrder:    return "Capture alive";
        case MenuAction::SacrificeOrder:  return "Sacrifice";
        case MenuAction::ConvertOrder:    return "Convert";
        case MenuAction::MoveTo:          return "Move here";
        case MenuAction::RaidAt:          return "Raid this area";
        case MenuAction::BuildAltarAt:    return "Build altar here";
        case MenuAction::BuildAt:         return "Build here";
        case MenuAction::ScoutAt:         return "Scout this area";
        case MenuAction::RitualSacrifice: return "Ritual: sacrifice";
        case MenuAction::RitualConvert:   return "Ritual: convert";
        case MenuAction::RitualNecromancy:return "Ritual: necromancy";
        case MenuAction::Cancel:          return "Cancel";
    }
    return "Unknown";
}

static MenuButton makeButton(MenuAction a, bool enabled = true,
                             const std::string& reason = "") {
    MenuButton b;
    b.action = a;
    b.label = menuActionLabel(a);
    b.enabled = enabled;
    b.disabledReason = reason;
    return b;
}

std::vector<MenuButton> CommandMenu::generateMenu(
        const MenuContext& ctx, const GameStateSummary& summary) const {
    std::vector<MenuButton> buttons;

    const bool noOrders = summary.commandableCultists <= 0;
    const bool opsFull = summary.activeOperations >= summary.maxOperations;

    switch (ctx.kind) {
        case MenuContext::Kind::OwnCultist:
            buttons.push_back(makeButton(MenuAction::Follow, !noOrders,
                noOrders ? "No cultists to command" : ""));
            buttons.push_back(makeButton(MenuAction::AttackTarget, !noOrders,
                noOrders ? "No cultists to command" : ""));
            buttons.push_back(makeButton(MenuAction::SacrificeOrder, !noOrders,
                noOrders ? "No cultists to command" : ""));
            buttons.push_back(makeButton(MenuAction::ConvertOrder, !noOrders,
                noOrders ? "No cultists to command" : ""));
            break;
        case MenuContext::Kind::Location:
            buttons.push_back(makeButton(MenuAction::MoveTo, !noOrders,
                noOrders ? "No cultists to command" : ""));
            buttons.push_back(makeButton(MenuAction::RaidAt, !noOrders && !opsFull,
                noOrders ? "No cultists to command"
                         : (opsFull ? "Directive executor full" : "")));
            buttons.push_back(makeButton(MenuAction::BuildAltarAt,
                !noOrders && !summary.hasAltarNearby,
                noOrders ? "No cultists to command"
                         : (summary.hasAltarNearby ? "Altar already nearby" : "")));
            buttons.push_back(makeButton(MenuAction::ScoutAt, !noOrders,
                noOrders ? "No cultists to command" : ""));
            break;
        case MenuContext::Kind::Enemy:
            buttons.push_back(makeButton(MenuAction::AttackTarget, !noOrders,
                noOrders ? "No cultists to command" : ""));
            buttons.push_back(makeButton(MenuAction::CaptureOrder, !noOrders,
                noOrders ? "No cultists to command" : ""));
            break;
        case MenuContext::Kind::Altar: {
            const bool rituals = summary.ritualsEnabled;
            buttons.push_back(makeButton(MenuAction::RitualSacrifice,
                rituals && summary.captivesHeld > 0,
                !rituals ? "Ritual site disrupted"
                         : (summary.captivesHeld <= 0 ? "No captives to sacrifice" : "")));
            buttons.push_back(makeButton(MenuAction::RitualConvert, rituals,
                rituals ? "" : "Ritual site disrupted"));
            buttons.push_back(makeButton(MenuAction::RitualNecromancy, rituals,
                rituals ? "" : "Ritual site disrupted"));
            break;
        }
        case MenuContext::Kind::Captive:
            buttons.push_back(makeButton(MenuAction::SacrificeOrder, !noOrders,
                noOrders ? "No cultists to command" : ""));
            buttons.push_back(makeButton(MenuAction::ConvertOrder, !noOrders,
                noOrders ? "No cultists to command" : ""));
            break;
    }

    buttons.push_back(makeButton(MenuAction::Cancel));
    return buttons;
}

bool CommandMenu::dispatch(const MenuButton& button, const MenuContext& ctx,
                           CommandSystem& cmds, DirectiveExecutor& exec) {
    (void)exec; // Executor spawns follow-through itself via DirectiveResolved.
    if (!button.enabled || button.action == MenuAction::Cancel) return false;

    switch (button.action) {
        // ---- Whole-cult directives (obedience roll + executor ops) ----
        case MenuAction::RaidAt:
            cmds.issueCommand(DirectiveType::RaidCity, ctx.pos);
            break;
        case MenuAction::AttackTarget:
            // Whole cult goes to war at the target's location; the
            // per-unit order below pins the actual attack on the entity.
            cmds.issueCommand(DirectiveType::GoToWar, ctx.pos);
            issued_.push_back({MenuAction::AttackTarget, ctx.entityId, ctx.pos});
            break;
        case MenuAction::ConvertOrder:
            cmds.issueCommand(DirectiveType::ConvertCampaign, ctx.pos);
            break;
        case MenuAction::RitualSacrifice:
            cmds.issueCommand(DirectiveType::MassSacrifice, ctx.pos);
            break;
        case MenuAction::RitualConvert:
            cmds.issueCommand(DirectiveType::ConvertCampaign, ctx.pos);
            break;
        // ---- Per-unit orders queued for the engine binding ----
        case MenuAction::Follow:
        case MenuAction::CaptureOrder:
        case MenuAction::SacrificeOrder:
        case MenuAction::MoveTo:
        case MenuAction::BuildAltarAt:
        case MenuAction::BuildAt:
        case MenuAction::ScoutAt:
        case MenuAction::RitualNecromancy:
            issued_.push_back({button.action, ctx.entityId, ctx.pos});
            break;
        case MenuAction::Cancel:
            return false;
    }
    return true;
}

std::string CommandMenu::renderText(
        const std::vector<MenuButton>& buttons) {
    std::ostringstream out;
    out << "== Command Menu ==\n";
    for (size_t i = 0; i < buttons.size(); ++i) {
        const auto& b = buttons[i];
        out << "  [" << i << "] " << b.label;
        if (!b.enabled) out << " (disabled: " << b.disabledReason << ")";
        out << "\n";
    }
    return out.str();
}

} // namespace cultulhu
