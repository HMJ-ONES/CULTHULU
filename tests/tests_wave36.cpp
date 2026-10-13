// CULT-ULHU wave 36 tests: ability visual-effect (particle) data.
//
// Guards the wave-36 graphics pass:
//   - assets/fx/ability_fx.def parses and ships one preset per ability
//     effect kind (every Q/F/R kit across the 20 playable characters
//     resolves to a real particle recipe);
//   - the `fx = <id>` override in character.def parses and resolves;
//   - every preset stays inside the low-spec host budget (particle
//     counts, additive fill-rate cap, looping-emitter cap, durations,
//     sizes);
//   - colors are valid hex, ranges are ordered, emitters are known.
// Wave 43 extends the catalog: one preset per RMB HeavyAttackKind
// (fxForHeavyAttack), the [rightclick] `fx` override, and the rock-kit
// presets resolved from AbilityCast events by fxForEvent().

#include "characters/AbilityFx.h"
#include "characters/CharacterDefParser.h"
#include "characters/CharacterPackageLoader.h"
#include "characters/CharacterRegistry.h"

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
    std::ifstream p("assets/fx/ability_fx.def");
    if (p) return "";
    return "../";
}

static bool isHexColor(const std::string& s) {
    if (s.size() != 6) return false;
    for (char c : s) {
        const bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                         (c >= 'A' && c <= 'F');
        if (!hex) return false;
    }
    return true;
}

// The catalog parses, and there is one preset per known effect kind.
static void testCatalogParses() {
    FxLoadResult r =
        loadFxLibrary(repoPrefix() + "assets/fx/ability_fx.def");
    CHECK(r.ok);
    if (!r.ok) {
        std::cout << "  fx library error: " << r.error << "\n";
        return;
    }
    std::cout << "  " << r.library.presets.size() << " fx presets\n";
    CHECK(r.library.presets.size() >= 11);
    static const char* kinds[] = {"aoe_damage", "fear_aura", "summon",
                                  "buff",       "projectile", "heal",
                                  "shield",     "debuff",     "dash",
                                  "pull",       "stun"};
    for (const char* k : kinds) {
        const FxPreset* p = r.library.find(k);
        if (!p) std::cout << "  MISSING preset for effect kind '" << k
                          << "'\n";
        CHECK(p != nullptr);
    }
    // Wave 43: one preset per RMB heavy-attack kind, plus the
    // event-driven rock-kit presets.
    static const char* heavyKinds[] = {"MeleeHeavy", "MindControl",
                                       "AcidSpit",   "EldritchGrasp"};
    for (const char* k : heavyKinds) {
        const FxPreset* p = r.library.find(k);
        if (!p) std::cout << "  MISSING preset for heavy-attack kind '"
                          << k << "'\n";
        CHECK(p != nullptr);
    }
    static const char* rockPresets[] = {"rock_rip", "rock_hurl",
                                        "rock_shatter"};
    for (const char* k : rockPresets) {
        if (!r.library.find(k))
            std::cout << "  MISSING rock preset '" << k << "'\n";
        CHECK(r.library.find(k) != nullptr);
    }
    // Beauty pass: the catalog also ships the event-driven destruction
    // preset used when buildings, districts, and cities fall.
    CHECK(r.library.find("raze") != nullptr);
    // Every preset has a display name and a texture hint.
    for (const auto& p : r.library.presets) {
        CHECK(!p.displayName.empty());
        CHECK(!p.texture.empty());
    }
}

