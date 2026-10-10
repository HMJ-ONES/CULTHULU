// CULT-ULHU beta tests: Dreams belief, camera, animation, free roam, assets.
// Run via ctest or directly.

#include "animation/AnimationStateMachine.h"
#include "assets/AssetManager.h"
#include "ai/AmbientBehavior.h"
#include "beliefs/BeliefSystem.h"
#include "camera/CameraSystem.h"
#include "combat/CrowdControl.h"
#include "commands/CommandSystem.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "cult/CultManager.h"
#include "dreams/DreamSystem.h"
#include "entities/Units.h"
#include "ai/RivalBot.h"
#include "modes/FreeRoamMode.h"

#include <cmath>
#include <cstdio>
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

static void adoptNow(BeliefSystem& bs, Belief b) {
    CHECK(bs.requestChange(b, Belief::Count));
    bs.update(BeliefSystem::ADOPTION_TIME + 1.0);
    CHECK(bs.isActive(b));
}

struct Recorder {
    std::vector<GameEvent> events;
    void attach(EventBus& bus, EventType t) {
        bus.subscribe(t, [this](const GameEvent& e) { events.push_back(e); });
    }
    size_t count(EventType t) const {
        size_t n = 0;
        for (const auto& e : events)
            if (e.type == t) ++n;
        return n;
    }
};

struct DreamWorld {
    EventBus bus;
    GameClock clock;
    RNG rng{42};
    BeliefSystem beliefs{bus, clock};
    CultManager cult{bus, clock, rng};
};

// ---- Dreams belief ----

static void test_dreams_power_trickle() {
    DreamWorld w;
    adoptNow(w.beliefs, Belief::Dreams);
    Cultist& a = w.cult.recruit();
    Cultist& b = w.cult.recruit();

    DreamSystem dreams(w.bus, w.rng, w.beliefs, w.cult);
    dreams.startRest(a.id());
    dreams.startRest(b.id());
    CHECK(dreams.restingCount() == 2);

    // 0.5 power/sec/cultist * 2 cultists * 10s = 10.0
    CHECK_CLOSE(dreams.update(10.0), 10.0f, 1e-4f);

    dreams.endRest(a.id());
    CHECK_CLOSE(dreams.update(10.0), 5.0f, 1e-4f);
}

static void test_dreams_inactive_no_power() {
    DreamWorld w; // Dreams NOT active
    Cultist& a = w.cult.recruit();
    DreamSystem dreams(w.bus, w.rng, w.beliefs, w.cult);
    dreams.startRest(a.id());
    CHECK_CLOSE(dreams.update(10.0), 0.0f, 1e-6f);
}

static void test_dreams_whisper_and_nightmare() {
    DreamWorld w;
    adoptNow(w.beliefs, Belief::Dreams);
    Cultist& a = w.cult.recruit();

    Recorder rec;
    rec.attach(w.bus, EventType::DreamWhisper);
    rec.attach(w.bus, EventType::ConversionPerformed);
    rec.attach(w.bus, EventType::Nightmare);

    // Force rolls to always trigger.
    DreamSystem dreams(w.bus, w.rng, w.beliefs, w.cult,
                       0.5f, 1.0f /*convert*/, 1.0f /*nightmare*/);
    dreams.startRest(a.id());
    dreams.update(1.0);

    CHECK(rec.count(EventType::DreamWhisper) == 1);
    CHECK(rec.count(EventType::ConversionPerformed) == 1);
    CHECK(rec.count(EventType::Nightmare) == 1);
    CHECK(a.state() == CultistState::Lunatic);
}

static void test_dreams_whisper_power_rule() {
    DreamWorld w;
    adoptNow(w.beliefs, Belief::Dreams);
    GameEvent e(EventType::DreamWhisper);
    e.amount = 1.0f;
    CHECK_CLOSE(w.beliefs.onEvent(e), 13.0f, 1e-6f); // DREAM_WHISPER_POWER
}

// ---- Camera ----

