#include "animation/ProceduralClips.h"

#include "animation/AnimationStateMachine.h"

#include <cmath>

namespace cultulhu {

namespace {

constexpr double kPi = 3.14159265358979323846;

Keyframe kf(double t, float px, float py, float pz, float rx, float ry,
           float rz) {
    Keyframe k;
    k.time = t;
    k.pos = Vec3(px, py, pz);
    k.rotEuler = Vec3(rx, ry, rz);
    return k;
}

Keyframe kfRot(double t, float rx, float ry, float rz) {
    return kf(t, 0.0f, 0.0f, 0.0f, rx, ry, rz);
}

void addTrack(AnimationClip& c, const std::string& bone,
              std::vector<Keyframe> keys) {
    BoneTrack tr;
    tr.bone = bone;
    tr.keys = std::move(keys);
    c.tracks[bone] = std::move(tr);
}

AnimationClip baseClip(const std::string& name, double duration, bool loop) {
    AnimationClip c;
    c.name = name;
    c.durationSeconds = duration;
    c.loop = loop;
    return c;
}

// Sample a per-bone angle function over one loop period, emitting N+1 keys
// with the endpoint duplicating the start for a seamless loop.
template <typename Fn>
std::vector<Keyframe> loopKeys(double duration, int steps, Fn angleAt) {
    std::vector<Keyframe> keys;
    keys.reserve(static_cast<size_t>(steps) + 1);
    for (int i = 0; i <= steps; ++i) {
        const double t = duration * static_cast<double>(i) /
                         static_cast<double>(steps);
        keys.push_back(angleAt(t));
    }
    return keys;
}

} // namespace

const std::vector<std::string>& humanoidBones() {
    static const std::vector<std::string> bones = {
        "hips",      "spine",     "head",      "upperArmL", "upperArmR",
        "lowerArmL", "lowerArmR", "upperLegL", "upperLegR", "lowerLegL",
        "lowerLegR",
    };
    return bones;
}

AnimationClip makeWalk() {
    AnimationClip c = baseClip("Walk", 1.0, true);
    const double T = 1.0;
    const int N = 12;
    const auto phase = [&](double t) { return 2.0 * kPi * t / T; };

    // Legs: thigh swings opposite phase; knee flexes most mid-swing.
    addTrack(c, "upperLegL", loopKeys(T, N, [&](double t) {
                 return kfRot(t, static_cast<float>(28.0 * std::sin(phase(t))),
                              0.0f, 0.0f);
             }));
    addTrack(c, "upperLegR", loopKeys(T, N, [&](double t) {
                 return kfRot(t,
                              static_cast<float>(28.0 * std::sin(phase(t) + kPi)),
                              0.0f, 0.0f);
             }));
    addTrack(c, "lowerLegL", loopKeys(T, N, [&](double t) {
                 const double bend = 42.0 * std::max(0.0, std::sin(phase(t) + 2.1));
                 return kfRot(t, static_cast<float>(-bend), 0.0f, 0.0f);
             }));
    addTrack(c, "lowerLegR", loopKeys(T, N, [&](double t) {
                 const double bend =
                     42.0 * std::max(0.0, std::sin(phase(t) + kPi + 2.1));
                 return kfRot(t, static_cast<float>(-bend), 0.0f, 0.0f);
             }));

    // Arms: counter-swing against the same-side leg, elbows slightly bent.
    addTrack(c, "upperArmL", loopKeys(T, N, [&](double t) {
                 return kfRot(t, static_cast<float>(-20.0 * std::sin(phase(t))),
                              0.0f, 6.0f);
             }));
    addTrack(c, "upperArmR", loopKeys(T, N, [&](double t) {
                 return kfRot(t, static_cast<float>(20.0 * std::sin(phase(t))),
                              0.0f, -6.0f);
             }));
    addTrack(c, "lowerArmL", loopKeys(T, N, [&](double t) {
                 return kfRot(t,
                              static_cast<float>(-14.0 - 8.0 * std::sin(phase(t))),
                              0.0f, 0.0f);
             }));
    addTrack(c, "lowerArmR", loopKeys(T, N, [&](double t) {
                 return kfRot(t,
                              static_cast<float>(-14.0 + 8.0 * std::sin(phase(t))),
                              0.0f, 0.0f);
             }));

    // Hips: bob twice per cycle, slight pelvic rotation.
    addTrack(c, "hips", loopKeys(T, N, [&](double t) {
                 const float y =
                     static_cast<float>(1.0 + 0.05 * std::sin(2.0 * phase(t)));
                 return kf(t, 0.0f, y, 0.0f,
                           0.0f, static_cast<float>(5.0 * std::sin(phase(t))),
                           0.0f);
             }));
    // Spine: counter-rotate, tiny forward lean. Head stays level.
    addTrack(c, "spine", loopKeys(T, N, [&](double t) {
                 return kfRot(t, 3.0f,
                              static_cast<float>(-4.0 * std::sin(phase(t))), 0.0f);
             }));
    addTrack(c, "head", loopKeys(T, N, [&](double t) {
                 return kfRot(t, 0.0f,
                              static_cast<float>(2.0 * std::sin(phase(t))), 0.0f);
             }));
    return c;
}

AnimationClip makeRun() {
    AnimationClip c = baseClip("Run", 0.6, true);
    const double T = 0.6;
    const int N = 12;
    const auto phase = [&](double t) { return 2.0 * kPi * t / T; };

    addTrack(c, "upperLegL", loopKeys(T, N, [&](double t) {
                 return kfRot(t, static_cast<float>(45.0 * std::sin(phase(t))),
                              0.0f, 0.0f);
             }));
    addTrack(c, "upperLegR", loopKeys(T, N, [&](double t) {
                 return kfRot(t,
                              static_cast<float>(45.0 * std::sin(phase(t) + kPi)),
                              0.0f, 0.0f);
             }));
    addTrack(c, "lowerLegL", loopKeys(T, N, [&](double t) {
                 const double bend = 65.0 * std::max(0.0, std::sin(phase(t) + 2.1));
                 return kfRot(t, static_cast<float>(-bend), 0.0f, 0.0f);
             }));
    addTrack(c, "lowerLegR", loopKeys(T, N, [&](double t) {
                 const double bend =
                     65.0 * std::max(0.0, std::sin(phase(t) + kPi + 2.1));
                 return kfRot(t, static_cast<float>(-bend), 0.0f, 0.0f);
             }));

    // Arms pump harder, elbows bent ~50 degrees.
    addTrack(c, "upperArmL", loopKeys(T, N, [&](double t) {
                 return kfRot(t, static_cast<float>(-32.0 * std::sin(phase(t))),
                              0.0f, 8.0f);
             }));
    addTrack(c, "upperArmR", loopKeys(T, N, [&](double t) {
                 return kfRot(t, static_cast<float>(32.0 * std::sin(phase(t))),
                              0.0f, -8.0f);
             }));
    addTrack(c, "lowerArmL", loopKeys(T, N, [&](double t) {
                 return kfRot(t,
                              static_cast<float>(-50.0 - 10.0 * std::sin(phase(t))),
                              0.0f, 0.0f);
             }));
    addTrack(c, "lowerArmR", loopKeys(T, N, [&](double t) {
                 return kfRot(t,
                              static_cast<float>(-50.0 + 10.0 * std::sin(phase(t))),
                              0.0f, 0.0f);
             }));

    addTrack(c, "hips", loopKeys(T, N, [&](double t) {
                 const float y =
                     static_cast<float>(1.0 + 0.09 * std::sin(2.0 * phase(t)));
                 return kf(t, 0.0f, y, 0.0f,
                           0.0f, static_cast<float>(7.0 * std::sin(phase(t))),
                           0.0f);
             }));
    // Forward lean into the run.
    addTrack(c, "spine", loopKeys(T, N, [&](double t) {
                 return kfRot(t, 12.0f,
                              static_cast<float>(-6.0 * std::sin(phase(t))), 0.0f);
             }));
    addTrack(c, "head", loopKeys(T, N, [&](double t) {
                 return kfRot(t, -6.0f,
                              static_cast<float>(3.0 * std::sin(phase(t))), 0.0f);
             }));
    return c;
}

AnimationClip makeIdle() {
    AnimationClip c = baseClip("Idle", 2.0, true);
    const double T = 2.0;
    const int N = 16;
    const auto phase = [&](double t) { return 2.0 * kPi * t / T; };

    // Breathing: spine rises/falls, hips bob subtly.
    addTrack(c, "spine", loopKeys(T, N, [&](double t) {
                 return kfRot(t, static_cast<float>(2.5 * std::sin(phase(t))),
                              0.0f, 0.0f);
             }));
    addTrack(c, "hips", loopKeys(T, N, [&](double t) {
                 const float y =
                     static_cast<float>(1.0 + 0.015 * std::sin(phase(t)));
                 return kf(t, 0.0f, y, 0.0f, 0.0f, 0.0f, 0.0f);
             }));
    // Arms hang with a faint sway; head looks around slowly.
    addTrack(c, "upperArmL", loopKeys(T, N, [&](double t) {
                 return kfRot(t, static_cast<float>(2.0 * std::sin(phase(t))),
                              0.0f, 7.0f);
             }));
    addTrack(c, "upperArmR", loopKeys(T, N, [&](double t) {
                 return kfRot(t, static_cast<float>(-2.0 * std::sin(phase(t))),
                              0.0f, -7.0f);
             }));
    addTrack(c, "lowerArmL", loopKeys(T, N, [&](double t) {
                 return kfRot(t, -6.0f, 0.0f, 0.0f);
             }));
    addTrack(c, "lowerArmR", loopKeys(T, N, [&](double t) {
                 return kfRot(t, -6.0f, 0.0f, 0.0f);
             }));
    addTrack(c, "head", loopKeys(T, N, [&](double t) {
                 return kfRot(t, 0.0f,
                              static_cast<float>(10.0 * std::sin(phase(t) * 0.5)),
                              0.0f);
             }));
    // Legs planted.
    for (const char* bone : {"upperLegL", "upperLegR", "lowerLegL",
                             "lowerLegR"}) {
        addTrack(c, bone,
                 {kfRot(0.0, 0.0f, 0.0f, 0.0f), kfRot(T, 0.0f, 0.0f, 0.0f)});
    }
    return c;
}

AnimationClip makeAttackSwing() {
    // Overhead two-handed swing: wind up (0-0.32s), strike (0.32-0.56s),
    // recover (0.56-0.8s). One-shot.
    AnimationClip c = baseClip("AttackSwing", 0.8, false);

    const std::vector<double> times = {0.0, 0.32, 0.56, 0.8};
    const std::vector<float> armPitch = {0.0f, -70.0f, 75.0f, 10.0f};
    const std::vector<float> elbow = {-10.0f, -20.0f, -5.0f, -10.0f};
    const std::vector<float> spinePitch = {0.0f, -8.0f, 18.0f, 4.0f};
    const std::vector<float> hipsY = {1.0f, 1.02f, 0.9f, 0.98f};
    const std::vector<float> hipsTwist = {0.0f, -10.0f, 12.0f, 0.0f};
    const std::vector<float> headPitch = {0.0f, -10.0f, 10.0f, 0.0f};

    auto keyed = [&](const std::vector<float>& v, int channel) {
        std::vector<Keyframe> keys;
        for (size_t i = 0; i < times.size(); ++i) {
            float rx = 0.0f, ry = 0.0f, rz = 0.0f;
            if (channel == 0) rx = v[i];
            else if (channel == 1) ry = v[i];
            else rz = v[i];
            keys.push_back(kfRot(times[i], rx, ry, rz));
        }
        return keys;
    };

    addTrack(c, "upperArmL", keyed(armPitch, 0));
    addTrack(c, "upperArmR", keyed(armPitch, 0));
    addTrack(c, "lowerArmL", keyed(elbow, 0));
    addTrack(c, "lowerArmR", keyed(elbow, 0));
    addTrack(c, "spine", keyed(spinePitch, 0));

    std::vector<Keyframe> hipsKeys;
    for (size_t i = 0; i < times.size(); ++i)
        hipsKeys.push_back(
            kf(times[i], 0.0f, hipsY[i], 0.0f, 0.0f, hipsTwist[i], 0.0f));
    addTrack(c, "hips", std::move(hipsKeys));
    addTrack(c, "head", keyed(headPitch, 0));

    // Wide stance, planted through the swing.
    addTrack(c, "upperLegL", {kfRot(0.0, -8.0f, 0.0f, 4.0f),
                             kfRot(0.8, -8.0f, 0.0f, 4.0f)});
    addTrack(c, "upperLegR", {kfRot(0.0, 8.0f, 0.0f, -4.0f),
                             kfRot(0.8, 8.0f, 0.0f, -4.0f)});
    addTrack(c, "lowerLegL", {kfRot(0.0, -6.0f, 0.0f, 0.0f),
                             kfRot(0.8, -6.0f, 0.0f, 0.0f)});
    addTrack(c, "lowerLegR", {kfRot(0.0, -6.0f, 0.0f, 0.0f),
                             kfRot(0.8, -6.0f, 0.0f, 0.0f)});
    return c;
}

AnimationClip makeDeath() {
    // Topple sideways and crumple: stagger (0-0.35s), collapse (0.35-0.7s),
    // settle (0.7-1.2s). One-shot.
    AnimationClip c = baseClip("Death", 1.2, false);

    const std::vector<double> times = {0.0, 0.35, 0.7, 1.2};
    const std::vector<float> hipsY = {1.0f, 0.75f, 0.35f, 0.18f};
    const std::vector<float> hipsRoll = {0.0f, 15.0f, 55.0f, 85.0f};
    const std::vector<float> spinePitch = {0.0f, 10.0f, 25.0f, 15.0f};
    const std::vector<float> headRoll = {0.0f, 10.0f, 40.0f, 70.0f};
    const std::vector<float> armSplay = {7.0f, 30.0f, 55.0f, 70.0f};
    const std::vector<float> kneeBuckle = {-5.0f, -30.0f, -60.0f, -80.0f};

    std::vector<Keyframe> hipsKeys;
    for (size_t i = 0; i < times.size(); ++i)
        hipsKeys.push_back(
            kf(times[i], 0.0f, hipsY[i], 0.0f, 0.0f, 0.0f, hipsRoll[i]));
    addTrack(c, "hips", std::move(hipsKeys));

    auto keyed = [&](const std::vector<float>& v, int channel) {
        std::vector<Keyframe> keys;
        for (size_t i = 0; i < times.size(); ++i) {
            float rx = 0.0f, ry = 0.0f, rz = 0.0f;
            if (channel == 0) rx = v[i];
            else if (channel == 1) ry = v[i];
            else rz = v[i];
            keys.push_back(kfRot(times[i], rx, ry, rz));
        }
        return keys;
    };

    addTrack(c, "spine", keyed(spinePitch, 0));
    addTrack(c, "head", keyed(headRoll, 2));

    std::vector<Keyframe> armL, armR;
    for (size_t i = 0; i < times.size(); ++i) {
        armL.push_back(kfRot(times[i], -10.0f, 0.0f, armSplay[i]));
        armR.push_back(kfRot(times[i], -10.0f, 0.0f, -armSplay[i]));
    }
    addTrack(c, "upperArmL", std::move(armL));
    addTrack(c, "upperArmR", std::move(armR));
    addTrack(c, "lowerArmL", keyed(kneeBuckle, 0)); // arms fold as they fall
    addTrack(c, "lowerArmR", keyed(kneeBuckle, 0));

    addTrack(c, "upperLegL", {kfRot(0.0, 0.0f, 0.0f, 5.0f),
                             kfRot(1.2, 10.0f, 0.0f, 20.0f)});
    addTrack(c, "upperLegR", {kfRot(0.0, 0.0f, 0.0f, -5.0f),
                             kfRot(1.2, -5.0f, 0.0f, -10.0f)});
    addTrack(c, "lowerLegL", keyed(kneeBuckle, 0));
    addTrack(c, "lowerLegR", keyed(kneeBuckle, 0));
    return c;
}

void bindProceduralFallbacks(AnimationStateMachine& sm) {
    if (!sm.hasClip(AnimationState::Idle))
        sm.bindClip(AnimationState::Idle, makeIdle());
    if (!sm.hasClip(AnimationState::Walk))
        sm.bindClip(AnimationState::Walk, makeWalk());
    if (!sm.hasClip(AnimationState::Run))
        sm.bindClip(AnimationState::Run, makeRun());
    if (!sm.hasClip(AnimationState::Attack))
        sm.bindClip(AnimationState::Attack, makeAttackSwing());
    if (!sm.hasClip(AnimationState::Death))
        sm.bindClip(AnimationState::Death, makeDeath());
    if (!sm.hasClip(AnimationState::Cast))
        sm.bindClip(AnimationState::Cast, makeCast());
    if (!sm.hasClip(AnimationState::Stunned))
        sm.bindClip(AnimationState::Stunned, makeStunned());
    if (!sm.hasClip(AnimationState::Channel))
        sm.bindClip(AnimationState::Channel, makeChannel());
    if (!sm.hasClip(AnimationState::CastWave))
        sm.bindClip(AnimationState::CastWave, makeCastWave());
    if (!sm.hasClip(AnimationState::FearRun))
        sm.bindClip(AnimationState::FearRun, makeFearRun());
    if (!sm.hasClip(AnimationState::Brawl))
        sm.bindClip(AnimationState::Brawl, makeBrawl());
    if (!sm.hasClip(AnimationState::SacrificePerformer))
        sm.bindClip(AnimationState::SacrificePerformer, makeSacrificePerformer());
    if (!sm.hasClip(AnimationState::SacrificeVictim))
        sm.bindClip(AnimationState::SacrificeVictim, makeSacrificeVictim());
    if (!sm.hasClip(AnimationState::Maul))
        sm.bindClip(AnimationState::Maul, makeMaul());
    if (!sm.hasClip(AnimationState::WarBattle))
        sm.bindClip(AnimationState::WarBattle, makeWarBattle());
    if (!sm.hasClip(AnimationState::Build))
        sm.bindClip(AnimationState::Build, makeBuild());
    if (!sm.hasClip(AnimationState::Repair))
        sm.bindClip(AnimationState::Repair, makeRepair());
    // Levitate/Launch/Levitated: left unbound on purpose; the Wave of
    // Domination runtime drives those victim-side states directly.
}

AnimationClip makeCast() {
    // Spell cast: gather (0-0.3s), thrust both arms forward to release
    // (0.3-0.6s), recover (0.6-1.0s). One-shot.
    AnimationClip c = baseClip("Cast", 1.0, false);

    const std::vector<double> times = {0.0, 0.3, 0.6, 1.0};
    const std::vector<float> armPitch = {10.0f, -30.0f, 85.0f, 10.0f};
    const std::vector<float> elbow = {-15.0f, -70.0f, -8.0f, -15.0f};
    const std::vector<float> spinePitch = {0.0f, -6.0f, 12.0f, 2.0f};
    const std::vector<float> hipsY = {1.0f, 0.96f, 0.94f, 1.0f};
    const std::vector<float> headPitch = {0.0f, -8.0f, 12.0f, 0.0f};

    auto keyed = [&](const std::vector<float>& v) {
        std::vector<Keyframe> keys;
        for (size_t i = 0; i < times.size(); ++i)
            keys.push_back(kfRot(times[i], v[i], 0.0f, 0.0f));
        return keys;
    };

    addTrack(c, "upperArmL", keyed(armPitch));
    addTrack(c, "upperArmR", keyed(armPitch));
    addTrack(c, "lowerArmL", keyed(elbow));
    addTrack(c, "lowerArmR", keyed(elbow));
    addTrack(c, "spine", keyed(spinePitch));
    addTrack(c, "head", keyed(headPitch));

    std::vector<Keyframe> hipsKeys;
    for (size_t i = 0; i < times.size(); ++i)
        hipsKeys.push_back(kf(times[i], 0.0f, hipsY[i], 0.0f,
                              0.0f, 0.0f, 0.0f));
    addTrack(c, "hips", std::move(hipsKeys));

    // Feet planted through the cast.
    for (const char* bone : {"upperLegL", "upperLegR", "lowerLegL",
                             "lowerLegR"}) {
        addTrack(c, bone,
                 {kfRot(0.0, 0.0f, 0.0f, 0.0f), kfRot(1.0, 0.0f, 0.0f, 0.0f)});
    }
    return c;
}

AnimationClip makeStunned() {
    // Hit by crowd control: snap the head back, arms flail outward,
    // knees buckle. One-shot.
    AnimationClip c = baseClip("Stunned", 0.9, false);

    const std::vector<double> times = {0.0, 0.25, 0.55, 0.9};
    const std::vector<float> headPitch = {0.0f, -25.0f, -10.0f, 5.0f};
    const std::vector<float> headYaw = {0.0f, 12.0f, -12.0f, 0.0f};
    const std::vector<float> armOut = {7.0f, 45.0f, 30.0f, 10.0f};
    const std::vector<float> spinePitch = {0.0f, -14.0f, -6.0f, 3.0f};
    const std::vector<float> hipsY = {1.0f, 0.9f, 0.86f, 0.97f};
    const std::vector<float> kneeBuckle = {-6.0f, -28.0f, -20.0f, -8.0f};

    auto keyed = [&](const std::vector<float>& v, int channel) {
        std::vector<Keyframe> keys;
        for (size_t i = 0; i < times.size(); ++i) {
            float rx = 0.0f, ry = 0.0f, rz = 0.0f;
            if (channel == 0) rx = v[i];
            else if (channel == 1) ry = v[i];
            else rz = v[i];
            keys.push_back(kfRot(times[i], rx, ry, rz));
        }
        return keys;
    };

    addTrack(c, "head", [&] {
        std::vector<Keyframe> headKeys;
        for (size_t i = 0; i < times.size(); ++i)
            headKeys.push_back(
                kfRot(times[i], headPitch[i], headYaw[i], 0.0f));
        return headKeys;
    }());
    addTrack(c, "upperArmL", keyed(armOut, 2));
    addTrack(c, "upperArmR", keyed(armOut, 2));
    addTrack(c, "lowerArmL", keyed(kneeBuckle, 0));
    addTrack(c, "lowerArmR", keyed(kneeBuckle, 0));
    addTrack(c, "spine", keyed(spinePitch, 0));

    std::vector<Keyframe> hipsKeys;
    for (size_t i = 0; i < times.size(); ++i)
        hipsKeys.push_back(kf(times[i], 0.0f, hipsY[i], 0.0f,
                              0.0f, 0.0f, 0.0f));
    addTrack(c, "hips", std::move(hipsKeys));
    addTrack(c, "upperLegL", keyed(kneeBuckle, 0));
    addTrack(c, "upperLegR", keyed(kneeBuckle, 0));
    addTrack(c, "lowerLegL", keyed(kneeBuckle, 0));
    addTrack(c, "lowerLegR", keyed(kneeBuckle, 0));
    return c;
}

AnimationClip makeChannel() {
    // Sustained cast: arms raised overhead, held with a faint tremble.
    // Loops until the channel ends.
    AnimationClip c = baseClip("Channel", 2.0, true);
    const double T = 2.0;
    const int N = 16;
    const auto phase = [&](double t) { return 2.0 * kPi * t / T; };
    const auto tremble = [&](double t, float base) {
        return base + static_cast<float>(3.0 * std::sin(phase(t) * 4.0));
    };

    addTrack(c, "upperArmL", loopKeys(T, N, [&](double t) {
                 return kfRot(t, tremble(t, -165.0f), 0.0f, 12.0f);
             }));
    addTrack(c, "upperArmR", loopKeys(T, N, [&](double t) {
                 return kfRot(t, tremble(t + 0.13, -165.0f), 0.0f, -12.0f);
             }));
    addTrack(c, "lowerArmL", loopKeys(T, N, [&](double t) {
                 return kfRot(t, tremble(t, -12.0f), 0.0f, 0.0f);
             }));
    addTrack(c, "lowerArmR", loopKeys(T, N, [&](double t) {
                 return kfRot(t, tremble(t + 0.13, -12.0f), 0.0f, 0.0f);
             }));
    addTrack(c, "spine", loopKeys(T, N, [&](double t) {
                 return kfRot(t, tremble(t, -8.0f), 0.0f, 0.0f);
             }));
    addTrack(c, "head", loopKeys(T, N, [&](double t) {
                 return kfRot(t, tremble(t, -18.0f), 0.0f, 0.0f);
             }));
    addTrack(c, "hips", loopKeys(T, N, [&](double t) {
                 return kf(t, 0.0f, 0.97f, 0.0f, 0.0f, 0.0f, 0.0f);
             }));
    for (const char* bone : {"upperLegL", "upperLegR", "lowerLegL",
                             "lowerLegR"}) {
        addTrack(c, bone,
                 {kfRot(0.0, 0.0f, 0.0f, 0.0f), kfRot(T, 0.0f, 0.0f, 0.0f)});
    }
    return c;
}

AnimationClip makeCastWave() {
    // RMB-kit gesture (Wave of Domination): wide horizontal sweep, both
    // arms, hips driving the turn. One-shot.
    AnimationClip c = baseClip("CastWave", 1.2, false);

    const std::vector<double> times = {0.0, 0.4, 0.8, 1.2};
    const std::vector<float> armSweep = {0.0f, -60.0f, 60.0f, 0.0f};
    const std::vector<float> armPitch = {20.0f, 45.0f, 45.0f, 20.0f};
    const std::vector<float> hipsTwist = {0.0f, -25.0f, 25.0f, 0.0f};
    const std::vector<float> spinePitch = {0.0f, 6.0f, 6.0f, 0.0f};

    auto keyed = [&](const std::vector<float>& v, int channel) {
        std::vector<Keyframe> keys;
        for (size_t i = 0; i < times.size(); ++i) {
            float rx = 0.0f, ry = 0.0f, rz = 0.0f;
            if (channel == 0) rx = v[i];
            else if (channel == 1) ry = v[i];
            else rz = v[i];
            keys.push_back(kfRot(times[i], rx, ry, rz));
        }
        return keys;
    };

    // Wide sweep: pitch raises the arms, yaw swings them; mirrored.
    addTrack(c, "upperArmL", [&] {
        std::vector<Keyframe> keys;
        for (size_t i = 0; i < times.size(); ++i)
            keys.push_back(
                kfRot(times[i], armPitch[i], armSweep[i], 0.0f));
        return keys;
    }());
    addTrack(c, "upperArmR", [&] {
        std::vector<Keyframe> keys;
        for (size_t i = 0; i < times.size(); ++i)
            keys.push_back(
                kfRot(times[i], armPitch[i], -armSweep[i], 0.0f));
        return keys;
    }());
    addTrack(c, "lowerArmL",
             {kfRot(0.0, -10.0f, 0.0f, 0.0f), kfRot(1.2, -10.0f, 0.0f, 0.0f)});
    addTrack(c, "lowerArmR",
             {kfRot(0.0, -10.0f, 0.0f, 0.0f), kfRot(1.2, -10.0f, 0.0f, 0.0f)});
    addTrack(c, "spine", keyed(spinePitch, 0));

    std::vector<Keyframe> hipsKeys;
    for (size_t i = 0; i < times.size(); ++i)
        hipsKeys.push_back(kf(times[i], 0.0f, 1.0f, 0.0f, 0.0f,
                              hipsTwist[i], 0.0f));
    addTrack(c, "hips", std::move(hipsKeys));
    addTrack(c, "head", keyed(hipsTwist, 1));
    for (const char* bone : {"upperLegL", "upperLegR"}) {
        addTrack(c, bone,
                 {kfRot(0.0, -5.0f, 0.0f, 6.0f), kfRot(1.2, -5.0f, 0.0f, 6.0f)});
    }
    for (const char* bone : {"lowerLegL", "lowerLegR"}) {
        addTrack(c, bone,
                 {kfRot(0.0, -8.0f, 0.0f, 0.0f), kfRot(1.2, -8.0f, 0.0f, 0.0f)});
    }
    return c;
}

AnimationClip makeFearRun() {
    // Panicked sprint: faster/harder than Run. Hunched torso, arms flailing
    // up-out on asymmetric phases, head jerking side to side, high hip bob
    // with lateral jitter. Loops.
    AnimationClip c = baseClip("FearRun", 0.55, true);
    const double T = 0.55;
    const int N = 16;
    const auto phase = [&](double t) { return 2.0 * kPi * t / T; };

    addTrack(c, "upperLegL", loopKeys(T, N, [&](double t) {
                 return kfRot(t, static_cast<float>(55.0 * std::sin(phase(t))),
                              0.0f, 0.0f);
             }));
    addTrack(c, "upperLegR", loopKeys(T, N, [&](double t) {
                 return kfRot(t,
                              static_cast<float>(55.0 * std::sin(phase(t) + kPi)),
                              0.0f, 0.0f);
             }));
    addTrack(c, "lowerLegL", loopKeys(T, N, [&](double t) {
                 const double bend = 75.0 * std::max(0.0, std::sin(phase(t) + 2.1));
                 return kfRot(t, static_cast<float>(-bend), 0.0f, 0.0f);
             }));
    addTrack(c, "lowerLegR", loopKeys(T, N, [&](double t) {
                 const double bend =
                     75.0 * std::max(0.0, std::sin(phase(t) + kPi + 2.1));
                 return kfRot(t, static_cast<float>(-bend), 0.0f, 0.0f);
             }));

    // Arms flail up and out on asymmetric phases — no pumping discipline.
    addTrack(c, "upperArmL", loopKeys(T, N, [&](double t) {
                 const double flail =
                     -30.0 * std::sin(phase(t)) - 18.0 * std::sin(2.0 * phase(t) + 0.7);
                 return kfRot(t, static_cast<float>(flail), 0.0f, 38.0f);
             }));
    addTrack(c, "upperArmR", loopKeys(T, N, [&](double t) {
                 const double flail =
                     -30.0 * std::sin(phase(t) + 1.7) - 18.0 * std::sin(2.0 * phase(t) + 2.2);
                 return kfRot(t, static_cast<float>(flail), 0.0f, -38.0f);
             }));
    addTrack(c, "lowerArmL", loopKeys(T, N, [&](double t) {
                 return kfRot(t,
                              static_cast<float>(-70.0 - 14.0 * std::sin(phase(t) + 0.5)),
                              0.0f, 0.0f);
             }));
    addTrack(c, "lowerArmR", loopKeys(T, N, [&](double t) {
                 return kfRot(t,
                              static_cast<float>(-70.0 - 14.0 * std::sin(phase(t) + 2.3)),
                              0.0f, 0.0f);
             }));

    // High bob with lateral jitter.
    addTrack(c, "hips", loopKeys(T, N, [&](double t) {
                 const float y =
                     static_cast<float>(1.0 + 0.13 * std::sin(2.0 * phase(t)));
                 const float x =
                     static_cast<float>(0.02 * std::sin(7.0 * phase(t)));
                 return kf(t, x, y, 0.0f,
                           0.0f, static_cast<float>(9.0 * std::sin(phase(t))),
                           0.0f);
             }));
    // Torso hunched hard forward.
    addTrack(c, "spine", loopKeys(T, N, [&](double t) {
                 return kfRot(t, static_cast<float>(28.0 + 4.0 * std::sin(2.0 * phase(t))),
                              static_cast<float>(-6.0 * std::sin(phase(t))), 0.0f);
             }));
    // Head jerks side to side.
    addTrack(c, "head", loopKeys(T, N, [&](double t) {
                 const double yaw = 22.0 * std::sin(2.0 * phase(t)) +
                                    8.0 * std::sin(5.0 * phase(t));
                 return kfRot(t, 8.0f, static_cast<float>(yaw), 0.0f);
             }));
    return c;
}

AnimationClip makeBrawl() {
    // Wild brawl: left haymaker (0-0.3s), right haymaker (0.3-0.55s),
    // grapple-shake midsection (0.55-0.75s), stagger back (0.75-0.9s).
    // One-shot.
    AnimationClip c = baseClip("Brawl", 0.9, false);

    const std::vector<double> times = {0.0, 0.15, 0.3, 0.45, 0.55,
                                       0.6, 0.65, 0.7, 0.75, 0.9};
    const std::vector<float> armLPitch = {-15.0f, -130.0f, 80.0f, 55.0f,
                                          35.0f, 30.0f, 35.0f, 30.0f, 35.0f,
                                          10.0f};
    const std::vector<float> armRPitch = {-15.0f, 40.0f, -130.0f, 80.0f,
                                          35.0f, 35.0f, 30.0f, 35.0f, 30.0f,
                                          10.0f};
    const std::vector<float> armYaw = {0.0f, 25.0f, -45.0f, -15.0f, 0.0f, 0.0f,
                                       0.0f, 0.0f, 0.0f, 0.0f};
    const std::vector<float> elbow = {-20.0f, -25.0f, -10.0f, -10.0f, -60.0f,
                                      -70.0f, -70.0f, -70.0f, -70.0f, -20.0f};
    const std::vector<float> spinePitch = {5.0f, -5.0f, 12.0f, 12.0f, 8.0f,
                                           8.0f, 8.0f, 8.0f, 8.0f, -8.0f};
    const std::vector<float> twist = {0.0f, -20.0f, 22.0f, -18.0f, 0.0f, 8.0f,
                                      -8.0f, 8.0f, -8.0f, 0.0f};
    const std::vector<float> hipsY = {1.0f, 0.95f, 0.92f, 0.92f, 0.9f, 0.88f,
                                      0.9f, 0.88f, 0.9f, 0.98f};
    const std::vector<float> headPitch = {0.0f, -5.0f, 10.0f, 10.0f, 5.0f,
                                          5.0f, 5.0f, 5.0f, 5.0f, -10.0f};

    // Pitch + yaw keyed together for the haymaker arcs.
    auto armTrack = [&](const std::vector<float>& pitch, float yawSign) {
        std::vector<Keyframe> keys;
        for (size_t i = 0; i < times.size(); ++i)
            keys.push_back(kfRot(times[i], pitch[i], yawSign * armYaw[i], 0.0f));
        return keys;
    };
    auto keyed = [&](const std::vector<float>& v, int channel) {
        std::vector<Keyframe> keys;
        for (size_t i = 0; i < times.size(); ++i) {
            float rx = 0.0f, ry = 0.0f, rz = 0.0f;
            if (channel == 0) rx = v[i];
            else if (channel == 1) ry = v[i];
            else rz = v[i];
            keys.push_back(kfRot(times[i], rx, ry, rz));
        }
        return keys;
    };

    addTrack(c, "upperArmL", armTrack(armLPitch, 1.0f));
    addTrack(c, "upperArmR", armTrack(armRPitch, -1.0f));
    addTrack(c, "lowerArmL", keyed(elbow, 0));
    addTrack(c, "lowerArmR", keyed(elbow, 0));
    // Torso twists with the punches; rapid small y-rotations during the
    // grapple-shake midsection.
    addTrack(c, "spine", [&] {
        std::vector<Keyframe> keys;
        for (size_t i = 0; i < times.size(); ++i)
            keys.push_back(kfRot(times[i], spinePitch[i], twist[i], 0.0f));
        return keys;
    }());

    std::vector<Keyframe> hipsKeys;
    for (size_t i = 0; i < times.size(); ++i)
        hipsKeys.push_back(kf(times[i], 0.0f, hipsY[i], 0.0f, 0.0f,
                              twist[i] * 0.6f, 0.0f));
    addTrack(c, "hips", std::move(hipsKeys));
    addTrack(c, "head", keyed(headPitch, 0));

    // Staggered stance, planted through the brawl.
    addTrack(c, "upperLegL", {kfRot(0.0, -10.0f, 0.0f, 6.0f),
                             kfRot(0.9, -10.0f, 0.0f, 6.0f)});
    addTrack(c, "upperLegR", {kfRot(0.0, 10.0f, 0.0f, -6.0f),
                             kfRot(0.9, 10.0f, 0.0f, -6.0f)});
    addTrack(c, "lowerLegL", {kfRot(0.0, -10.0f, 0.0f, 0.0f),
                             kfRot(0.9, -10.0f, 0.0f, 0.0f)});
    addTrack(c, "lowerLegR", {kfRot(0.0, -10.0f, 0.0f, 0.0f),
                             kfRot(0.9, -10.0f, 0.0f, 0.0f)});
    return c;
}

AnimationClip makeSacrificePerformer() {
    // Ceremonial rite: arms raised overhead pulsing slowly up-down, slight
    // torso sway, head tilted up. Reads as ritual, not combat. Loops.
    AnimationClip c = baseClip("SacrificePerformer", 2.0, true);
    const double T = 2.0;
    const int N = 16;
    const auto phase = [&](double t) { return 2.0 * kPi * t / T; };
    const auto pulse = [&](double t) {
        return static_cast<float>(6.0 * std::sin(phase(t)));
    };

    addTrack(c, "upperArmL", loopKeys(T, N, [&](double t) {
                 return kfRot(t, -160.0f - pulse(t), 0.0f, 12.0f);
             }));
    addTrack(c, "upperArmR", loopKeys(T, N, [&](double t) {
                 return kfRot(t, -160.0f - pulse(t + 0.2), 0.0f, -12.0f);
             }));
    addTrack(c, "lowerArmL", loopKeys(T, N, [&](double t) {
                 return kfRot(t, -12.0f - pulse(t) * 0.5f, 0.0f, 0.0f);
             }));
    addTrack(c, "lowerArmR", loopKeys(T, N, [&](double t) {
                 return kfRot(t, -12.0f - pulse(t + 0.2) * 0.5f, 0.0f, 0.0f);
             }));
    addTrack(c, "spine", loopKeys(T, N, [&](double t) {
                 return kfRot(t, -6.0f,
                              static_cast<float>(3.0 * std::sin(phase(t))),
                              static_cast<float>(3.0 * std::sin(phase(t))));
             }));
    addTrack(c, "head", loopKeys(T, N, [&](double t) {
                 return kfRot(t, static_cast<float>(-22.0 + 2.0 * std::sin(phase(t))),
                              0.0f, 0.0f);
             }));
    addTrack(c, "hips", loopKeys(T, N, [&](double t) {
                 return kf(t, static_cast<float>(0.02 * std::sin(phase(t))),
                           0.98f, 0.0f, 0.0f, 0.0f, 0.0f);
             }));
    for (const char* bone : {"upperLegL", "upperLegR", "lowerLegL",
                             "lowerLegR"}) {
        addTrack(c, bone,
                 {kfRot(0.0, 0.0f, 0.0f, 0.0f), kfRot(T, 0.0f, 0.0f, 0.0f)});
    }
    return c;
}

AnimationClip makeSacrificeVictim() {
    // Bound victim: kneeling (hips dropped, knees bent), torso hunched,
    // arms bound behind the back. Periodic struggle bursts spike the
    // shoulders and torso every ~0.5s (3 bursts per 1.6s loop). Loops.
    AnimationClip c = baseClip("SacrificeVictim", 1.6, true);
    const double T = 1.6;
    const int N = 24;
    const auto phase = [&](double t) { return 2.0 * kPi * t / T; };
    // Struggle envelope: 3 sharp bursts per loop period.
    const auto burst = [&](double t) {
        const double b = std::max(0.0, std::sin(3.0 * kPi * t / T));
        return std::pow(b, 8.0);
    };

    addTrack(c, "hips", loopKeys(T, N, [&](double t) {
                 const float y =
                     static_cast<float>(0.55 + 0.03 * burst(t));
                 return kf(t, 0.0f, y, 0.0f, 0.0f,
                           static_cast<float>(6.0 * burst(t) *
                                              std::sin(8.0 * phase(t))),
                           0.0f);
             }));
    // Kneel: thighs angled down-back, knees folded deep.
    addTrack(c, "upperLegL", loopKeys(T, N, [&](double t) {
                 return kfRot(t, static_cast<float>(-30.0 - 4.0 * burst(t)),
                              0.0f, 8.0f);
             }));
    addTrack(c, "upperLegR", loopKeys(T, N, [&](double t) {
                 return kfRot(t, static_cast<float>(-30.0 - 4.0 * burst(t)),
                              0.0f, -8.0f);
             }));
    addTrack(c, "lowerLegL", loopKeys(T, N, [&](double t) {
                 return kfRot(t, static_cast<float>(95.0 - 6.0 * burst(t)),
                              0.0f, 0.0f);
             }));
    addTrack(c, "lowerLegR", loopKeys(T, N, [&](double t) {
                 return kfRot(t, static_cast<float>(95.0 - 6.0 * burst(t)),
                              0.0f, 0.0f);
             }));
    // Torso hunched; struggles spike the twist.
    addTrack(c, "spine", loopKeys(T, N, [&](double t) {
                 return kfRot(t, static_cast<float>(25.0 + 8.0 * burst(t)),
                              static_cast<float>(10.0 * burst(t) *
                                                 std::sin(8.0 * phase(t))),
                              0.0f);
             }));
    // Head bows; jerks during bursts.
    addTrack(c, "head", loopKeys(T, N, [&](double t) {
                 return kfRot(t, static_cast<float>(15.0 - 8.0 * burst(t)),
                              static_cast<float>(6.0 * burst(t) *
                                                 std::sin(11.0 * phase(t))),
                              0.0f);
             }));
    // Arms bound behind the back; shoulders strain during bursts.
    addTrack(c, "upperArmL", loopKeys(T, N, [&](double t) {
                 return kfRot(t, -45.0f, 0.0f,
                              static_cast<float>(10.0 + 15.0 * burst(t)));
             }));
    addTrack(c, "upperArmR", loopKeys(T, N, [&](double t) {
                 return kfRot(t, -45.0f, 0.0f,
                              static_cast<float>(-10.0 - 15.0 * burst(t)));
             }));
    addTrack(c, "lowerArmL", loopKeys(T, N, [&](double t) {
                 return kfRot(t, static_cast<float>(-30.0 - 20.0 * burst(t)),
                              0.0f, 0.0f);
             }));
    addTrack(c, "lowerArmR", loopKeys(T, N, [&](double t) {
                 return kfRot(t, static_cast<float>(-30.0 - 20.0 * burst(t)),
                              0.0f, 0.0f);
             }));
    return c;
}

AnimationClip makeMaul() {
    // Beast pounce-and-tear: crouch (0-0.25s), pounce forward (0.25-0.45s),
    // alternating raking arm tears with a head snap (0.45-0.75s), settle
    // into a low crouch (0.75-1.0s). One-shot.
    AnimationClip c = baseClip("Maul", 1.0, false);

    const std::vector<double> times = {0.0, 0.25, 0.45, 0.6, 0.75, 1.0};
    const std::vector<float> hipsY = {1.0f, 0.62f, 0.75f, 0.68f, 0.62f, 0.65f};
    const std::vector<float> hipsZ = {0.0f, -0.1f, 0.35f, 0.3f, 0.15f, 0.1f};
    const std::vector<float> spinePitch = {5.0f, 25.0f, 35.0f, 40.0f, 30.0f,
                                           22.0f};
    const std::vector<float> armLPitch = {-20.0f, -60.0f, 70.0f, -40.0f,
                                          75.0f, 40.0f};
    const std::vector<float> armRPitch = {-20.0f, -60.0f, -40.0f, 70.0f,
                                          -30.0f, 40.0f};
    const std::vector<float> elbow = {-15.0f, -40.0f, -10.0f, -10.0f, -10.0f,
                                      -25.0f};
    const std::vector<float> headPitch = {0.0f, 5.0f, 10.0f, 35.0f, 15.0f,
                                          10.0f};
    const std::vector<float> thigh = {-5.0f, -45.0f, -35.0f, -40.0f, -45.0f,
                                      -40.0f};
    const std::vector<float> knee = {-6.0f, -70.0f, -50.0f, -55.0f, -60.0f,
                                     -55.0f};

    auto keyed = [&](const std::vector<float>& v, int channel) {
        std::vector<Keyframe> keys;
        for (size_t i = 0; i < times.size(); ++i) {
            float rx = 0.0f, ry = 0.0f, rz = 0.0f;
            if (channel == 0) rx = v[i];
            else if (channel == 1) ry = v[i];
            else rz = v[i];
            keys.push_back(kfRot(times[i], rx, ry, rz));
        }
        return keys;
    };

    addTrack(c, "upperArmL", keyed(armLPitch, 0));
    addTrack(c, "upperArmR", keyed(armRPitch, 0));
    addTrack(c, "lowerArmL", keyed(elbow, 0));
    addTrack(c, "lowerArmR", keyed(elbow, 0));
    addTrack(c, "spine", keyed(spinePitch, 0));
    // Head snaps down with the rakes.
    addTrack(c, "head", keyed(headPitch, 0));

    std::vector<Keyframe> hipsKeys;
    for (size_t i = 0; i < times.size(); ++i)
        hipsKeys.push_back(kf(times[i], 0.0f, hipsY[i], hipsZ[i],
                              0.0f, 0.0f, 0.0f));
    addTrack(c, "hips", std::move(hipsKeys));

    addTrack(c, "upperLegL", keyed(thigh, 0));
    addTrack(c, "upperLegR", keyed(thigh, 0));
    addTrack(c, "lowerLegL", keyed(knee, 0));
    addTrack(c, "lowerLegR", keyed(knee, 0));
    return c;
}

AnimationClip makeWarBattle() {
    // Disciplined war fighting: clean measured weapon arcs (right arm),
    // shield-block raises with a hold (left arm up across the torso),
    // deliberate advancing steps (weight shifts forward each cycle).
    // Heavier and far more controlled than Brawl — trained soldiers.
    // Loops. All motion is integer-frequency over the period, so the
    // loopKeys endpoint matches the start exactly.
    AnimationClip c = baseClip("WarBattle", 1.1, true);
    const double T = 1.1;
    const int N = 20;
    const auto phase = [&](double t) { return 2.0 * kPi * t / T; };
    // Shield raise with a held plateau: rises, holds, lowers.
    const auto blockHold = [&](double t) {
        const double b = 0.5 - 0.5 * std::cos(phase(t));
        const double x = (b - 0.35) / (0.65 - 0.35);
        const double cl = x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x);
        return cl * cl * (3.0 - 2.0 * cl);
    };