// Every preset stays inside the low-spec host budget.
static void testBudgetLimits() {
    FxLoadResult r =
        loadFxLibrary(repoPrefix() + "assets/fx/ability_fx.def");
    CHECK(r.ok);
    if (!r.ok) return;
    for (const auto& p : r.library.presets) {
        CHECK(p.count >= 1 && p.count <= 256);
        if (p.additive) CHECK(p.count <= 128); // fill-rate guard
        if (p.loop) CHECK(p.count <= 64);      // continuous emitters
        CHECK(p.durationSec >= 0.2f && p.durationSec <= 4.0f);
        CHECK(p.sizeMax <= 6.0f);
        CHECK(p.lifetimeMax <= 6.0f);
        CHECK(p.spreadDeg >= 0.0f && p.spreadDeg <= 180.0f);
        CHECK(p.lifetimeMin <= p.lifetimeMax);
        CHECK(p.speedMin <= p.speedMax);
        CHECK(p.sizeMin <= p.sizeMax);
        CHECK(isHexColor(p.color0));
        CHECK(isHexColor(p.color1));
        if (p.count > 256 || p.sizeMax > 6.0f)
            std::cout << "  BUDGET VIOLATION in preset '" << p.id << "'\n";
    }
}

// Every Q/F/R ability of every playable character resolves to a preset
// (explicit `fx` override or the effect-kind default).
static void testAllKitsResolve() {
    FxLoadResult fr =
        loadFxLibrary(repoPrefix() + "assets/fx/ability_fx.def");
    CHECK(fr.ok);
    if (!fr.ok) return;
    CharacterRegistry registry;
    CharacterPackageLoader loader(registry);
    const int loaded =
        loader.scanAndLoad(repoPrefix() + "assets/characters/");
    CHECK(loaded >= 20);
    static const char* ids[] = {
        "cthulhu_avatar", "yog_sothoth",   "nyarlathotep", "shub_niggurath",
        "mi_go",          "shoggoths",      "bokrug",       "nodens",
        "dagon",          "mother_hydra",   "ghatanothoa",  "nug_and_yeb",
        "rhan_tegoth",    "sghllor",        "hastur",       "yig",
        "bast",           "elder_mind",     "howling_eye",  "hypnos",
    };
    int spells = 0, resolved = 0;
    for (const char* id : ids) {
        const CharacterPackage* pkg = loader.find(id);
        if (!pkg || !pkg->defOk) {
            std::cout << "  UNABLE to load package '" << id << "'\n";
            ++failures;
            continue;
        }
        const CharacterDef& d = pkg->def;
        const SpellDef* list[3] = {&d.qAbility, &d.fAbility, &d.rAbility};
        for (const SpellDef* s : list) {
            if (s->id.empty()) continue;
            ++spells;
            const FxPreset* p = fxForSpell(*s, fr.library);
            if (!p) {
                std::cout << "  NO FX for " << d.id << " spell '"
                          << s->id << "' (effect=" << s->effectKind
                          << ", fx=" << s->fxPreset << ")\n";
            } else {
                ++resolved;
            }
            CHECK(p != nullptr);
        }
    }
    std::cout << "  " << resolved << "/" << spells
              << " kit spells resolve to an fx preset\n";
    CHECK(spells == 60); // 20 characters x Q/F/R
}

// The `fx` override parses and wins over the effect-kind default.
static void testFxOverrideParsing() {
    const std::string text =
        "id = fx_test_char\n"
        "display_name = Fx Test\n"
        "[q]\n"
        "id = test_slam\n"
        "name = Test Slam\n"
        "effect = aoe_damage\n"
        "cooldown = 8\n"
        "fx = stun\n"; // Mind Shatter preset, not the default aoe_damage
    ParseResult r = parseCharacterDef(text);
    CHECK(r.ok);
    CHECK(r.def.qAbility.fxPreset == "stun");

    FxLoadResult fr =
        loadFxLibrary(repoPrefix() + "assets/fx/ability_fx.def");
    CHECK(fr.ok);
    if (!fr.ok) return;
    const FxPreset* p = fxForSpell(r.def.qAbility, fr.library);
    CHECK(p != nullptr);
    if (p) CHECK(p->id == "stun"); // override wins over aoe_damage default

    // No override: falls back to the effect-kind preset.
    SpellDef plain;
    plain.id = "plain_slam";
    plain.effectKind = "aoe_damage";
    const FxPreset* d = fxForSpell(plain, fr.library);
    CHECK(d != nullptr);
    CHECK(d->id == "aoe_damage");

    // Unknown override id: resolves to nothing (authoring error the
    // driver/validator surfaces, not a crash).
    SpellDef bad;
    bad.id = "bad_slam";
    bad.effectKind = "aoe_damage";
    bad.fxPreset = "no_such_preset";
    CHECK(fxForSpell(bad, fr.library) == nullptr);
}