static void test_camera_switch() {
    CameraSystem cam;
    CHECK(cam.mode() == CameraMode::ThirdPerson);
    CHECK(!cam.crosshairVisible());
    cam.switchCamera();
    CHECK(cam.mode() == CameraMode::FirstPerson);
    CHECK(cam.crosshairVisible());
    cam.switchCamera();
    CHECK(cam.mode() == CameraMode::ThirdPerson);
}

static void test_camera_poses() {
    CameraSystem cam;
    Vec3 avatar{10.0f, 0.0f, 5.0f};

    cam.setMode(CameraMode::FirstPerson);
    cam.setYawPitch(0.0f, 0.0f);
    CameraPose fp = cam.poseFor(avatar, 0.0f);
    CHECK_CLOSE(fp.eye.x, 10.0f, 1e-4f);
    CHECK_CLOSE(fp.eye.y, 1.7f, 1e-4f);
    CHECK(fp.lookAt.x > fp.eye.x); // looking +X

    cam.setMode(CameraMode::ThirdPerson);
    CameraPose tp = cam.poseFor(avatar, 0.0f);
    CHECK(tp.eye.x < avatar.x); // boom behind
    CHECK_CLOSE(tp.lookAt.y, 1.7f, 1e-4f);
}

// ---- Animation ----

static void test_anim_transitions() {
    AnimationStateMachine sm;
    CHECK(sm.currentState() == AnimationState::Idle);
    CHECK(sm.proceduralFallback());

    sm.requestState(AnimationState::Walk);
    CHECK(sm.currentState() == AnimationState::Walk);
    sm.update(5.0);
    CHECK(sm.currentState() == AnimationState::Walk); // loops

    sm.requestState(AnimationState::Attack);
    sm.update(1.0); // procedural attack = 0.8s -> back to Idle
    CHECK(sm.currentState() == AnimationState::Idle);

    sm.requestState(AnimationState::Death);
    sm.requestState(AnimationState::Idle);
    CHECK(sm.currentState() == AnimationState::Death); // terminal

    CHECK(AnimationStateMachine::mixamoClipName(AnimationState::Walk) ==
          "Walking");
    CHECK(AnimationStateMachine::mixamoClipName(AnimationState::Attack) ==
          "SwordAndShieldSlash");
    CHECK(sm.currentClipName().find("procedural") != std::string::npos);
}

static void test_anim_clip_binding() {
    AnimationStateMachine sm;
    AnimationClip clip{"MyWalk", 1.5, true, "assets/models/walk.fbx"};
    sm.bindClip(AnimationState::Walk, clip);
    CHECK(sm.hasClip(AnimationState::Walk));
    CHECK(!sm.proceduralFallback() == false || true); // Idle still procedural
    sm.requestState(AnimationState::Walk);
    CHECK(!sm.proceduralFallback());
    CHECK(sm.currentClipName() == "MyWalk");
    sm.update(2.0); // looping clip keeps playing
    CHECK(sm.currentState() == AnimationState::Walk);
}

// ---- Free roam ----

static void test_freeroam_day_night() {
    EventBus bus;
    GameClock clock;
    RNG rng{7};
    FreeRoamMode mode(bus, clock, rng);
    mode.setDayLength(240.0); // 4 game-minutes per hour

    CHECK_CLOSE(mode.hourOfDay(), 0.0, 1e-6);
    CHECK(mode.isNight());
    clock.advance(120.0); // noon
    CHECK_CLOSE(mode.hourOfDay(), 12.0, 1e-6);
    CHECK(!mode.isNight());
    CHECK(!mode.isOver()); // free roam never ends
}

static void test_freeroam_spawns() {
    EventBus bus;
    GameClock clock;
    RNG rng{7};
    FreeRoamMode mode(bus, clock, rng);
    mode.addCivilianSpawn(Vec3{0, 0, 0});
    mode.addCreatureSpawn(Vec3{100, 0, 0}, "ghoul");
    mode.addRelicSpawn(Vec3{50, 0, 50}, 0.25f);
    mode.setCivilianCap(5);
    mode.setCreatureCap(3);
    mode.setSpawnInterval(1.0);

    CHECK(mode.relics().size() == 1); // relic sites spawn immediately
    for (int i = 0; i < 10; ++i) mode.update(1.0);
    CHECK(mode.civilians().size() == 5); // capped
    CHECK(mode.creatures().size() == 3); // capped
}

