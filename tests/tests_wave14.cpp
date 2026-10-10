// CULT-ULHU second-shift tests: deep character-package validation,
// rig-mapper hardening, and procedural fallback clip coverage.
//
// Covers: validateDeep grades (CLEAN / OK WITH WARNINGS / INVALID),
// def-schema checks, model magic probing, rig.map warning surfacing,
// per-bone diagnostics, expanded bone aliases (Mixamo/Blender/Rigify
// DEF-), and the new Cast/Stunned/Channel/CastWave procedural clips.
//
// Usage: tests_wave14 [repo_root]

#include "animation/AnimationStateMachine.h"
#include "animation/ProceduralClips.h"
#include "characters/CharacterPackageLoader.h"
#include "characters/CharacterRegistry.h"
#include "characters/CharacterValidator.h"
#include "characters/RigMapper.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace cultulhu;
namespace fs = std::filesystem;

static int checks = 0;
static int failures = 0;
#define CHECK(cond) do { ++checks; if (!(cond)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

static void writeFile(const std::string& path, const std::string& text) {
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream out(path);
    out << text;
}

static void writeBinary(const std::string& path, const std::string& bytes) {
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream out(path, std::ios::binary);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

static const char* kGoodDef = R"(
id = deep_test
display_name = Deep Test
flavor = Exercises the deep validator.
max_hp = 200
move_speed = 6.0
max_stamina = 100
melee_combo = test_flurry

[q]
id = zap
name = Zap
cooldown = 5
stamina_cost = 10
effect = aoe_damage
power = 50
range = 8

[rightclick]
kind = MeleeHeavy
name = Slam
damage_mult = 1.2
range = 3
)";

static CharacterPackage loadPkg(const std::string& pkgDir) {
    // pkgDir = <root>/<name>; scanAndLoad takes the root and find() the
    // package by folder name (loadOne is private to the loader).
    const std::string name = fs::path(pkgDir).filename().string();
    const std::string root = fs::path(pkgDir).parent_path().string();
    CharacterRegistry reg;
    CharacterPackageLoader loader(reg);
    loader.scanAndLoad(root);
    const CharacterPackage* p = loader.find(name);
    CHECK(p != nullptr);
    return p ? *p : CharacterPackage{};
}

static bool hasSub(const std::vector<std::string>& v, const std::string& s) {
    for (const auto& x : v)
        if (x.find(s) != std::string::npos) return true;
    return false;
}

// --- deep validator: grades ----------------------------------------------

static void testDeepClean() {
    const std::string root = "/tmp/w14_deep_clean/mychar";
    fs::remove_all("/tmp/w14_deep_clean");
    writeFile(root + "/character.def", kGoodDef);
    // Valid GLB magic + a bones.list covering the whole rig.
    writeBinary(root + "/model.glb", std::string("glTF\x02\x00\x00\x00", 8));
    writeFile(root + "/bones.list",
              "Hips\nSpine\nHead\nLeftArm\nRightArm\nLeftForeArm\n"
              "RightForeArm\nLeftUpLeg\nRightUpLeg\nLeftLeg\nRightLeg\n");
    CharacterPackage pkg = loadPkg(root);
    CHECK(pkg.defOk);
    DeepValidationReport r = CharacterValidator::validateDeep(pkg);
    // No animations/ and no cast clip for Q -> warnings, never errors.
    CHECK(r.grade == PackageGrade::Warnings);
    CHECK(r.errors.empty());
    CHECK(hasSub(r.warnings, "procedural fallback"));
    CHECK(hasSub(r.info, "model: 'model.glb'"));
    CHECK(hasSub(r.info, "rig:"));
    fs::remove_all("/tmp/w14_deep_clean");
}

static void testDeepInvalid() {
    const std::string root = "/tmp/w14_deep_bad/mychar";
    fs::remove_all("/tmp/w14_deep_bad");
    writeFile(root + "/character.def",
              "id = deep_bad\ndisplay_name = Bad\nmax_hp = -5\n"
              "move_speed = 5\nmax_stamina = 100\n"
              "[q]\nid = x\nname = X\ncooldown = -1\neffect = aoe_damage\n");
    CharacterPackage pkg = loadPkg(root);
    CHECK(pkg.defOk); // parses; values are the problem
    DeepValidationReport r = CharacterValidator::validateDeep(pkg);
    CHECK(r.grade == PackageGrade::Invalid);
    CHECK(hasSub(r.errors, "max_hp"));
    CHECK(hasSub(r.errors, "cooldown"));
    fs::remove_all("/tmp/w14_deep_bad");
}