// Negative tests: malformed catalog text is rejected, not half-loaded.
static void testCatalogParseErrors() {
    // Unknown emitter.
    FxLoadResult a = parseFxLibraryText(
        "[p]\nemitter = explosion\ncount = 10\n");
    CHECK(!a.ok);
    // Bad hex color.
    FxLoadResult b = parseFxLibraryText(
        "[p]\ncount = 10\ncolors = zzzzzz,ffffff\n");
    CHECK(!b.ok);
    // Duplicate preset id.
    FxLoadResult c = parseFxLibraryText(
        "[p]\ncount = 10\n[p]\ncount = 12\n");
    CHECK(!c.ok);
    // Min > max range.
    FxLoadResult d = parseFxLibraryText(
        "[p]\ncount = 10\nlifetime = 2.0,1.0\n");
    CHECK(!d.ok);
    // Good minimal preset parses.
    FxLoadResult e = parseFxLibraryText(
        "[p]\ndisplay_name = Plain\ncount = 10\ncolors = 000000,ffffff\n");
    CHECK(e.ok);
    CHECK(e.library.presets.size() == 1);
    CHECK(e.library.find("p") != nullptr);
}

// Event-driven (non-spell) FX: destruction events resolve to the
// `raze` preset, and the announced match punctuation marks (streaks,
// kills, relics, captures, triumphs) resolve to the semantically
// nearest existing preset. Quiet background events stay unmapped.
static void testFxForEvent() {
    FxLoadResult r =
        loadFxLibrary(repoPrefix() + "assets/fx/ability_fx.def");
    CHECK(r.ok);
    if (!r.ok) return;
    const FxPreset* raze = fxForEvent(EventType::DistrictRazed, r.library);
    CHECK(raze != nullptr);
    if (raze) {
        CHECK(raze->id == "raze");
        CHECK(!raze->displayName.empty());
    }
    CHECK(fxForEvent(EventType::CityDestroyed, r.library) == raze);
    CHECK(fxForEvent(EventType::CityBuildingDestroyed, r.library) == raze);

    // 5 PM beauty (2026-10-12): the announced-events FX sweep — the
    // game's punctuation marks now resolve to existing presets instead
    // of nullptr. No new FX assets; the budget test below still guards
    // the catalog size.
    const FxPreset* stun = r.library.find("stun");
    const FxPreset* aoe = r.library.find("aoe_damage");
    const FxPreset* summon = r.library.find("summon");
    const FxPreset* aura = r.library.find("fear_aura");
    const FxPreset* buff = r.library.find("buff");
    CHECK(stun != nullptr);
    CHECK(aoe != nullptr);
    CHECK(summon != nullptr);
    CHECK(aura != nullptr);
    CHECK(buff != nullptr);
    CHECK(fxForEvent(EventType::KillStreak, r.library) == stun);
    CHECK(fxForEvent(EventType::StreakEnded, r.library) == stun);
    CHECK(fxForEvent(EventType::OvertimeStarted, r.library) == stun);
    CHECK(fxForEvent(EventType::FirstBlood, r.library) == aoe);
    CHECK(fxForEvent(EventType::PlayerKilled, r.library) == aoe);
    CHECK(fxForEvent(EventType::DholeAmbush, r.library) == aoe);
    CHECK(fxForEvent(EventType::RelicManifested, r.library) == summon);
    CHECK(fxForEvent(EventType::RelicClaimed, r.library) == summon);
    CHECK(fxForEvent(EventType::MaddeningWhispers, r.library) == aura);
    CHECK(fxForEvent(EventType::PointCaptured, r.library) == buff);
    CHECK(fxForEvent(EventType::AchievementUnlocked, r.library) == buff);
    CHECK(fxForEvent(EventType::MatchPoint, r.library) == buff);
    CHECK(fxForEvent(EventType::PlayerRespawned, r.library) == buff);
    CHECK(fxForEvent(EventType::DiscoveryMade, r.library) == buff);
    CHECK(fxForEvent(EventType::DungeonCompleted, r.library) == buff);
    // Micro-dev (2026-10-12): UltCharged shipped after the sweep —
    // folded in here per the 19:12 note, reusing the buff preset.
    CHECK(fxForEvent(EventType::UltCharged, r.library) == buff);
    CHECK(fxForEvent(EventType::GreatOldOneSlain, r.library) == raze);
    // Micro-dev (2026-10-12): the low-health ping resolves to the same
    // alarm burst as the GOO-under-attack warning.
    CHECK(fxForEvent(EventType::PlayerLowHp, r.library) == stun);
    // Quiet background events stay unmapped (still text-only / silent).
    CHECK(fxForEvent(EventType::CombatStarted, r.library) == nullptr);
    CHECK(fxForEvent(EventType::SuppliesGathered, r.library) == nullptr);
    CHECK(fxForEvent(EventType::BuildProgress, r.library) == nullptr);
}

