// CULT-ULHU wave 5a tests: animation clip data model, serialization,
// procedural generators, state-machine blending, fallback binding.
// Compiled manually against the static lib (see wave5a notes); run directly.

#include "animation/AnimationClip.h"
#include "animation/AnimationStateMachine.h"
#include "animation/BoneTrack.h"
#include "animation/ClipSerializer.h"
#include "animation/FbxClipImporter.h"
#include "animation/ProceduralClips.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

using namespace cultulhu;

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

#define CHECK_CLOSE(a, b, eps) do { ++checks; if (std::fabs((a) - (b)) > (eps)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #a " (" << (a) \
    << ") != " #b " (" << (b) << ")\n"; } } while (0)

static Keyframe kf(double t, float px, float py, float pz, float rx, float ry,
                   float rz) {
    Keyframe k;
    k.time = t;
    k.pos = Vec3(px, py, pz);
    k.rotEuler = Vec3(rx, ry, rz);
    return k;
}

// Build a simple two-key looping clip on one bone: rotX 0 -> 90 over 2s.
static AnimationClip twoKeyClip(const std::string& name, double duration,
                                bool loop, float endRotX) {
    AnimationClip c;
    c.name = name;
    c.durationSeconds = duration;
    c.loop = loop;
    BoneTrack tr;
    tr.bone = "hips";
    tr.keys.push_back(kf(0.0, 0, 0, 0, 0, 0, 0));
    tr.keys.push_back(kf(duration, 0, 10, 0, endRotX, 0, 0));
    c.tracks["hips"] = tr;
    return c;
}

// 1. Clip sampling: exact keys, interpolation, clamps, loop wrap.
static void testClipSampling() {
    AnimationClip c = twoKeyClip("T", 2.0, true, 90.0f);

    Pose p0 = c.sampleAt(0.0);
    CHECK_CLOSE(p0["hips"].rotEuler.x, 0.0, 1e-6);
    CHECK_CLOSE(p0["hips"].pos.y, 0.0, 1e-6);

    Pose p1 = c.sampleAt(2.0); // exact end key (wraps to 0 for loops)
    CHECK_CLOSE(p1["hips"].rotEuler.x, 0.0, 1e-6);

    Pose pm = c.sampleAt(1.0); // midpoint lerp
    CHECK_CLOSE(pm["hips"].rotEuler.x, 45.0, 1e-4);
    CHECK_CLOSE(pm["hips"].pos.y, 5.0, 1e-4);

    Pose pw = c.sampleAt(5.0); // wraps to t=1.0
    CHECK_CLOSE(pw["hips"].rotEuler.x, 45.0, 1e-4);

    Pose pn = c.sampleAt(-0.5); // wraps to t=1.5
    CHECK_CLOSE(pn["hips"].rotEuler.x, 67.5, 1e-4);

    // One-shot: clamp at the ends.
    AnimationClip one = twoKeyClip("O", 2.0, false, 90.0f);
    Pose po = one.sampleAt(99.0);
    CHECK_CLOSE(po["hips"].rotEuler.x, 90.0, 1e-6);
    Pose po0 = one.sampleAt(-3.0);
    CHECK_CLOSE(po0["hips"].rotEuler.x, 0.0, 1e-6);
    Pose poe = one.sampleAt(2.0);
    CHECK_CLOSE(poe["hips"].rotEuler.x, 90.0, 1e-6);

    // Empty clip -> empty pose; empty track -> identity transform.
    AnimationClip empty;
    CHECK(empty.sampleAt(0.5).empty());
    BoneTrack tr;
    tr.bone = "head";
    BoneTransform bt = tr.sample(3.0);
    CHECK_CLOSE(bt.pos.x, 0.0, 1e-9);
    CHECK_CLOSE(bt.rotEuler.x, 0.0, 1e-9);

    // Existing API intact.
    CHECK(c.isProcedural());
    CHECK(c.hasTrackData());
    CHECK(!empty.hasTrackData());
}