static void testDeepDefWarnings() {
    const std::string root = "/tmp/w14_deep_warn/mychar";
    fs::remove_all("/tmp/w14_deep_warn");
    writeFile(root + "/character.def",
              "id = deep_warn\nmax_hp = 100\nmove_speed = 5\n"
              "max_stamina = 100\n"
              "[q]\nid = x\nname = X\neffect = mind_meld\n"
              "[rightclick]\nkind = MeleeHeavy\ncc = Paralyze\n");
    CharacterPackage pkg = loadPkg(root);
    DeepValidationReport r = CharacterValidator::validateDeep(pkg);
    CHECK(r.grade == PackageGrade::Warnings);
    CHECK(hasSub(r.warnings, "display_name"));
    CHECK(hasSub(r.warnings, "unknown effect 'mind_meld'"));
    CHECK(hasSub(r.warnings, "unknown cc 'Paralyze'"));
    CHECK(hasSub(r.warnings, "no melee_combo"));
    fs::remove_all("/tmp/w14_deep_warn");
}

static void testDeepModelMagic() {
    const std::string root = "/tmp/w14_deep_magic/mychar";
    fs::remove_all("/tmp/w14_deep_magic");
    writeFile(root + "/character.def", kGoodDef);
    // Not a GLB: wrong magic -> error, not a warning.
    writeBinary(root + "/model.glb", "definitely not a glb file....");
    CharacterPackage pkg = loadPkg(root);
    CHECK(pkg.hasModel);
    DeepValidationReport r = CharacterValidator::validateDeep(pkg);
    CHECK(r.grade == PackageGrade::Invalid);
    CHECK(hasSub(r.errors, "glTF"));
    fs::remove_all("/tmp/w14_deep_magic");
}

static void testDeepRigMapWarnings() {
    const std::string root = "/tmp/w14_deep_rigmap/mychar";
    fs::remove_all("/tmp/w14_deep_rigmap");
    writeFile(root + "/character.def", kGoodDef);
    writeBinary(root + "/model.glb", std::string("glTF\x02\x00\x00\x00", 8));
    writeFile(root + "/rig.map",
              "# explicit map\n"
              "upperArmL = MyArm_L\n"
              "tentacle = MyTentacle\n"   // unknown engine bone
              "this line has no equals\n");
    CharacterPackage pkg = loadPkg(root);
    CHECK(pkg.hasRigMap);
    CHECK(pkg.rigMapWarnings.size() == 2);
    DeepValidationReport r = CharacterValidator::validateDeep(pkg);
    CHECK(hasSub(r.warnings, "rig.map:"));
    CHECK(hasSub(r.warnings, "unknown engine bone 'tentacle'"));
    CHECK(hasSub(r.info, "rig: explicit rig.map"));
    // Diagnostics show the explicit mapping at 100%.
    CHECK(hasSub(r.info, "upperArmL"));
    CHECK(hasSub(r.info, "MyArm_L"));
    fs::remove_all("/tmp/w14_deep_rigmap");
}

static void testDeepTemplateAvatar(const std::string& repoRoot) {
    // The Cthulhu Avatar is the gold standard: deep validation must find
    // no ERRORS — only honest fallback warnings (no model yet).
    const std::string dir = repoRoot + "/assets/characters/cthulhu_avatar";
    if (!fs::is_directory(dir)) {
        std::cout << "(skip) no assets/characters under '" << repoRoot
                  << "'\n";
        return;
    }
    CharacterPackage pkg = loadPkg(dir);
    CHECK(pkg.defOk);
    DeepValidationReport r = CharacterValidator::validateDeep(pkg);
    CHECK(r.grade != PackageGrade::Invalid);
    CHECK(r.errors.empty());
    CHECK(hasSub(r.warnings, "procedural fallback"));
}

static void testDeepMinimalExample(const std::string& repoRoot) {
    // echo_of_the_void: def-only package must validate with warnings,
    // never errors — the graceful-fallback proof.
    const std::string dir = repoRoot + "/assets/characters/echo_of_the_void";
    if (!fs::is_directory(dir)) {
        std::cout << "(skip) no assets/characters under '" << repoRoot
                  << "'\n";
        return;
    }
    CharacterPackage pkg = loadPkg(dir);
    CHECK(pkg.defOk);
    DeepValidationReport r = CharacterValidator::validateDeep(pkg);
    CHECK(r.grade == PackageGrade::Warnings);
    CHECK(r.errors.empty());
    CHECK(hasSub(r.warnings, "slot empty"));
}