// Wave 43: RMB heavy attacks resolve to presets (kind default, with
// an [rightclick] `fx` override), and rock-kit AbilityCast events
// resolve to the rock_* presets through the fxForEvent() overload.
static void testHeavyAttackFx() {
    FxLoadResult fr =
        loadFxLibrary(repoPrefix() + "assets/fx/ability_fx.def");
    CHECK(fr.ok);
    if (!fr.ok) return;

    // Each heavy-attack kind resolves to the preset named for it.
    static const HeavyAttackKind kinds[] = {
        HeavyAttackKind::MeleeHeavy, HeavyAttackKind::MindControl,
        HeavyAttackKind::AcidSpit, HeavyAttackKind::EldritchGrasp};
    for (HeavyAttackKind k : kinds) {
        HeavyAttackDef h;
        h.kind = k;
        const FxPreset* p = fxForHeavyAttack(h, fr.library);
        if (!p) std::cout << "  NO FX for heavy-attack kind '"
                          << heavyAttackKindName(k) << "'\n";
        CHECK(p != nullptr);
        if (p) CHECK(p->id == heavyAttackKindName(k));
    }

    // Explicit override wins; a bad one resolves to nothing.
    HeavyAttackDef over;
    over.kind = HeavyAttackKind::AcidSpit;
    over.fxPreset = "stun";
    const FxPreset* o = fxForHeavyAttack(over, fr.library);
    CHECK(o != nullptr);
    if (o) CHECK(o->id == "stun");
    HeavyAttackDef bad;
    bad.kind = HeavyAttackKind::MeleeHeavy;
    bad.fxPreset = "no_such_preset";
    CHECK(fxForHeavyAttack(bad, fr.library) == nullptr);

    // The [rightclick] `fx` line parses into HeavyAttackDef::fxPreset.
    ParseResult pr = parseCharacterDef(
        "id = fx_rmb_char\n"
        "display_name = Fx Rmb Test\n"
        "[rightclick]\n"
        "kind = EldritchGrasp\n"
        "name = Test Grasp\n"
        "fx = rock_shatter\n");
    CHECK(pr.ok);
    CHECK(pr.def.rightClick.kind == HeavyAttackKind::EldritchGrasp);
    CHECK(pr.def.rightClick.fxPreset == "rock_shatter");
    const FxPreset* rp = fxForHeavyAttack(pr.def.rightClick, fr.library);
    CHECK(rp != nullptr);
    if (rp) CHECK(rp->id == "rock_shatter");

    // Rock-kit AbilityCast events (slot 3) resolve to rock presets.
    auto rockEvent = [](const char* tag) {
        GameEvent e(EventType::AbilityCast);
        e.tag = tag;
        e.amount = 3; // slot 3 = RMB
        return e;
    };
    const FxPreset* rip = fxForEvent(rockEvent(kRockRipTag), fr.library);
    CHECK(rip != nullptr);
    if (rip) CHECK(rip->id == "rock_rip");
    const FxPreset* hurl = fxForEvent(rockEvent(kRockHurlTag), fr.library);
    CHECK(hurl != nullptr);
    if (hurl) CHECK(hurl->id == "rock_hurl");
    const FxPreset* sh1 =
        fxForEvent(rockEvent("the rock shatters"), fr.library);
    CHECK(sh1 != nullptr);
    if (sh1) CHECK(sh1->id == "rock_shatter");
    const FxPreset* sh2 =
        fxForEvent(rockEvent("the rock crumbles to dust"), fr.library);
    CHECK(sh2 != nullptr);
    if (sh2) CHECK(sh2->id == "rock_shatter");

    // Ordinary RMB casts are NOT rock events: the event overload
    // returns nullptr and the caller falls back to fxForHeavyAttack.
    CHECK(fxForEvent(rockEvent("Dominate Mind"), fr.library) == nullptr);
    // Same rock tag outside the RMB slot: no rock visual.
    GameEvent wrongSlot(EventType::AbilityCast);
    wrongSlot.tag = kRockRipTag;
    wrongSlot.amount = 0;
    CHECK(fxForEvent(wrongSlot, fr.library) == nullptr);

    // The GameEvent overload still delegates destruction events.
    GameEvent razed(EventType::DistrictRazed);
    const FxPreset* raze = fxForEvent(razed, fr.library);
    CHECK(raze != nullptr);
    if (raze) CHECK(raze->id == "raze");
    GameEvent other(EventType::CombatStarted);
    CHECK(fxForEvent(other, fr.library) == nullptr);

    // Every playable character's RMB resolves to a preset.
    CharacterRegistry registry;
    CharacterPackageLoader loader(registry);
    const int loaded =
        loader.scanAndLoad(repoPrefix() + "assets/characters/");
    CHECK(loaded >= 20);
    static const char* ids[] = {
        "cthulhu_avatar", "yog_sothoth",   "nyarlathotep", "shub_niggurath",
        "mi_go",          "shoggoths",      "bokrug",       "nodens",
        "dagon",          "mother_hydra",   "ghatanothoa",  "nug_and_yeb",
        "rhan_tegoth",    "sghllor",        "hastur",       "yig",
        "bast",           "elder_mind",     "howling_eye",  "hypnos",
    };
    int rmbs = 0;
    for (const char* id : ids) {
        const CharacterPackage* pkg = loader.find(id);
        if (!pkg || !pkg->defOk) {
            std::cout << "  UNABLE to load package '" << id << "'\n";
            ++failures;
            continue;
        }
        ++rmbs;
        const FxPreset* p = fxForHeavyAttack(pkg->def.rightClick,
                                             fr.library);
        if (!p) std::cout << "  NO FX for " << id << " RMB\n";
        CHECK(p != nullptr);
    }
    std::cout << "  " << rmbs << "/20 character RMBs resolve to an fx preset\n";
}

int main() {
    testCatalogParses();
    testBudgetLimits();
    testAllKitsResolve();
    testFxOverrideParsing();
    testCatalogParseErrors();
    testFxForEvent();
    testHeavyAttackFx();
    std::cout << "wave36: " << checks << " checks, " << failures
              << " failures\n";
    return failures == 0 ? 0 : 1;
}