    // Weapon arm: one clean overhead arc per cycle, elbow extending
    // through the downswing. No wild yaw.
    addTrack(c, "upperArmR", loopKeys(T, N, [&](double t) {
                 const double p = phase(t);
                 return kfRot(t, static_cast<float>(8.0 - 62.0 * std::sin(p)),
                              static_cast<float>(6.0 * std::sin(2.0 * p)),
                              0.0f);
             }));
    addTrack(c, "lowerArmR", loopKeys(T, N, [&](double t) {
                 const double extend =
                     18.0 + 10.0 * std::max(0.0, std::sin(phase(t)));
                 return kfRot(t, static_cast<float>(-extend), 0.0f, 0.0f);
             }));
    // Shield arm: raised across the torso, pulsing up into a held block
    // counter-phase to the weapon swing.
    addTrack(c, "upperArmL", loopKeys(T, N, [&](double t) {
                 return kfRot(t,
                              static_cast<float>(-52.0 + 16.0 * blockHold(t)),
                              -22.0f, 0.0f);
             }));
    addTrack(c, "lowerArmL", loopKeys(T, N, [&](double t) {
                 return kfRot(t, -72.0f, 0.0f, 0.0f);
             }));

    // Torso: measured twist with the weapon, slight forward press.
    addTrack(c, "spine", loopKeys(T, N, [&](double t) {
                 const double p = phase(t);
                 return kfRot(t, static_cast<float>(7.0 + 3.0 * std::sin(2.0 * p)),
                              static_cast<float>(10.0 * std::sin(p)), 0.0f);
             }));
    // Head steady, eyes forward.
    addTrack(c, "head", loopKeys(T, N, [&](double t) {
                 return kfRot(t, 4.0f,
                              static_cast<float>(4.0 * std::sin(phase(t))), 0.0f);
             }));

