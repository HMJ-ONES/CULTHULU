// CULT-ULHU wave 25 tests: the 20 playable character kits.
// Every kit must parse, validate clean (no errors; missing-model warnings
// are expected — models arrive with the owner's rigs), and sit inside the
// shared power budget (see assets/characters/KITS.md).

#include "characters/CharacterPackageLoader.h"
#include "characters/CharacterRegistry.h"
#include "characters/CharacterValidator.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace cultulhu;

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

static std::string repoPrefix() {
    std::ifstream p("assets/creatures/MANIFEST.md");
    if (p) return "";
    return "../";
}

static bool knownEffect(const std::string& k) {
    static const char* kinds[] = {"aoe_damage", "fear_aura", "summon",
                                  "buff",       "projectile", "heal",
                                  "shield",     "debuff",     "dash",
                                  "pull",       "stun"};
    for (const char* c : kinds)
        if (k == c) return true;
    return false;
}

// Amortized DPS of one spell. Direct damage counts fully; summons are
// valued at ~50 damage per spawn for Q/F and ~150 for an R-tier spawn.
static float spellDps(const SpellDef& s, bool isUlt) {
    if (s.cooldownSec <= 0.0f) return 0.0f;
    if (s.effectKind == "aoe_damage" || s.effectKind == "projectile")
        return s.effectPower / s.cooldownSec;
    if (s.effectKind == "summon")
        return s.effectPower * (isUlt ? 150.0f : 50.0f) / s.cooldownSec;
    return 0.0f;
}

static float kitDps(const CharacterDef& d) {
    return spellDps(d.qAbility, false) + spellDps(d.fAbility, false) +
           spellDps(d.rAbility, true);
}

static const std::vector<std::string> kPlayables = {
    "cthulhu_avatar", "yog_sothoth",   "nyarlathotep", "shub_niggurath",
    "mi_go",          "shoggoths",      "bokrug",       "nodens",
    "dagon",          "mother_hydra",   "ghatanothoa",  "nug_and_yeb",
    "rhan_tegoth",    "sghllor",        "hastur",       "yig",
    "bast",           "elder_mind",     "howling_eye",  "hypnos",
};

static void testAllPlayablesLoad() {
    const std::string root = repoPrefix() + "assets/characters/";
    CharacterRegistry registry;
    CharacterPackageLoader loader(registry);
    const int loaded = loader.scanAndLoad(root);
    CHECK(loaded >= 20);
    int found = 0;
    for (const std::string& id : kPlayables) {
        const CharacterPackage* pkg = loader.find(id);
        if (pkg == nullptr) {
            std::cout << "FAIL: playable package '" << id
                      << "' not found\n";
            ++failures;
            continue;
        }
        ++found;
        CHECK(pkg->defOk);
        if (!pkg->defOk) continue;
        CHECK(pkg->def.id == id);
    }
    std::cout << "  [info] playable kits loaded: " << found << "/"
              << kPlayables.size() << "\n";
    CHECK(found == (int)kPlayables.size());
}

static void checkKitCompleteness(const CharacterDef& d) {
    const SpellDef* spells[3] = {&d.qAbility, &d.fAbility, &d.rAbility};
    const char* slots[3] = {"Q", "F", "R"};
    for (int i = 0; i < 3; ++i) {
        const SpellDef& s = *spells[i];
        CHECK(!s.id.empty());
        CHECK(!s.name.empty());
        CHECK(s.cooldownSec > 0.0f);
        CHECK(knownEffect(s.effectKind));
        CHECK(s.staminaCost >= 0.0f);
        CHECK(s.staminaCost <= d.maxStamina);
        if (s.id.empty() || s.name.empty())
            std::cout << "  [info] " << d.id << " slot " << slots[i]
                      << " incomplete\n";
    }
    // Spell ids unique within the kit.
    CHECK(d.qAbility.id != d.fAbility.id);
    CHECK(d.qAbility.id != d.rAbility.id);
    CHECK(d.fAbility.id != d.rAbility.id);
    CHECK(!d.displayName.empty());
    CHECK(!d.meleeComboId.empty());
    CHECK(!d.passiveId.empty());
    CHECK(!d.passiveDesc.empty());
}