// --- rig mapper hardening -------------------------------------------------

static void testRigAliases() {
    // Blender / Rigify / Mixamo naming variants.
    const std::vector<std::string> bones = {
        "Hips", "Spine", "Neck",           // Blender defaults
        "Clavicle_L", "Clavicle_R",        // Blender clavicles
        "Forearm.L", "Forearm.R",          // Blender dotted
        "DEF-thigh.L", "DEF-thigh.R",      // Rigify deform prefix
        "shin.L", "shin.R",                // Rigify
        "mixamorig:Head",                  // Mixamo (dup of Neck slot race)
    };
    RigMapping m = RigMapper::mapBones(bones);
    auto mapped = [&](const std::string& eng) {
        for (const auto& b : m.bones)
            if (b.engineBone == eng) return b.mapped;
        return false;
    };
    CHECK(mapped("hips"));
    CHECK(mapped("spine"));
    CHECK(mapped("head"));       // Neck -> head alias
    CHECK(mapped("upperArmL"));   // Clavicle_L
    CHECK(mapped("upperArmR"));   // Clavicle_R
    CHECK(mapped("lowerArmL"));   // Forearm.L
    CHECK(mapped("lowerArmR"));   // Forearm.R
    CHECK(mapped("upperLegL"));   // DEF-thigh.L
    CHECK(mapped("upperLegR"));   // DEF-thigh.R
    CHECK(mapped("lowerLegL"));   // shin.L
    CHECK(mapped("lowerLegR"));   // shin.R
}

static void testRigDiagnostics() {
    RigMapping m = RigMapper::mapBones({"mixamorig:Hips", "mixamorig:Spine"});
    auto lines = m.diagnosticLines();
    CHECK(lines.size() == 11); // one per engine bone
    CHECK(hasSub(lines, "hips"));
    CHECK(hasSub(lines, "mixamorig:Hips"));
    CHECK(hasSub(lines, "(unmapped)")); // head etc. have no match
    // Exact match reports 100%.
    CHECK(hasSub(lines, "100%"));
}

// --- procedural clips ------------------------------------------------------

static void checkClipSane(const AnimationClip& c, const std::string& name,
                          bool loop) {
    CHECK(c.name == name);
    CHECK(c.durationSeconds > 0.0);
    CHECK(c.loop == loop);
    CHECK(!c.tracks.empty());
    // Every engine bone the state machine cares about is present.
    for (const std::string& b : humanoidBones())
        CHECK(c.tracks.count(b) == 1);
    // Sampling works across the clip.
    auto pose = c.sampleAt(c.durationSeconds * 0.5);
    (void)pose;
    auto poseEnd = c.sampleAt(c.durationSeconds * 2.0); // wraps/clamps
    (void)poseEnd;
}

static void testNewClips() {
    checkClipSane(makeCast(), "Cast", false);
    checkClipSane(makeStunned(), "Stunned", false);
    checkClipSane(makeChannel(), "Channel", true);
    checkClipSane(makeCastWave(), "CastWave", false);
}

static void testFallbackBinding() {
    AnimationStateMachine sm;
    bindProceduralFallbacks(sm);
    CHECK(sm.hasClip(AnimationState::Idle));
    CHECK(sm.hasClip(AnimationState::Walk));
    CHECK(sm.hasClip(AnimationState::Run));
    CHECK(sm.hasClip(AnimationState::Attack));
    CHECK(sm.hasClip(AnimationState::Death));
    CHECK(sm.hasClip(AnimationState::Cast));
    CHECK(sm.hasClip(AnimationState::Stunned));
    CHECK(sm.hasClip(AnimationState::Channel));
    CHECK(sm.hasClip(AnimationState::CastWave));
}

int main(int argc, char** argv) {
    const std::string repoRoot = (argc > 1) ? argv[1] : ".";
    testDeepClean();
    testDeepInvalid();
    testDeepDefWarnings();
    testDeepModelMagic();
    testDeepRigMapWarnings();
    testDeepTemplateAvatar(repoRoot);
    testDeepMinimalExample(repoRoot);
    testRigAliases();
    testRigDiagnostics();
    testNewClips();
    testFallbackBinding();
    std::cout << "wave14 checks: " << checks << ", failures: " << failures
              << "\n";
    return failures == 0 ? 0 : 1;
}