    // Advancing steps: deliberate alternate strides with forward weight
    // shifts, knees driving.
    addTrack(c, "upperLegL", loopKeys(T, N, [&](double t) {
                 return kfRot(t, static_cast<float>(16.0 * std::sin(phase(t))),
                              0.0f, 0.0f);
             }));
    addTrack(c, "upperLegR", loopKeys(T, N, [&](double t) {
                 return kfRot(t,
                              static_cast<float>(16.0 * std::sin(phase(t) + kPi)),
                              0.0f, 0.0f);
             }));
    addTrack(c, "lowerLegL", loopKeys(T, N, [&](double t) {
                 const double bend =
                     34.0 * std::max(0.0, std::sin(phase(t) + 2.1));
                 return kfRot(t, static_cast<float>(-bend), 0.0f, 0.0f);
             }));
    addTrack(c, "lowerLegR", loopKeys(T, N, [&](double t) {
                 const double bend =
                     34.0 * std::max(0.0, std::sin(phase(t) + kPi + 2.1));
                 return kfRot(t, static_cast<float>(-bend), 0.0f, 0.0f);
             }));
    addTrack(c, "hips", loopKeys(T, N, [&](double t) {
                 const double p = phase(t);
                 const float y =
                     static_cast<float>(1.0 + 0.02 * std::sin(2.0 * p));
                 const float z = static_cast<float>(0.05 * std::sin(p));
                 return kf(t, 0.0f, y, z, 0.0f,
                           static_cast<float>(4.0 * std::sin(p)), 0.0f);
             }));
    return c;
}