// 2. Serialization round-trip on a procedural walk clip.
static void testSerializationRoundTrip() {
    // Tests may run from the build dir (ctest) where assets/animations/
    // doesn't exist; ensure it before any file writes.
    std::error_code ec;
    std::filesystem::create_directories("assets/animations", ec);
    const std::string path = "assets/animations/_test_roundtrip.canim";
    AnimationClip walk = makeWalk();
    CHECK(ClipSerializer::save(walk, path));

    AnimationClip loaded;
    CHECK(ClipSerializer::load(path, loaded));
    CHECK(loaded.name == walk.name);
    CHECK_CLOSE(loaded.durationSeconds, walk.durationSeconds, 1e-9);
    CHECK(loaded.loop == walk.loop);
    CHECK(loaded.tracks.size() == walk.tracks.size());
    for (const auto& kv : walk.tracks) {
        const auto it = loaded.tracks.find(kv.first);
        CHECK(it != loaded.tracks.end());
        if (it == loaded.tracks.end()) continue;
        const std::vector<Keyframe>& a = kv.second.keys;
        const std::vector<Keyframe>& b = it->second.keys;
        CHECK(a.size() == b.size());
        for (size_t i = 0; i < a.size() && i < b.size(); ++i) {
            CHECK_CLOSE(a[i].time, b[i].time, 1e-6);
            CHECK_CLOSE(a[i].pos.x, b[i].pos.x, 1e-4);
            CHECK_CLOSE(a[i].pos.y, b[i].pos.y, 1e-4);
            CHECK_CLOSE(a[i].pos.z, b[i].pos.z, 1e-4);
            CHECK_CLOSE(a[i].rotEuler.x, b[i].rotEuler.x, 1e-4);
            CHECK_CLOSE(a[i].rotEuler.y, b[i].rotEuler.y, 1e-4);
            CHECK_CLOSE(a[i].rotEuler.z, b[i].rotEuler.z, 1e-4);
        }
    }
    std::remove(path.c_str());

    // Missing file -> false, out untouched.
    AnimationClip untouched;
    untouched.name = "keep";
    CHECK(!ClipSerializer::load("assets/animations/does_not_exist.canim",
                                untouched));
    CHECK(untouched.name == "keep");

    // Malformed file -> false.
    const std::string bad = "assets/animations/_test_bad.canim";
    FILE* f = std::fopen(bad.c_str(), "w");
    std::fputs("GARBAGE not a clip\n", f);
    std::fclose(f);
    AnimationClip badOut;
    CHECK(!ClipSerializer::load(bad, badOut));
    std::remove(bad.c_str());
}

// 3. Procedural generator sanity.
static void testProceduralSanity() {
    const std::vector<std::string>& bones = humanoidBones();
    CHECK(bones.size() == 11);

    AnimationClip walk = makeWalk();
    AnimationClip run = makeRun();
    AnimationClip idle = makeIdle();
    AnimationClip attack = makeAttackSwing();
    AnimationClip death = makeDeath();

    for (const AnimationClip* c : {&walk, &run, &idle, &attack, &death}) {
        CHECK(!c->tracks.empty());
        for (const std::string& b : bones) {
            CHECK(c->tracks.count(b) == 1);
            if (c->tracks.count(b))
                CHECK(!c->tracks.at(b).keys.empty());
        }
    }

    CHECK_CLOSE(walk.durationSeconds, 1.0, 1e-9);
    CHECK(walk.loop);
    CHECK_CLOSE(run.durationSeconds, 0.6, 1e-9);
    CHECK(run.loop);
    CHECK_CLOSE(idle.durationSeconds, 2.0, 1e-9);
    CHECK(idle.loop);
    CHECK_CLOSE(attack.durationSeconds, 0.8, 1e-9);
    CHECK(!attack.loop);
    CHECK_CLOSE(death.durationSeconds, 1.2, 1e-9);
    CHECK(!death.loop);

    // Walk loop seam: first and last keys of each track match.
    for (const auto& kv : walk.tracks) {
        const std::vector<Keyframe>& keys = kv.second.keys;
        CHECK_CLOSE(keys.front().rotEuler.x, keys.back().rotEuler.x, 1e-4);
        CHECK_CLOSE(keys.front().pos.y, keys.back().pos.y, 1e-4);
    }

    // Walk legs swing opposite phase at quarter period.
    Pose wq = walk.sampleAt(0.25);
    CHECK(wq["upperLegL"].rotEuler.x > 20.0);
    CHECK(wq["upperLegR"].rotEuler.x < -20.0);
    // Arms counter-swing the same-side leg.
    CHECK(wq["upperArmL"].rotEuler.x < 0.0);
    // Hips stay near rest height.
    CHECK(std::fabs(wq["hips"].pos.y - 1.0) < 0.1);

    // Death ends on the ground, tipped over.
    Pose de = death.sampleAt(1.2);
    CHECK(de["hips"].pos.y < 0.3);
    CHECK(de["hips"].rotEuler.z > 60.0);

    // Attack is an overhead swing: arms rise then strike.
    Pose aw = attack.sampleAt(0.32);
    Pose as = attack.sampleAt(0.56);
    CHECK(aw["upperArmL"].rotEuler.x < -40.0); // wound up overhead
    CHECK(as["upperArmL"].rotEuler.x > 40.0);  // struck down

    // Procedural clips report as procedural (no source path).
    CHECK(walk.isProcedural());
}

