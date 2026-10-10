// CULT-ULHU wave 7 (Worker B) tests: command menu, input model, melee
// combos, stats panel.
// Covers: combo chaining / timeout / restart, stamina drain-exhaust-regen,
// contextual menu button sets + disabled cases, menu dispatch, InputManager
// WASD normalization / Tab toggle edge / Alt+LMB edge / jump buffer-coyote,
// and StatsPanel::gather filling counts and gauges.

#include "combat/Combos.h"
#include "input/InputManager.h"
#include "ui/CommandMenu.h"
#include "ui/StatsPanel.h"

#include "beliefs/BeliefSystem.h"
#include "commands/CommandSystem.h"
#include "commands/DirectiveExecutor.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "cult/CultManager.h"
#include "exertion/ExertionSystem.h"
#include "power/PowerSystem.h"

#include <cmath>
#include <iostream>
#include <vector>

using namespace cultulhu;

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

#define CHECK_CLOSE(a, b, eps) do { ++checks; if (std::fabs((a) - (b)) > (eps)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #a " (" << (a) \
    << ") != " #b " (" << (b) << ")\n"; } } while (0)

static const MenuButton* findButton(const std::vector<MenuButton>& buttons,
                                    MenuAction a) {
    for (const auto& b : buttons)
        if (b.action == a) return &b;
    return nullptr;
}

// ---------------- Combos ----------------

static void testComboChain() {
    ComboTracker t; // default basic_flurry, 1.2s window

    t.onLmbClick(0.0);
    CHECK(t.currentStage() == 1);
    t.onLmbClick(0.5);
    CHECK(t.currentStage() == 2);
    t.onLmbClick(1.0);
    CHECK(t.currentStage() == 3); // 3 clicks in window -> stage 3

    const ComboStage* s = t.stageDef();
    CHECK(s != nullptr);
    CHECK(s->animState == "Attack");
    CHECK_CLOSE(s->damageMult, 1.5f, 0.001f);
    CHECK(s->ccType == "Stun");
    CHECK_CLOSE(s->ccSeconds, 0.75f, 0.001f);
}

static void testComboTimeoutResets() {
    ComboTracker t;
    t.onLmbClick(0.0);
    t.onLmbClick(0.5);
    CHECK(t.currentStage() == 2);
    t.onLmbClick(5.0); // gap beyond the 1.2s window
    CHECK(t.currentStage() == 1); // chain resets to stage 1

    t.onLmbClick(5.4);
    t.update(10.0); // explicit timeout without another click
    CHECK(!t.active());
    CHECK(t.currentStage() == 0);
    CHECK(t.stageDef() == nullptr);
}

static void testComboEndRestarts() {
    ComboTracker t;
    t.onLmbClick(0.0);
    t.onLmbClick(0.5);
    t.onLmbClick(1.0);
    CHECK(t.currentStage() == 3);
    t.onLmbClick(1.5); // combo end -> fresh chain at stage 1
    CHECK(t.currentStage() == 1);
}

// ---------------- Stamina ----------------

static void testStaminaDrainExhaustRegen() {
    InputManager im;
    InputState sprint;
    sprint.shift.held = true;

    im.update(sprint, 1.0f); // 100 - 18*1
    CHECK_CLOSE(im.stamina().value, 82.0f, 0.01f);
    CHECK(!im.stamina().exhausted);
    CHECK(im.sprintHeld());

    im.update(sprint, 10.0f); // drains to zero
    CHECK_CLOSE(im.stamina().value, 0.0f, 0.01f);
    CHECK(im.stamina().exhausted);
    CHECK(!im.sprintHeld()); // exhausted blocks sprint

    InputState idle;
    im.update(idle, 1.0f); // regen 12
    CHECK_CLOSE(im.stamina().value, 12.0f, 0.01f);
    CHECK(im.stamina().exhausted); // still below recover threshold
    CHECK(!im.sprintHeld());

    im.update(idle, 2.0f); // 12 + 24 = 36 >= 30 -> recovered
    CHECK(!im.stamina().exhausted);
    im.update(sprint, 0.1f);
    CHECK(im.sprintHeld());
}

// ---------------- Command menu ----------------

static MenuContext ctxWith(MenuContext::Kind kind, uint64_t id = 0) {
    MenuContext c;
    c.kind = kind;
    c.entityId = id;
    c.pos = Vec3(10.0f, 0.0f, 20.0f);
    return c;
}

