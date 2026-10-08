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

} // namespace cultulhu
