// CULT-ULHU wave 7e tests: plug-and-play character packages.
// Covers: character.def parsing, package discovery/loading, validator
// warnings, rig auto-mapper against mocked Mixamo + Rigify bone lists,
// fallback behavior with model-less packages, hot-load, and the
// Cthulhu Avatar migration end-to-end.
//
// Usage: tests_wave7e [repo_root]  (repo root so the test can find
// assets/characters/; defaults to ".")

#include "characters/CharacterDefParser.h"
#include "characters/CharacterPackageLoader.h"
#include "characters/CharacterRegistry.h"
#include "characters/CharacterValidator.h"
#include "characters/CthulhuAvatar.h"
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

static const char* kDefText = R"(
id = test_wraith
display_name = Test Wraith
flavor = A test that haunts.
max_hp = 120
move_speed = 7.5
max_stamina = 80
melee_combo = wraith_flurry
rmb_ability = wave_of_domination

[q]
id = wail
name = Wail
cooldown = 5
stamina_cost = 10
effect = fear_aura
power = 2
range = 6

[rightclick]
kind = AcidSpit
name = Spit
damage_mult = 1.2
range = 12
)";

static void testParseDef() {
    ParseResult r = parseCharacterDef(kDefText);
    CHECK(r.ok);
    CHECK(r.def.id == "test_wraith");
    CHECK(r.def.displayName == "Test Wraith");
    CHECK(std::fabs(r.def.maxHp - 120.0f) < 0.01f);
    CHECK(std::fabs(r.def.moveSpeed - 7.5f) < 0.01f);
    CHECK(r.def.qAbility.id == "wail");
    CHECK(std::fabs(r.def.qAbility.cooldownSec - 5.0f) < 0.01f);
    CHECK(r.def.qAbility.effectKind == "fear_aura");
    CHECK(r.def.rightClick.kind == HeavyAttackKind::AcidSpit);
    CHECK(r.def.rmbAbilityId == "wave_of_domination");
    // Unset optionals keep defaults.
    CHECK(r.def.fAbility.id.empty());
    CHECK(r.def.passiveId.empty());
}

static void testParseErrors() {
    ParseResult r = parseCharacterDef("max_hp = abc\nid = x\n");
    CHECK(!r.ok);
    ParseResult r2 = parseCharacterDef("max_hp = 10\n"); // no id
    CHECK(!r2.ok);
    ParseResult r3 = parseCharacterDef("[rightclick]\nkind = Bogus\n");
    CHECK(!r3.ok); // unknown kind is an error; id missing too
}

static void testDiscovery(const std::string& root) {
    const std::string base = root + "/w7e_pkgs";
    fs::remove_all(base);
    writeFile(base + "/wraith/character.def", kDefText);
    writeFile(base + "/broken/character.def", "max_hp = nope\nid = broken\n");
    fs::create_directories(base + "/empty"); // no character.def

    CharacterRegistry reg;
    CharacterPackageLoader loader(reg);
    const int n = loader.scanAndLoad(base);
    CHECK(n == 1);
    CHECK(reg.count() == 1);
    CHECK(reg.get("test_wraith") != nullptr);
    CHECK(loader.loadErrors().size() == 2); // broken def + missing def
    const CharacterPackage* p = loader.find("wraith");
    CHECK(p != nullptr);
    CHECK(!p->hasModel);
    CHECK(p->clipFiles.empty());
    fs::remove_all(base);
}

static void testValidatorFallbacks() {
    CharacterPackage pkg;
    pkg.folderName = "wraith";
    ParseResult r = parseCharacterDef(kDefText);
    pkg.def = r.def;
    pkg.defOk = r.ok;
    // Nothing else: no model, no clips, no rig.map, no bones.list.

    ValidationReport rep = CharacterValidator::validate(pkg);
    CHECK(rep.ok); // still playable!
    bool sawModel = false, sawClips = false;
    for (const auto& w : rep.warnings) {
        if (w.find("model slot empty") != std::string::npos) sawModel = true;
        if (w.find("procedural fallback") != std::string::npos) sawClips = true;
    }
    CHECK(sawModel);
    CHECK(sawClips);

    // Unknown RMB kit -> warning, not error.
    pkg.def.rmbAbilityId = "nope_not_real";
    ValidationReport rep2 = CharacterValidator::validate(pkg);
    CHECK(rep2.ok);
    bool sawKit = false;
    for (const auto& w : rep2.warnings) {
        if (w.find("unknown RMB kit") != std::string::npos) sawKit = true;
    }
    CHECK(sawKit);
}

static void testRigMapperMixamo() {
    const std::vector<std::string> mixamo = {
        "mixamorig:Hips", "mixamorig:Spine", "mixamorig:Spine1",
        "mixamorig:Neck", "mixamorig:Head", "mixamorig:LeftShoulder",
        "mixamorig:LeftArm", "mixamorig:LeftForeArm", "mixamorig:LeftHand",
        "mixamorig:RightShoulder", "mixamorig:RightArm",
        "mixamorig:RightForeArm", "mixamorig:RightHand",
        "mixamorig:LeftUpLeg", "mixamorig:LeftLeg", "mixamorig:LeftFoot",
        "mixamorig:RightUpLeg", "mixamorig:RightLeg", "mixamorig:RightFoot",
    };
    RigMapping m = RigMapper::mapBones(mixamo);
    CHECK(m.mappedCount() == 11);
    CHECK(m.unmapped().empty());
    CHECK(m.overallConfidence() > 0.85f);
    for (const auto& b : m.bones) {
        if (b.engineBone == "hips") CHECK(b.fbxBone == "mixamorig:Hips");
        if (b.engineBone == "upperArmL") CHECK(b.fbxBone == "mixamorig:LeftArm");
        if (b.engineBone == "lowerLegR") CHECK(b.fbxBone == "mixamorig:RightLeg");
    }
}