static GameStateSummary baseSummary() {
    GameStateSummary s;
    s.commandableCultists = 5;
    s.captivesHeld = 2;
    s.hasAltarNearby = false;
    s.ritualsEnabled = true;
    return s;
}

static void testMenuContexts() {
    CommandMenu menu;
    const GameStateSummary s = baseSummary();

    // OwnCultist -> Follow / AttackTarget / SacrificeOrder / ConvertOrder
    auto own = menu.generateMenu(ctxWith(MenuContext::Kind::OwnCultist, 7), s);
    CHECK(own.size() == 5); // 4 actions + Cancel
    CHECK(findButton(own, MenuAction::Follow) != nullptr);
    CHECK(findButton(own, MenuAction::AttackTarget) != nullptr);
    CHECK(findButton(own, MenuAction::SacrificeOrder) != nullptr);
    CHECK(findButton(own, MenuAction::ConvertOrder) != nullptr);
    CHECK(findButton(own, MenuAction::Cancel) != nullptr);
    CHECK(findButton(own, MenuAction::RaidAt) == nullptr);

    // Location -> MoveTo / RaidAt / BuildAltarAt / ScoutAt
    auto loc = menu.generateMenu(ctxWith(MenuContext::Kind::Location), s);
    CHECK(loc.size() == 5);
    CHECK(findButton(loc, MenuAction::MoveTo) != nullptr);
    CHECK(findButton(loc, MenuAction::RaidAt) != nullptr);
    CHECK(findButton(loc, MenuAction::BuildAltarAt) != nullptr);
    CHECK(findButton(loc, MenuAction::ScoutAt) != nullptr);

    // Enemy -> AttackTarget / CaptureOrder
    auto enemy = menu.generateMenu(ctxWith(MenuContext::Kind::Enemy, 42), s);
    CHECK(enemy.size() == 3);
    CHECK(findButton(enemy, MenuAction::AttackTarget) != nullptr);
    CHECK(findButton(enemy, MenuAction::CaptureOrder) != nullptr);

    // Altar -> three rituals
    auto altar = menu.generateMenu(ctxWith(MenuContext::Kind::Altar, 9), s);
    CHECK(altar.size() == 4);
    CHECK(findButton(altar, MenuAction::RitualSacrifice) != nullptr);
    CHECK(findButton(altar, MenuAction::RitualConvert) != nullptr);
    CHECK(findButton(altar, MenuAction::RitualNecromancy) != nullptr);
}

static void testMenuDisabledCases() {
    CommandMenu menu;

    // Altar already nearby -> BuildAltarAt disabled with a reason.
    GameStateSummary near = baseSummary();
    near.hasAltarNearby = true;
    auto loc = menu.generateMenu(ctxWith(MenuContext::Kind::Location), near);
    const MenuButton* build = findButton(loc, MenuAction::BuildAltarAt);
    CHECK(build != nullptr);
    CHECK(!build->enabled);
    CHECK(!build->disabledReason.empty());

    // No captives -> RitualSacrifice disabled.
    GameStateSummary noCapt = baseSummary();
    noCapt.captivesHeld = 0;
    auto altar = menu.generateMenu(ctxWith(MenuContext::Kind::Altar, 9), noCapt);
    const MenuButton* rit = findButton(altar, MenuAction::RitualSacrifice);
    CHECK(rit != nullptr);
    CHECK(!rit->enabled);

    // Nothing to command -> every order button disabled.
    GameStateSummary empty;
    auto own = menu.generateMenu(ctxWith(MenuContext::Kind::OwnCultist, 7), empty);
    for (const auto& b : own) {
        if (b.action == MenuAction::Cancel) {
            CHECK(b.enabled);
        } else {
            CHECK(!b.enabled);
        }
    }

    // Executor full -> RaidAt disabled.
    GameStateSummary full = baseSummary();
    full.activeOperations = 4;
    auto locFull = menu.generateMenu(ctxWith(MenuContext::Kind::Location), full);
    const MenuButton* raid = findButton(locFull, MenuAction::RaidAt);
    CHECK(raid != nullptr);
    CHECK(!raid->enabled);
}