AnimationClip makeBuild() {
    // The construction cycle in one loop: overhead hammering
    // (0-0.6s), bend-lift-carry (0.6-1.0s), crouch-place (1.0-1.35s),
    // stand and recover (1.35-1.8s). Keyframed; the first key is
    // duplicated at T for a seamless loop.
    AnimationClip c = baseClip("Build", 1.8, true);
    const double T = 1.8;

    const std::vector<double> times = {0.0,  0.2, 0.34, 0.46, 0.6,
                                       0.72, 0.88, 1.0, 1.18, 1.32,
                                       1.5,  T};
    // Right arm: two overhead hammer strikes, then lowers to grab, lifts
    // the load, sets it down, recovers.
    const std::vector<float> armRPitch = {0.0f,   -115.0f, 65.0f, -115.0f,
                                          55.0f,  45.0f,  20.0f, 25.0f,
                                          70.0f,  75.0f,  15.0f, 0.0f};
    const std::vector<float> armRElbow = {-12.0f, -25.0f, -8.0f,  -25.0f,
                                          -10.0f, -35.0f, -75.0f, -70.0f,
                                          -20.0f, -12.0f, -12.0f, -12.0f};
    // Left arm mirrors the hammer, then steadies the carried load.
    const std::vector<float> armLPitch = {0.0f,   -100.0f, 50.0f, -100.0f,
                                          40.0f,  35.0f,  15.0f, 20.0f,
                                          60.0f,  65.0f,  10.0f, 0.0f};
    const std::vector<float> spinePitch = {4.0f,  -6.0f, 16.0f, -6.0f,
                                           14.0f, 34.0f, 12.0f, 8.0f,
                                           26.0f, 30.0f, 6.0f,  4.0f};
    const std::vector<float> hipsY = {1.0f, 1.0f, 0.94f, 1.0f, 0.95f, 0.82f,
                                      0.9f, 0.95f, 0.62f, 0.6f, 0.92f, 1.0f};
    const std::vector<float> knee = {-8.0f,  -8.0f,  -14.0f, -8.0f, -12.0f,
                                     -35.0f, -18.0f, -12.0f, -55.0f, -60.0f,
                                     -15.0f, -8.0f};
    const std::vector<float> headPitch = {0.0f, -8.0f, 12.0f, -8.0f, 10.0f,
                                          18.0f, 8.0f,  4.0f,  16.0f, 18.0f,
                                          2.0f,  0.0f};

    auto keyed = [&](const std::vector<float>& v, int channel) {
        std::vector<Keyframe> keys;
        for (size_t i = 0; i < times.size(); ++i) {
            float rx = 0.0f, ry = 0.0f, rz = 0.0f;
            if (channel == 0) rx = v[i];
            else if (channel == 1) ry = v[i];
            else rz = v[i];
            keys.push_back(kfRot(times[i], rx, ry, rz));
        }
        return keys;
    };

    addTrack(c, "upperArmR", keyed(armRPitch, 0));
    addTrack(c, "lowerArmR", keyed(armRElbow, 0));
    addTrack(c, "upperArmL", keyed(armLPitch, 0));
    addTrack(c, "lowerArmL", keyed(armRElbow, 0));
    addTrack(c, "spine", keyed(spinePitch, 0));
    addTrack(c, "head", keyed(headPitch, 0));

    std::vector<Keyframe> hipsKeys;
    for (size_t i = 0; i < times.size(); ++i)
        hipsKeys.push_back(kf(times[i], 0.0f, hipsY[i], 0.0f, 0.0f, 0.0f, 0.0f));
    addTrack(c, "hips", std::move(hipsKeys));

    // Planted wide stance; knees track the crouches.
    addTrack(c, "upperLegL", {kfRot(0.0, -6.0f, 0.0f, 8.0f),
                             kfRot(T, -6.0f, 0.0f, 8.0f)});
    addTrack(c, "upperLegR", {kfRot(0.0, -6.0f, 0.0f, -8.0f),
                             kfRot(T, -6.0f, 0.0f, -8.0f)});
    addTrack(c, "lowerLegL", keyed(knee, 0));
    addTrack(c, "lowerLegR", keyed(knee, 0));
    return c;
}