// 4. Blending math: factor 0 / 0.5 / 1 gives prev / mid / next pose.
static void testBlending() {
    // A: 20 deg/s over 2s loop. B: 100 deg/s over 1s loop.
    AnimationClip a = twoKeyClip("A", 2.0, true, 40.0f);
    AnimationClip b = twoKeyClip("B", 1.0, true, 100.0f);

    AnimationStateMachine sm;
    sm.bindClip(AnimationState::Idle, a);
    sm.bindClip(AnimationState::Walk, b);

    sm.requestState(AnimationState::Idle, 0.0); // snap, no blend
    sm.update(1.0);                            // A at t=1.0 -> 20 deg
    CHECK_CLOSE(sm.currentPose()["hips"].rotEuler.x, 20.0, 1e-4);

    sm.requestState(AnimationState::Walk, 2.0); // 2s blend
    CHECK(sm.blending());
    CHECK_CLOSE(sm.blendT(), 0.0, 1e-9);
    // blendT=0 -> previous pose: A(1.0) = 20.
    CHECK_CLOSE(sm.sampleBlendedPose()["hips"].rotEuler.x, 20.0, 1e-4);
    CHECK_CLOSE(sm.currentPose()["hips"].rotEuler.x, 20.0, 1e-4);

    sm.requestState(AnimationState::Walk, 2.0); // 2s blend
    CHECK(sm.blending());
    CHECK_CLOSE(sm.blendT(), 0.0, 1e-9);
    // blendT=0 -> previous pose: A(1.0) = 20 deg.
    CHECK_CLOSE(sm.sampleBlendedPose()["hips"].rotEuler.x, 20.0, 1e-4);
    CHECK_CLOSE(sm.currentPose()["hips"].rotEuler.x, 20.0, 1e-4);

    sm.update(0.5); // blendT=0.25; prev A(1.5)=30, new B(0.5)=50
    CHECK_CLOSE(sm.blendT(), 0.25, 1e-9);
    const double s = 0.25 * 0.25 * (3.0 - 2.0 * 0.25); // smoothstep(0.25)
    const double expectMid = 30.0 + (50.0 - 30.0) * s; // 33.125
    CHECK_CLOSE(sm.sampleBlendedPose()["hips"].rotEuler.x, expectMid, 1e-3);
    CHECK_CLOSE(sm.currentPose()["hips"].rotEuler.x, expectMid, 1e-3);

    sm.update(1.5); // blend finished (blendT=1)
    CHECK(!sm.blending());
    CHECK_CLOSE(sm.blendT(), 1.0, 1e-9);
    // New clip fully in charge: B(2.0) wraps on the 1s loop -> B(0.0) = 0.
    CHECK_CLOSE(sm.currentPose()["hips"].rotEuler.x, 0.0, 1e-4);
    CHECK_CLOSE(sm.sampleBlendedPose()["hips"].rotEuler.x, 0.0, 1e-4);

    // Transition rules still hold with blending: Death is terminal,
    // Stunned interrupts.
    sm.requestState(AnimationState::Death, 0.5);
    CHECK(sm.currentState() == AnimationState::Death);
    sm.update(0.25);
    sm.requestState(AnimationState::Walk, 0.5);
    CHECK(sm.currentState() == AnimationState::Death); // terminal

    AnimationStateMachine sm2;
    sm2.bindClip(AnimationState::Idle, a);
    sm2.bindClip(AnimationState::Stunned, b);
    sm2.requestState(AnimationState::Idle, 0.0);
    sm2.requestState(AnimationState::Stunned, 0.3); // interrupts with blend
    CHECK(sm2.currentState() == AnimationState::Stunned);
    CHECK(sm2.blending());
    sm2.update(0.3);
    CHECK(!sm2.blending());

    // One-shot auto-return still works and blends back to Idle.
    AnimationStateMachine sm3;
    sm3.bindClip(AnimationState::Idle, a);
    sm3.bindClip(AnimationState::Attack, twoKeyClip("Atk", 0.8, false, 60.f));
    sm3.requestState(AnimationState::Attack, 0.0);
    sm3.update(0.8);
    CHECK(sm3.currentState() == AnimationState::Idle);
    CHECK(sm3.blending()); // auto-return blends
    sm3.update(0.25);
    CHECK(!sm3.blending());
}