static void testMenuDispatch() {
    EventBus bus;
    GameClock clock;
    RNG rng(1234);
    BeliefSystem beliefs(bus, clock);
    CultManager cult(bus, clock, rng);
    CommandSystem cmds(bus, rng, beliefs, cult);
    DirectiveExecutor exec(bus, rng, cult);

    CommandMenu menu;
    const GameStateSummary s = baseSummary();
    MenuContext c = ctxWith(MenuContext::Kind::Location);
    auto loc = menu.generateMenu(c, s);

    // Per-unit order path: MoveTo queues an IssuedOrder.
    const MenuButton* move = findButton(loc, MenuAction::MoveTo);
    CHECK(move != nullptr);
    CHECK(menu.dispatch(*move, c, cmds, exec));
    CHECK(menu.issuedOrders().size() == 1);
    CHECK(menu.issuedOrders()[0].action == MenuAction::MoveTo);
    CHECK_CLOSE(menu.issuedOrders()[0].pos.x, 10.0f, 0.001f);

    // Directive path: RaidAt issues RaidCity through CommandSystem (no crash,
    // obedience roll + executor follow-through handled by the bus).
    const MenuButton* raid = findButton(loc, MenuAction::RaidAt);
    CHECK(raid != nullptr);
    CHECK(menu.dispatch(*raid, c, cmds, exec));

    // Disabled button -> dispatch refuses.
    GameStateSummary near = baseSummary();
    near.hasAltarNearby = true;
    auto locNear = menu.generateMenu(c, near);
    const MenuButton* build = findButton(locNear, MenuAction::BuildAltarAt);
    CHECK(build != nullptr);
    menu.clearIssuedOrders();
    CHECK(!menu.dispatch(*build, c, cmds, exec));
    CHECK(menu.issuedOrders().empty());

    // Cancel is a no-op.
    const MenuButton* cancel = findButton(loc, MenuAction::Cancel);
    CHECK(cancel != nullptr);
    CHECK(!menu.dispatch(*cancel, c, cmds, exec));

    // renderText produces something printable for the driver.
    std::string text = CommandMenu::renderText(loc);
    CHECK(!text.empty());
    CHECK(text.find("Move here") != std::string::npos);
}

// ---------------- InputManager ----------------

static void testMoveVectorNormalization() {
    InputManager im;
    InputState s;
    s.w.held = true;
    s.d.held = true;
    im.update(s, 1.0f / 60.0f);
    Vec3 v = im.moveVector();
    CHECK_CLOSE(v.length(), 1.0f, 0.001f); // diagonal normalized, not sqrt(2)
    CHECK_CLOSE(v.x, 0.70710678f, 0.001f);
    CHECK_CLOSE(v.z, 0.70710678f, 0.001f);

    InputState back;
    back.s.held = true;
    im.update(back, 1.0f / 60.0f);
    Vec3 v2 = im.moveVector();
    CHECK_CLOSE(v2.z, -1.0f, 0.001f);
    CHECK_CLOSE(v2.x, 0.0f, 0.001f);

    InputState none;
    im.update(none, 1.0f / 60.0f);
    CHECK_CLOSE(im.moveVector().length(), 0.0f, 0.001f);
}

static void testTabToggleEdge() {
    InputManager im;
    InputState s;
    s.tab.held = true;
    im.update(s, 1.0f / 60.0f);
    CHECK(im.statsOverlayVisible()); // pressed -> toggled on

    im.update(s, 1.0f / 60.0f); // still held, no new edge
    CHECK(im.statsOverlayVisible());

    InputState up; // released...
    im.update(up, 1.0f / 60.0f);
    CHECK(im.statsOverlayVisible());

    im.update(s, 1.0f / 60.0f); // ...pressed again -> toggled off
    CHECK(!im.statsOverlayVisible());
}

static void testAltLmbEdge() {
    InputManager im;
    InputState s;
    s.alt.held = true;
    s.lmb.held = true;
    s.mouseX = 320.0f;
    s.mouseY = 240.0f;
    im.update(s, 1.0f / 60.0f);
    CHECK(im.altLeftClick());
    CHECK_CLOSE(im.menuRequestPos().x, 320.0f, 0.001f);
    CHECK_CLOSE(im.menuRequestPos().y, 240.0f, 0.001f);

    // LMB still held next frame -> no new edge, no new request.
    im.update(s, 1.0f / 60.0f);
    CHECK(!im.altLeftClick());

    // Alt alone does nothing.
    InputState altOnly;
    altOnly.alt.held = true;
    im.update(altOnly, 1.0f / 60.0f);
    CHECK(!im.altLeftClick());

    // LMB without Alt does nothing either.
    InputState lmbOnly;
    lmbOnly.lmb.held = true;
    im.update(lmbOnly, 1.0f / 60.0f);
    CHECK(!im.altLeftClick());
    CHECK(im.lmbHeld());
    CHECK(im.lmbLastClickTime() >= 0.0);
}