AnimationClip makeRepair() {
    // Mending, not erecting: a low kneeling posture held through the loop,
    // small repeated hammering/fitting motions at waist height, then an
    // inspection pause (lean back, head tilts up) before resuming.
    // Keyframed; the first key is duplicated at T for a seamless loop.
    AnimationClip c = baseClip("Repair", 1.6, true);
    const double T = 1.6;

    const std::vector<double> times = {0.0, 0.2, 0.4, 0.6, 0.8,
                                       1.0, 1.2, 1.4, T};
    // Right arm: fitting taps at waist height (small, quick), rests on the
    // knee during inspection.
    const std::vector<float> armRPitch = {35.0f, 55.0f, 35.0f, 55.0f, 35.0f,
                                          15.0f, 10.0f, 25.0f, 35.0f};
    const std::vector<float> armRElbow = {-30.0f, -22.0f, -30.0f, -22.0f,
                                          -30.0f, -35.0f, -35.0f, -32.0f,
                                          -30.0f};
    // Left arm steadies the workpiece, then rests during inspection.
    const std::vector<float> armLPitch = {30.0f, 32.0f, 30.0f, 32.0f, 30.0f,
                                          12.0f, 10.0f, 22.0f, 30.0f};
    // Spine leans into the work; leans back to inspect.
    const std::vector<float> spinePitch = {18.0f, 20.0f, 18.0f, 20.0f, 18.0f,
                                           -4.0f, -8.0f, 10.0f, 18.0f};
    // Head watches the work; tilts up to inspect.
    const std::vector<float> headPitch = {14.0f, 14.0f, 14.0f, 14.0f, 14.0f,
                                          -12.0f, -18.0f, 4.0f, 14.0f};
    const std::vector<float> hipsY = {0.62f, 0.6f, 0.62f, 0.6f, 0.62f,
                                      0.64f, 0.65f, 0.63f, 0.62f};

    auto keyed = [&](const std::vector<float>& v, int channel) {
        std::vector<Keyframe> keys;
        for (size_t i = 0; i < times.size(); ++i) {
            float rx = 0.0f, ry = 0.0f, rz = 0.0f;
            if (channel == 0) rx = v[i];
            else if (channel == 1) ry = v[i];
            else rz = v[i];
            keys.push_back(kfRot(times[i], rx, ry, rz));
        }
        return keys;
    };

    addTrack(c, "upperArmR", keyed(armRPitch, 0));
    addTrack(c, "lowerArmR", keyed(armRElbow, 0));
    addTrack(c, "upperArmL", keyed(armLPitch, 0));
    addTrack(c, "lowerArmL", keyed(armRElbow, 0));
    addTrack(c, "spine", keyed(spinePitch, 0));
    addTrack(c, "head", keyed(headPitch, 0));

    std::vector<Keyframe> hipsKeys;
    for (size_t i = 0; i < times.size(); ++i)
        hipsKeys.push_back(kf(times[i], 0.0f, hipsY[i], 0.0f, 0.0f, 0.0f, 0.0f));
    addTrack(c, "hips", std::move(hipsKeys));

    // Kneel: thighs angled, knees folded deep, planted for the loop.
    addTrack(c, "upperLegL", {kfRot(0.0, -35.0f, 0.0f, 8.0f),
                             kfRot(T, -35.0f, 0.0f, 8.0f)});
    addTrack(c, "upperLegR", {kfRot(0.0, -35.0f, 0.0f, -8.0f),
                             kfRot(T, -35.0f, 0.0f, -8.0f)});
    addTrack(c, "lowerLegL", {kfRot(0.0, 80.0f, 0.0f, 0.0f),
                             kfRot(T, 80.0f, 0.0f, 0.0f)});
    addTrack(c, "lowerLegR", {kfRot(0.0, 80.0f, 0.0f, 0.0f),
                             kfRot(T, 80.0f, 0.0f, 0.0f)});
    return c;
}

} // namespace cultulhu