// 5. Procedural fallback binding.
static void testFallbackBinding() {
    AnimationStateMachine sm;
    CHECK(!sm.hasClip(AnimationState::Idle));
    CHECK(sm.proceduralFallback());

    bindProceduralFallbacks(sm);
    CHECK(sm.hasClip(AnimationState::Idle));
    CHECK(sm.hasClip(AnimationState::Walk));
    CHECK(sm.hasClip(AnimationState::Run));
    CHECK(sm.hasClip(AnimationState::Attack));
    CHECK(sm.hasClip(AnimationState::Death));
    CHECK(!sm.hasClip(AnimationState::Cast)); // deliberately left unbound
    CHECK(!sm.proceduralFallback());         // Idle now bound

    sm.requestState(AnimationState::Cast, 0.0);
    CHECK(sm.proceduralFallback()); // runtime fallback still covers Cast
    CHECK(sm.currentPose().empty());

    // Existing bindings are never overwritten.
    AnimationClip custom = twoKeyClip("Custom", 1.0, true, 10.0f);
    sm.bindClip(AnimationState::Idle, custom);
    bindProceduralFallbacks(sm);
    CHECK(sm.clip(AnimationState::Idle)->name == "Custom");

    // Bound procedural idle actually samples a pose.
    AnimationStateMachine sm2;
    bindProceduralFallbacks(sm2);
    sm2.update(0.5);
    Pose p = sm2.currentPose();
    CHECK(!p.empty());
    CHECK(p.count("hips") == 1);
}

// 6. FBX hook compiles and documents the future path (no SDK bundled).
static void testFbxHookCompiles() {
    struct NullImporter : FbxClipImporter {
        bool importFile(const std::string&, const RigDefinition&,
                        std::vector<AnimationClip>&) override {
            return false;
        }
    };
    NullImporter imp;
    FbxClipImporter& base = imp;
    std::vector<AnimationClip> out;
    RigDefinition rig{"mixamo_x_bot", 11, "MixamoToUnreal"};
    CHECK(!base.importFile("missing.fbx", rig, out));
    CHECK(out.empty());
}

int main() {
    std::cout << "CULT-ULHU wave 5a animation tests\n";
    testClipSampling();
    testSerializationRoundTrip();
    testProceduralSanity();
    testBlending();
    testFallbackBinding();
    testFbxHookCompiles();
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