static void testRigMapperRigify() {
    const std::vector<std::string> rigify = {
        "root", "torso", "hips", "spine", "chest", "neck", "head",
        "shoulder.L", "upper_arm.L", "forearm.L", "hand.L",
        "shoulder.R", "upper_arm.R", "forearm.R", "hand.R",
        "thigh.L", "shin.L", "foot.L", "thigh.R", "shin.R", "foot.R",
    };
    RigMapping m = RigMapper::mapBones(rigify);
    CHECK(m.mappedCount() == 11);
    CHECK(m.unmapped().empty());
    for (const auto& b : m.bones) {
        if (b.engineBone == "upperLegL") CHECK(b.fbxBone == "thigh.L");
        if (b.engineBone == "lowerArmR") CHECK(b.fbxBone == "forearm.R");
    }
}

static void testRigMapperUnknown() {
    const std::vector<std::string> weird = {"Bone_001", "Bone_002"};
    RigMapping m = RigMapper::mapBones(weird);
    CHECK(m.mappedCount() == 0);
    CHECK(m.unmapped().size() == 11);
    CHECK(m.overallConfidence() == 0.0f);

    // Explicit rig.map parsing.
    std::vector<std::string> warns;
    RigMapping m2 = RigMapper::parseRigMap(
        "hips = MyHips\nspine = MySpine\nbogus_bone = X\nnoequals\n", warns);
    CHECK(m2.mappedCount() == 2);
    CHECK(warns.size() == 2); // unknown bone + malformed line
}

static void testHotLoad(const std::string& root) {
    const std::string base = root + "/w7e_hot";
    fs::remove_all(base);
    writeFile(base + "/wraith/character.def", kDefText);

    CharacterRegistry reg;
    CharacterPackageLoader loader(reg);
    CHECK(loader.scanAndLoad(base) == 1);
    // Hot-load a second character at runtime.
    writeFile(base + "/ghoul/character.def",
              "id = test_ghoul\ndisplay_name = Test Ghoul\nmax_hp = 90\n");
    CHECK(loader.hotLoad(base + "/ghoul"));
    CHECK(reg.count() == 2);
    CHECK(loader.find("ghoul") != nullptr);
    // Duplicate id rejected.
    CHECK(!loader.hotLoad(base + "/wraith"));
    fs::remove_all(base);
}

static void testMigrationEndToEnd(const std::string& repoRoot) {
    // The Cthulhu Avatar template package must load and match the
    // code-built def field-for-field (the pipeline proving itself).
    CharacterRegistry reg;
    CharacterPackageLoader loader(reg);
    const int n = loader.scanAndLoad(repoRoot + "/assets/characters");
    const CharacterPackage* pkg = loader.find("cthulhu_avatar");
    if (pkg == nullptr) {
        // Assets dir not present (e.g. running from build/ with no args):
        // the pipeline itself is exercised by the temp-root tests above.
        std::cout << "(skip) no assets/characters under '" << repoRoot
                  << "'; template-package check skipped\n";
        return;
    }
    CHECK(n >= 1);
    CHECK(pkg->defOk);

    const CharacterDef code = makeCthulhuAvatar();
    CHECK(pkg->def.id == code.id);
    CHECK(pkg->def.displayName == code.displayName);
    CHECK(std::fabs(pkg->def.maxHp - code.maxHp) < 0.01f);
    CHECK(std::fabs(pkg->def.moveSpeed - code.moveSpeed) < 0.01f);
    CHECK(pkg->def.qAbility.name == code.qAbility.name);
    CHECK(std::fabs(pkg->def.qAbility.effectPower -
                    code.qAbility.effectPower) < 0.01f);
    CHECK(pkg->def.rmbAbilityId == code.rmbAbilityId);
    CHECK(pkg->def.rightClick.name == code.rightClick.name);

    // Registry got it through the loader (no code changes needed).
    CHECK(reg.get("cthulhu_avatar") != nullptr);

    // Validator on the template: playable, warns only about the
    // (intentionally) absent model/clips.
    ValidationReport rep = CharacterValidator::validate(*pkg);
    CHECK(rep.ok);
}

int main(int argc, char** argv) {
    const std::string repoRoot = (argc > 1) ? argv[1] : ".";
    const std::string tmpRoot = "/tmp/w7e_test_root";
    testParseDef();
    testParseErrors();
    testDiscovery(tmpRoot);
    testValidatorFallbacks();
    testRigMapperMixamo();
    testRigMapperRigify();
    testRigMapperUnknown();
    testHotLoad(tmpRoot);
    testMigrationEndToEnd(repoRoot);
    fs::remove_all(tmpRoot);
    std::cout << "wave7e checks: " << checks << ", failures: " << failures
              << "\n";
    return failures == 0 ? 0 : 1;
}