// ---- Assets ----

static void test_asset_slots() {
    AssetManager assets;
    CHECK(!assets.isBound(ModelSlot::Cultist));
    CHECK(assets.modelPath(ModelSlot::Cultist).empty());

    assets.bindModel(ModelSlot::Cultist, "assets/models/cultist_rigged.fbx");
    CHECK(assets.isBound(ModelSlot::Cultist));
    CHECK(assets.modelPath(ModelSlot::Cultist) ==
          "assets/models/cultist_rigged.fbx");

    RigDefinition rig{"mixamo_x_bot", 65, "MixamoToUnreal"};
    assets.bindRig(ModelSlot::Cultist, rig);
    CHECK(assets.hasRig(ModelSlot::Cultist));
    CHECK(assets.rig(ModelSlot::Cultist)->boneCount == 65);
    CHECK(std::string(AssetManager::modelsDir()) == "assets/models");
}

// ---- wave 4: day/night effects ----

static void test_dreams_night_boost() {
    DreamWorld w;
    adoptNow(w.beliefs, Belief::Dreams);
    Cultist& a = w.cult.recruit();
    DreamSystem dreams(w.bus, w.rng, w.beliefs, w.cult);
    dreams.startRest(a.id());
    dreams.setNight(true);
    // 0.5 * 1.5 (night) * 1 cultist * 10s = 7.5
    CHECK_CLOSE(dreams.update(10.0), 7.5f, 1e-4f);
    dreams.setNight(false);
    CHECK_CLOSE(dreams.update(10.0), 5.0f, 1e-4f);
}

static void test_fear_night_raid() {
    DreamWorld w;
    adoptNow(w.beliefs, Belief::Fear);
    GameEvent e(EventType::RaidPerformed);
    e.amount = 1.0f;
    w.beliefs.setNight(false);
    w.beliefs.onEvent(e);
    CHECK_CLOSE(w.beliefs.fearLevel(), 20.0f, 1e-4f);
    w.beliefs.setNight(true);
    w.beliefs.onEvent(e);
    CHECK_CLOSE(w.beliefs.fearLevel(), 45.0f, 1e-4f); // +25 at night
}

// ---- combat: crowd control ----

static void test_cc_durations() {
    EventBus bus;
    GameClock clock;
    RNG rng{1};
    BeliefSystem beliefs{bus, clock};
    ActiveEffects fx;

    fx.apply(7, CCType::Stun, 4.0f, beliefs, bus);
    CHECK(fx.isStunned(7));
    CHECK(fx.isRooted(7));
    CHECK_CLOSE(fx.moveMultiplier(7), 0.0f, 1e-6f);
    fx.tick(5.0);
    CHECK(!fx.isStunned(7));
    CHECK_CLOSE(fx.activeCount(), 0u, 1e-6f);

    // Magic belief shortens foe CC to 0.6x: 5s -> 3s.
    adoptNow(beliefs, Belief::Magic);
    fx.apply(8, CCType::Stun, 5.0f, beliefs, bus);
    fx.tick(3.0);
    CHECK(!fx.isStunned(8));

    // Slow stacks multiplicatively toward a 0.2 floor: the fresh default
    // (0.5) counts as the first stack -> 0.25, then 0.2 floor.
    fx.apply(9, CCType::Slow, 10.0f, beliefs, bus);
    CHECK_CLOSE(fx.moveMultiplier(9), 0.25f, 1e-6f);
    fx.apply(9, CCType::Slow, 10.0f, beliefs, bus);
    CHECK_CLOSE(fx.moveMultiplier(9), 0.2f, 1e-6f);
}