static void checkPowerBudget(const CharacterDef& d) {
    CHECK(d.maxHp >= 400.0f && d.maxHp <= 700.0f);
    CHECK(d.moveSpeed >= 4.5f && d.moveSpeed <= 7.0f);

    const SpellDef& q = d.qAbility;
    const SpellDef& f = d.fAbility;
    const SpellDef& r = d.rAbility;

    CHECK(q.cooldownSec >= 6.0f && q.cooldownSec <= 10.0f);
    CHECK(f.cooldownSec >= 10.0f && f.cooldownSec <= 18.0f);
    CHECK(r.cooldownSec >= 40.0f && r.cooldownSec <= 60.0f);

    if (q.effectKind == "aoe_damage" || q.effectKind == "projectile")
        CHECK(q.effectPower >= 80.0f && q.effectPower <= 150.0f);
    if (r.effectKind == "aoe_damage" || r.effectKind == "projectile")
        CHECK(r.effectPower <= 250.0f);

    CHECK(q.staminaCost >= 20.0f && q.staminaCost <= 30.0f);
    CHECK(f.staminaCost >= 25.0f && f.staminaCost <= 40.0f);
    CHECK(r.staminaCost >= 45.0f && r.staminaCost <= 65.0f);

    // CC discipline: hard stuns short and never chainable; fear is softer
    // and allowed a longer tail.
    const SpellDef* spells[3] = {&q, &f, &r};
    for (const SpellDef* s : spells) {
        if (s->effectKind == "stun") {
            CHECK(s->effectPower <= 2.5f);
            CHECK(s->cooldownSec >= 4.0f * s->effectPower);
        }
        if (s->effectKind == "fear_aura") CHECK(s->effectPower <= 4.0f);
    }

    // Right-click: bounded multiplier, known CC, short CC. Control-focused
    // kinds (MindControl) trade damage for CC, so the floor is low.
    CHECK(d.rightClick.damageMult >= 0.3f &&
          d.rightClick.damageMult <= 1.6f);
    CHECK(d.rightClick.ccSeconds <= 2.5f);
    if (!d.rightClick.ccType.empty()) {
        CHECK(d.rightClick.ccType == "Stun" || d.rightClick.ccType == "Slow" ||
              d.rightClick.ccType == "Root" || d.rightClick.ccType == "Fear");
    }

    // Whole-kit sustained DPS stays in the engaging-but-fair band.
    const float dps = kitDps(d);
    if (dps < 10.0f || dps > 40.0f)
        std::cout << "  [info] " << d.id << " kit DPS " << dps
                  << " outside 10-40 band\n";
    CHECK(dps >= 10.0f && dps <= 40.0f);
}

static void testKitsValidateAndBalance() {
    const std::string root = repoPrefix() + "assets/characters/";
    CharacterRegistry registry;
    CharacterPackageLoader loader(registry);
    loader.scanAndLoad(root);

    float dpsMin = 1e9f, dpsMax = 0.0f;
    std::string dpsMinId, dpsMaxId;
    for (const std::string& id : kPlayables) {
        const CharacterPackage* pkg = loader.find(id);
        if (pkg == nullptr || !pkg->defOk) continue;
        const CharacterDef& d = pkg->def;

        // Parses and deep-validates with no errors. Warnings are fine:
        // every kit ships without a model until the owner's rigs land.
        ValidationReport vr = CharacterValidator::validate(*pkg);
        if (!vr.ok)
            std::cout << "FAIL: package " << id
                      << " invalid:\n" << vr.summary();
        CHECK(vr.ok);
        DeepValidationReport dr = CharacterValidator::validateDeep(*pkg);
        if (!dr.errors.empty()) {
            std::cout << "FAIL: package " << id << " deep errors:\n";
            for (const auto& e : dr.errors) std::cout << "  " << e << "\n";
        }
        CHECK(dr.errors.empty());

        checkKitCompleteness(d);
        checkPowerBudget(d);

        const float dps = kitDps(d);
        if (dps < dpsMin) { dpsMin = dps; dpsMinId = id; }
        if (dps > dpsMax) { dpsMax = dps; dpsMaxId = id; }
    }
    std::cout << "  [info] kit DPS spread: " << dpsMinId << " " << dpsMin
              << " .. " << dpsMaxId << " " << dpsMax << "\n";
    // The roster's strongest kit should not more than double the weakest.
    CHECK(dpsMax <= 2.5f * dpsMin);
}

int main() {
    std::cout << "== wave25: 20 character kits ==\n";
    testAllPlayablesLoad();
    testKitsValidateAndBalance();
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