static void testJumpBufferAndCoyote() {
    InputManager im;

    // Space while grounded -> jump fires, buffer consumed.
    InputState jump;
    jump.space.held = true;
    im.update(jump, 1.0f / 60.0f);
    CHECK(im.jumpPressed());
    CHECK(!im.jumpPressed()); // consumed

    // Coyote time: leave the ground, then press within 0.12s.
    im.setGrounded(false);
    im.update(InputState{}, 1.0f / 60.0f); // release Space first (new edge)
    InputState airJump;
    airJump.space.held = true;
    im.update(airJump, 0.05f); // 0.05s after leaving the ground
    CHECK(im.jumpPressed());

    // Past the coyote window -> no jump.
    im.update(InputState{}, 0.5f);
    InputState lateJump;
    lateJump.space.held = true;
    im.update(lateJump, 1.0f / 60.0f);
    CHECK(!im.jumpPressed());
}

static void testAbilityCooldowns() {
    InputManager im;
    im.bindAbility(0, "tentacle_slam", 2.0f);

    InputState q;
    q.q.held = true;
    im.update(q, 1.0f / 60.0f);
    CHECK(im.abilityFired(0));

    // Still cooling down -> a second press does nothing.
    InputState up;
    im.update(up, 0.5f);
    im.update(q, 1.0f / 60.0f);
    CHECK(!im.abilityFired(0));

    // Cooldown expired -> fires again.
    im.update(up, 2.0f);
    im.update(q, 1.0f / 60.0f);
    CHECK(im.abilityFired(0));
}

static void testFindInteractable() {
    InputManager im;
    std::vector<Interactable> cands = {
        {1, Interactable::Kind::Altar, "E: perform ritual", Vec3(0, 0, 0)},
        {2, Interactable::Kind::Relic, "E: take relic", Vec3(2, 0, 0)},
    };
    const Interactable* near = im.findInteractable(Vec3(1.6f, 0, 0), cands);
    CHECK(near != nullptr);
    CHECK(near->entityId == 2);

    const Interactable* far = im.findInteractable(Vec3(100, 0, 0), cands);
    CHECK(far == nullptr);
}

// ---------------- StatsPanel ----------------

static void testStatsPanelGather() {
    EventBus bus;
    GameClock clock;
    RNG rng(99);
    BeliefSystem beliefs(bus, clock);
    CultManager cult(bus, clock, rng);
    PowerSystem power;
    ExertionSystem exertion(bus, beliefs, power, cult, rng);

    cult.recruit();
    cult.recruit();
    cult.recruit();
    cult.markInfringer(cult.at(0));

    power.add(50.0f); // 100 + 50 = 150
    exertion.addExertion(Belief::War, 25.0f);

    std::vector<net::KdaRow> rows = {
        {"Alice", 5, 2, 3},
        {"Bob", 3, 4, 1},
    };

    StatsData d = StatsPanel::gather(cult, exertion, power, rows);

    CHECK(d.cultCountsByState["Loyal"] == 2);
    CHECK(d.cultCountsByState["Infringer"] == 1);
    CHECK_CLOSE(d.power, 150.0f, 0.001f);
    CHECK_CLOSE(d.insurrectionRisk, cult.insurrectionRisk(), 0.001f);
    CHECK_CLOSE(d.beliefGauges[static_cast<int>(Belief::War)],
                exertion.exertion(Belief::War), 0.001f);
    CHECK(d.totalKills == 8); // 5 + 3, summed across rows
    CHECK(d.kdaTable.size() == 2);
    CHECK(d.kdaTable[0].name == "Alice");
    CHECK(d.kdaTable[0].deaths == 2);
    CHECK(d.kdaTable[1].assists == 1);
}

int main() {
    testComboChain();
    testComboTimeoutResets();
    testComboEndRestarts();
    testStaminaDrainExhaustRegen();
    testMenuContexts();
    testMenuDisabledCases();
    testMenuDispatch();
    testMoveVectorNormalization();
    testTabToggleEdge();
    testAltLmbEdge();
    testJumpBufferAndCoyote();
    testAbilityCooldowns();
    testFindInteractable();
    testStatsPanelGather();

    if (failures == 0) {
        std::cout << "wave7b: all " << checks << " checks passed\n";
        return 0;
    }
    std::cout << "wave7b: " << failures << " / " << checks << " checks FAILED\n";
    return 1;
}