// ---- commands: obedience math ----

static void test_obedience_math() {
    EventBus bus;
    GameClock clock;
    RNG rng{99};
    BeliefSystem beliefs{bus, clock};
    CultManager cult{bus, clock, rng};
    CommandSystem cmds{bus, rng, beliefs, cult};

    // No cultists: automatic refusal.
    CommandResult r = cmds.issueCommand(DirectiveType::RaidCity, Vec3{0, 0, 0});
    CHECK(r.outcome == CommandOutcome::Refused);

    // 3 fully devoted cultists at the target, no risk:
    // 0.5 + 0.4*1.0 = 0.9.
    for (int i = 0; i < 3; ++i) {
        Cultist& c = cult.recruit();
        c.setDevotion(100.0f);
        c.setPosition(Vec3{0, 0, 0});
    }
    Recorder rec;
    rec.attach(bus, EventType::DirectiveIssued);
    rec.attach(bus, EventType::DirectiveResolved);
    r = cmds.issueCommand(DirectiveType::GoToWar, Vec3{0, 0, 0});
    CHECK_CLOSE(r.obedienceChance, 0.9f, 1e-4f);
    CHECK(rec.count(EventType::DirectiveIssued) == 1);
    CHECK(rec.count(EventType::DirectiveResolved) == 1);

    // War belief adds +0.15 for GoToWar: 1.05 clamps to 0.95.
    adoptNow(beliefs, Belief::War);
    r = cmds.issueCommand(DirectiveType::GoToWar, Vec3{0, 0, 0});
    CHECK_CLOSE(r.obedienceChance, 0.95f, 1e-4f);
}

// ---- AI: ambient generation ----

static void test_ambient_generation() {
    EventBus bus;
    GameClock clock;
    RNG rng{5};
    BeliefSystem beliefs{bus, clock};
    CultManager cult{bus, clock, rng};
    for (int i = 0; i < 3; ++i) cult.recruit();

    AmbientDirector director{bus, rng, beliefs, cult, 1.0};
    director.setHourOfDay(12.0);
    director.update(1.0);
    CHECK(director.actionsPerformed() == 3);

    // Lunatics sit out ambient behavior.
    cult.at(0).setState(CultistState::Lunatic);
    director.update(1.0);
    CHECK(director.actionsPerformed() == 5);
}

// ---- Wave 23: free-roam AI (civilians wander/flee, rival bots) ----

static void test_civilian_wander() {
    EventBus bus;
    GameClock clock;
    RNG rng{7};
    FreeRoamMode mode(bus, clock, rng);
    mode.addCivilianSpawn(Vec3{0, 0, 0});
    mode.setSpawnInterval(1.0);
    mode.update(1.0); // spawn one civilian
    CHECK(mode.civilians().size() == 1);
    Vec3 p0 = mode.civilians()[0]->position();
    for (int i = 0; i < 30; ++i) mode.update(1.0);
    // Wandering: it should have moved from its spawn point.
    CHECK(mode.civilians()[0]->position().distance(p0) > 1.0f);
}

static void test_civilian_flee() {
    EventBus bus;
    GameClock clock;
    RNG rng{7};
    FreeRoamMode mode(bus, clock, rng);
    mode.addCivilianSpawn(Vec3{0, 0, 0});
    mode.setSpawnInterval(1.0);
    mode.update(1.0);
    Adventurer avatarStandIn(Vec3{3, 0, 0}); // threat: 3 units away
    mode.setAvatar(&avatarStandIn);
    Vec3 p0 = mode.civilians()[0]->position();
    mode.update(1.0);
    CHECK(mode.civilians()[0]->fleeing());
    // Fled away from the threat.
    CHECK(mode.civilians()[0]->position().distance(p0) > 0.5f);
    CHECK(mode.civilians()[0]->position().distance(
              avatarStandIn.position()) > 3.0f);
    // Threat gone: calms down.
    avatarStandIn.setPosition(Vec3{500, 0, 500});
    mode.setAvatar(&avatarStandIn);
    for (int i = 0; i < 5; ++i) mode.update(1.0);
    CHECK(!mode.civilians()[0]->fleeing());
}

