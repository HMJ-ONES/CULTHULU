// CULT-ULHU wave 18 tests: behavior-hook animation states, the
// AnimationDirector, sacrifice event ids, the Maul hook, the glTF embedded
// animation extractor, and package .canim clip binding. Headless only.

#include "ai/AI.h"
#include "animation/AnimationDirector.h"
#include "animation/AnimationStateMachine.h"
#include "animation/ProceduralClips.h"
#include "assets/GlbAnimExtractor.h"
#include "combat/Combat.h"
#include "core/EventBus.h"
#include "core/Events.h"
#include "entities/Units.h"
#include "rituals/Ritual.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

using namespace cultulhu;

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

#define CHECK_CLOSE(a, b, eps) do { ++checks; if (std::fabs((a) - (b)) > (eps)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #a " (" << (a) \
    << ") != " #b " (" << (b) << ")\n"; } } while (0)

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

static std::string repoPrefix() {
    std::ifstream p("assets/creatures/dagon_spawn.glb");
    if (p) return "";
    return "../";
}

int main() {
    // ---------- Task 1: one-shot auto-return (Brawl/Maul/CastWave/Launch) --
    {
        for (AnimationState s :
             {AnimationState::Brawl, AnimationState::Maul,
              AnimationState::CastWave, AnimationState::Launch,
              AnimationState::Attack, AnimationState::Cast}) {
            AnimationStateMachine sm;
            bindProceduralFallbacks(sm);
            sm.requestState(s, 0.0);
            CHECK(sm.currentState() == s);
            sm.update(5.0); // past every one-shot duration
            CHECK(sm.currentState() == AnimationState::Idle);
        }
        // Channel: the procedural clip LOOPS by design ("loops until the
        // channel ends"), so a bound Channel correctly holds. The
        // auto-return still covers the unbound procedural-fallback path.
        {
            AnimationStateMachine sm; // nothing bound
            sm.requestState(AnimationState::Channel, 0.0);
            sm.update(5.0); // past the 3.0s fallback duration
            CHECK(sm.currentState() == AnimationState::Idle);
        }
        // Looping states must NOT auto-return.
        AnimationStateMachine sm;
        bindProceduralFallbacks(sm);
        sm.requestState(AnimationState::FearRun, 0.0);
        sm.update(5.0);
        CHECK(sm.currentState() == AnimationState::FearRun);
        sm.requestState(AnimationState::SacrificePerformer, 0.0);
        sm.update(60.0);
        CHECK(sm.currentState() == AnimationState::SacrificePerformer);
    }
    // ---------- Task 2: entities hold an AnimationStateMachine ------------
    {
        Civilian c(Vec3{0, 0, 0});
        CHECK(c.anim().currentState() == AnimationState::Idle);
        c.anim().requestState(AnimationState::FearRun);
        CHECK(c.anim().currentState() == AnimationState::FearRun);
        // Default Entity::update advances the machine.
        c.anim().requestState(AnimationState::Brawl, 0.0);
        bindProceduralFallbacks(c.anim());
        c.update(5.0);
        CHECK(c.anim().currentState() == AnimationState::Idle);
    }
    // ---------- Task 3a: FearRun on flee, Idle on calm ------------------
    {
        Civilian c(Vec3{0, 0, 0});
        bindProceduralFallbacks(c.anim());
        ai::civilianFlee(c, Vec3{10, 0, 0}, 1.0);
        CHECK(c.anim().currentState() == AnimationState::FearRun);
        ai::civilianCalm(c);
        CHECK(c.anim().currentState() == AnimationState::Idle);
        // Calm is safe anytime: no-op on Idle, never overrides Death.
        ai::civilianCalm(c);
        CHECK(c.anim().currentState() == AnimationState::Idle);
        c.takeDamage(999.0f);
        c.anim().requestState(AnimationState::Death);
        ai::civilianCalm(c);
        CHECK(c.anim().currentState() == AnimationState::Death);
    }
    // ---------- Task 3b: Brawl consumer (both brawlers) ------------------
    {
        EventBus bus;
        AnimationDirector dir(bus);
        Cultist a(FACTION_CTHULHU, Vec3{0, 0, 0});
        Cultist b(FACTION_CTHULHU, Vec3{1, 0, 0});
        for (Cultist* c : {&a, &b}) {
            bindProceduralFallbacks(c->anim());
            dir.track(*c);
        }
        GameEvent e(EventType::BrawlBrokeOut);
        e.sourceId = a.id();
        e.targetId = b.id();
        bus.publish(e);
        CHECK(a.anim().currentState() == AnimationState::Brawl);
        CHECK(b.anim().currentState() == AnimationState::Brawl);
        // Brawl is a 0.9s one-shot: the tick auto-returns both to Idle.
        dir.tick(1.0);
        CHECK(a.anim().currentState() == AnimationState::Idle);
        CHECK(b.anim().currentState() == AnimationState::Idle);
        // Unknown ids are ignored, never crash.
        GameEvent e2(EventType::BrawlBrokeOut);
        e2.sourceId = 424242;
        e2.targetId = 0;
        bus.publish(e2);
        CHECK(a.anim().currentState() == AnimationState::Idle);
    }
    // ---------- Task 3c: sacrifice ids + start/interrupt ----------------
    {
        EventBus bus;
        AnimationDirector dir(bus);
        Recorder started, done, stopped;
        started.attach(bus, EventType::SacrificeStarted);
        done.attach(bus, EventType::SacrificeCompleted);
        stopped.attach(bus, EventType::SacrificeInterrupted);
        Cultist priest(FACTION_CTHULHU, Vec3{0, 0, 0});
        Civilian victim(Vec3{2, 0, 0});
        for (Entity* e : {static_cast<Entity*>(&priest),
                          static_cast<Entity*>(&victim)}) {
            bindProceduralFallbacks(e->anim());
            dir.track(*e);
        }
        SacrificeRitual r(bus);
        r.setPerformer(priest.id());
        r.setVictim(victim.id());
        r.start();
        CHECK(started.count(EventType::SacrificeStarted) == 1);
        CHECK(started.events[0].sourceId == priest.id());
        CHECK(started.events[0].targetId == victim.id());
        CHECK(priest.anim().currentState() ==
              AnimationState::SacrificePerformer);
        CHECK(victim.anim().currentState() == AnimationState::SacrificeVictim);
        r.interrupt();
        CHECK(stopped.count(EventType::SacrificeInterrupted) == 1);
        CHECK(stopped.events[0].sourceId == priest.id());
        CHECK(stopped.events[0].targetId == victim.id());
        CHECK(priest.anim().currentState() == AnimationState::Idle);
        CHECK(victim.anim().currentState() == AnimationState::Idle);
        // Completion carries the ids too.
        SacrificeRitual r2(bus);
        r2.setPerformer(priest.id());
        r2.setVictim(victim.id());
        r2.start();
        CHECK(r2.update(30.0));
        CHECK(done.count(EventType::SacrificeCompleted) == 1);
        CHECK(done.events[0].sourceId == priest.id());
        CHECK(done.events[0].targetId == victim.id());
    }
    // ---------- Task 3d: Maul hook (monstrosity vs human) ---------------
    {
        EventBus bus;
        AnimationDirector dir(bus);
        Recorder maul;
        maul.attach(bus, EventType::MaulStruck);
        Monstrosity beast(FACTION_NEUTRAL, Vec3{0, 0, 0}, "ossified_brute",
                          true);
        Civilian civ(Vec3{1, 0, 0});
        Cultist cult(FACTION_CTHULHU, Vec3{2, 0, 0});
        for (Entity* e : {static_cast<Entity*>(&beast),
                          static_cast<Entity*>(&civ),
                          static_cast<Entity*>(&cult)}) {
            bindProceduralFallbacks(e->anim());
            dir.track(*e);
        }
        combat::dealDamage(civ, 10.0f, bus, EventType::MimicSlain, false,
                           beast.id(), EntityType::Monstrosity);
        CHECK(maul.count(EventType::MaulStruck) == 1);
        CHECK(maul.events[0].sourceId == beast.id());
        CHECK(maul.events[0].targetId == civ.id());
        CHECK(beast.anim().currentState() == AnimationState::Maul);
        dir.tick(1.0); // Maul auto-returns
        CHECK(beast.anim().currentState() == AnimationState::Idle);
        // Cultists are human prey too.
        combat::dealDamage(cult, 10.0f, bus, EventType::MimicSlain, false,
                           beast.id(), EntityType::Monstrosity);
        CHECK(maul.count(EventType::MaulStruck) == 2);
        // Non-human targets: no maul event (beast-on-beast).
        Monstrosity other(FACTION_NEUTRAL, Vec3{3, 0, 0}, "pale_wight", true);
        combat::dealDamage(other, 10.0f, bus, EventType::MimicSlain, false,
                           beast.id(), EntityType::Monstrosity);
        CHECK(maul.count(EventType::MaulStruck) == 2);
        // Non-monstrosity attackers: no maul event.
        Civilian civ2(Vec3{4, 0, 0});
        combat::dealDamage(civ2, 10.0f, bus, EventType::MimicSlain, false,
                           cult.id(), EntityType::Cultist);
        CHECK(maul.count(EventType::MaulStruck) == 2);
    }
    // ---------- Task 4a: glTF extractor reads 32 embedded clips ---------
    {
        const std::string prefix = repoPrefix();
        for (const char* species : {"dagon_spawn", "risen_dead", "wraith"}) {
            std::map<std::string, AnimationClip> clips;
            std::string err;
            const bool ok = extractGlbAnimations(
                prefix + "assets/creatures/" + species + ".glb", clips,
                &err);
            CHECK(ok);
            if (!ok) std::cout << "  extract err: " << err << "\n";
            CHECK(clips.size() == 32);
            for (const char* want :
                 {"idle", "walk", "sprint", "die", "attack-melee-right"}) {
                CHECK(clips.count(want) == 1);
            }
            const AnimationClip& idle = clips["idle"];
            CHECK(idle.loop);
            CHECK(idle.durationSeconds > 0.0);
            CHECK(!idle.isProcedural()); // real asset data
            CHECK(!idle.tracks.empty());
            CHECK(idle.tracks.count("torso") == 1); // joint name resolved
            const AnimationClip& die = clips["die"];
            CHECK(!die.loop); // one-shot
            // Tracks are sampled sanely: keys ascending, euler in degrees.
            for (const auto& kv : idle.tracks) {
                const auto& keys = kv.second.keys;
                CHECK(!keys.empty());
                for (size_t k = 1; k < keys.size(); ++k)
                    CHECK(keys[k].time >= keys[k - 1].time);
            }
        }
        // Missing file: false, no crash.
        std::map<std::string, AnimationClip> clips;
        std::string err;
        CHECK(!extractGlbAnimations(prefix + "assets/creatures/nope.glb",
                                    clips, &err));
        CHECK(!err.empty());
    }
    // ---------- Task 4b: embedded clip binding + wins rule --------------
    {
        const std::string prefix = repoPrefix();
        AnimationStateMachine sm;
        const int n = bindEmbeddedSpeciesClips(sm, "dagon_spawn", prefix +
                                                            "assets");
        CHECK(n == 6); // idle, walk, sprint, die, attack-melee-right,
                       // attack-kick-right
        CHECK(sm.hasClip(AnimationState::Idle));
        CHECK(sm.hasClip(AnimationState::Walk));
        CHECK(sm.hasClip(AnimationState::Run));
        CHECK(sm.hasClip(AnimationState::Death));
        CHECK(sm.hasClip(AnimationState::Attack));
        CHECK(sm.hasClip(AnimationState::Maul));
        CHECK(!sm.hasClip(AnimationState::Cast)); // no gltf "cast"
        CHECK(sm.clip(AnimationState::Idle)->name == "idle");
        CHECK(!sm.clip(AnimationState::Idle)->isProcedural());
        CHECK(!sm.clip(AnimationState::Death)->loop);
        // Wins rule: a clip bound earlier (e.g. custom .canim) is kept.
        AnimationStateMachine sm2;
        AnimationClip custom;
        custom.name = "CustomIdle";
        sm2.bindClip(AnimationState::Idle, custom);
        bindEmbeddedSpeciesClips(sm2, "dagon_spawn", prefix + "assets");
        CHECK(sm2.clip(AnimationState::Idle)->name == "CustomIdle");
        // Unknown species / missing file: 0, procedural still applies.
        AnimationStateMachine sm3;
        CHECK(bindEmbeddedSpeciesClips(sm3, "pale_wight",
                                      prefix + "assets") == 0);
        CHECK(!sm3.hasClip(AnimationState::Idle));
    }
    // ---------- Task 4c: package .canim binding -------------------------
    {
        const std::string dir = repoPrefix() +
            "assets/characters/civilian_villager/animations";
        AnimationStateMachine sm;
        const int n = bindPackCanimClips(sm, dir);
        // 16 files; cheer/dodge/interact/work have no engine state.
        CHECK(n == 12);
        CHECK(sm.hasClip(AnimationState::Idle));
        CHECK(sm.hasClip(AnimationState::Walk));
        CHECK(sm.hasClip(AnimationState::Run));
        CHECK(sm.hasClip(AnimationState::Attack));
        CHECK(sm.hasClip(AnimationState::Death));
        CHECK(sm.hasClip(AnimationState::Stunned));
        CHECK(sm.hasClip(AnimationState::Cast));
        CHECK(sm.hasClip(AnimationState::Channel));
        CHECK(sm.hasClip(AnimationState::FearRun));
        CHECK(sm.hasClip(AnimationState::Brawl));
        CHECK(sm.hasClip(AnimationState::SacrificePerformer));
        CHECK(sm.hasClip(AnimationState::SacrificeVictim));
        CHECK(sm.clip(AnimationState::FearRun)->name == "FearRun");
        CHECK(sm.clip(AnimationState::Attack)->name == "Attack");
        CHECK(!sm.clip(AnimationState::Attack)->isProcedural());
        CHECK(!sm.clip(AnimationState::Brawl)->loop); // one-shot
        // Missing dir: 0, no crash (procedural covers it).
        AnimationStateMachine sm2;
        CHECK(bindPackCanimClips(sm2, "assets/characters/nope/animations") ==
              0);
        CHECK(bindPackCanimClips(sm2, "") == 0);
        CHECK(!sm2.hasClip(AnimationState::Idle));
    }
    // ---------- Task 4d: full wins order .canim > embedded > procedural --
    {
        const std::string prefix = repoPrefix();
        Monstrosity m(FACTION_NEUTRAL, Vec3{0, 0, 0}, "dagon_spawn", true);
        m.setAnimPackDir(prefix +
                         "assets/characters/civilian_villager/animations");
        bindEntityClips(m, prefix + "assets");
        // .canim wins over the embedded "idle" for Idle...
        CHECK(m.anim().clip(AnimationState::Idle)->name == "Idle");
        // ...embedded wins over procedural for Maul (no .canim "Maul"?) —
        // actually brawl.canim exists, so Brawl is .canim; Maul has no
        // .canim file, so the embedded "attack-kick-right" binds it.
        CHECK(m.anim().clip(AnimationState::Maul)->name ==
              "attack-kick-right");
        // ...procedural covers states nobody provides (Maul for others).
        CHECK(m.anim().hasClip(AnimationState::Channel)); // .canim
        // Regression: 9 skeleton-only models stay fully procedural.
        for (const char* sp :
             {"pale_wight", "ossified_brute", "charnel_imp",
              "skittering_ghoul", "spawned"}) {
            Monstrosity sk(FACTION_NEUTRAL, Vec3{0, 0, 0}, sp, true);
            bindEntityClips(sk, prefix + "assets");
            CHECK(sk.anim().hasClip(AnimationState::Idle));
            CHECK(sk.anim().clip(AnimationState::Idle)->isProcedural());
            CHECK(sk.anim().hasClip(AnimationState::FearRun));
            CHECK(sk.anim().hasClip(AnimationState::Brawl));
            CHECK(sk.anim().hasClip(AnimationState::Maul));
            CHECK(sk.anim().hasClip(AnimationState::SacrificeVictim));
        }
    }

    std::cout << "checks=" << checks << " failures=" << failures << "\n";
    return failures == 0 ? 0 : 1;
}