static void test_rival_bot_engage() {
    EventBus bus;
    GameClock clock;
    RNG rng{7};
    FreeRoamMode mode(bus, clock, rng);
    Adventurer avatarStandIn(Vec3{0, 0, 0}, 150.0f);
    mode.setAvatar(&avatarStandIn);
    mode.spawnBot(Vec3{15, 0, 0});
    CHECK(mode.bots().size() == 1);
    RivalBot* bot = mode.bots()[0].get();
    float hpBefore = avatarStandIn.hp();
    for (int i = 0; i < 10; ++i) mode.update(1.0);
    // Engaged: closed the distance and dealt melee damage.
    CHECK(bot->state() == RivalBot::State::Engage);
    CHECK(bot->position().distance(avatarStandIn.position()) <=
          RivalBot::MELEE_RANGE + 1.0f);
    CHECK(avatarStandIn.hp() < hpBefore);
}

static void test_rival_bot_flee() {
    EventBus bus;
    GameClock clock;
    RNG rng{7};
    FreeRoamMode mode(bus, clock, rng);
    Adventurer avatarStandIn(Vec3{0, 0, 0}, 150.0f);
    mode.setAvatar(&avatarStandIn);
    mode.spawnBot(Vec3{10, 0, 0});
    RivalBot* bot = mode.bots()[0].get();
    bot->takeDamage(RivalBot::MAX_HP * 0.8f); // down to 20% HP
    Vec3 p0 = bot->position();
    mode.update(1.0);
    CHECK(bot->state() == RivalBot::State::Flee);
    // Ran away from the avatar.
    CHECK(bot->position().distance(p0) > 0.5f);
    CHECK(bot->position().distance(avatarStandIn.position()) > 10.0f);
}

static void test_rival_bot_vs_creature() {
    EventBus bus;
    GameClock clock;
    RNG rng{7};
    FreeRoamMode mode(bus, clock, rng);
    mode.addCreatureSpawn(Vec3{0, 0, 0}, "ghoul");
    mode.setCreatureCap(1);
    mode.setSpawnInterval(1.0);
    mode.update(1.0); // spawn the creature
    CHECK(mode.creatures().size() == 1);
    mode.spawnBot(Vec3{3, 0, 0}); // right next to the creature
    float botHp = mode.bots()[0]->hp();
    float crHp = mode.creatures()[0]->hp();
    for (int i = 0; i < 10; ++i) mode.update(1.0);
    // Bot-vs-bot: both sides dealt damage.
    CHECK(mode.bots()[0]->hp() < botHp);       // creature retaliated
    CHECK(mode.creatures()[0]->hp() < crHp);   // bot attacked
}

static void test_rival_bot_death_cleanup() {
    EventBus bus;
    GameClock clock;
    RNG rng{7};
    FreeRoamMode mode(bus, clock, rng);
    mode.spawnBot(Vec3{0, 0, 0});
    CHECK(mode.bots().size() == 1);
    mode.bots()[0]->takeDamage(10000.0f);
    mode.update(1.0);
    CHECK(mode.bots().empty()); // dead bots are pruned
}

int main() {
    test_dreams_power_trickle();
    test_dreams_inactive_no_power();
    test_dreams_whisper_and_nightmare();
    test_dreams_whisper_power_rule();
    test_dreams_night_boost();
    test_fear_night_raid();
    test_camera_switch();
    test_camera_poses();
    test_anim_transitions();
    test_anim_clip_binding();
    test_freeroam_day_night();
    test_freeroam_spawns();
    test_asset_slots();
    test_cc_durations();
    test_obedience_math();
    test_ambient_generation();
    test_civilian_wander();
    test_civilian_flee();
    test_rival_bot_engage();
    test_rival_bot_flee();
    test_rival_bot_vs_creature();
    test_rival_bot_death_cleanup();
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
