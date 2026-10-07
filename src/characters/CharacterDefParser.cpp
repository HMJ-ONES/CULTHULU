#include "characters/CharacterDefParser.h"

#include <sstream>

namespace cultulhu {

namespace {

std::string trim(std::string s) {
    const char* ws = " \t\r\n";
    s.erase(0, s.find_first_not_of(ws));
    if (!s.empty()) s.erase(s.find_last_not_of(ws) + 1);
    return s;
}

bool parseFloat(const std::string& s, float& out) {
    try {
        size_t n = 0;
        out = std::stof(s, &n);
        return n == s.size();
    } catch (...) {
        return false;
    }
}

bool parseHeavyKind(const std::string& s, HeavyAttackKind& out) {
    if (s == "MeleeHeavy") { out = HeavyAttackKind::MeleeHeavy; return true; }
    if (s == "MindControl") { out = HeavyAttackKind::MindControl; return true; }
    if (s == "AcidSpit") { out = HeavyAttackKind::AcidSpit; return true; }
    if (s == "EldritchGrasp") { out = HeavyAttackKind::EldritchGrasp; return true; }
    return false;
}

} // namespace

ParseResult parseCharacterDef(const std::string& text) {
    ParseResult r;
    std::string section; // "", "q", "f", "r", "rightclick", "passive"
    std::istringstream in(text);
    std::string line;
    int lineNo = 0;

    auto spellFor = [&](const std::string& sec) -> SpellDef* {
        if (sec == "q") return &r.def.qAbility;
        if (sec == "f") return &r.def.fAbility;
        if (sec == "r") return &r.def.rAbility;
        return nullptr;
    };

    while (std::getline(in, line)) {
        ++lineNo;
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            section = trim(line.substr(1, line.size() - 2));
            for (auto& ch : section) ch = static_cast<char>(std::tolower(ch));
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos) {
            r.error = "line " + std::to_string(lineNo) + ": no '='";
            return r;
        }
        const std::string key = trim(line.substr(0, eq));
        const std::string val = trim(line.substr(eq + 1));

        float f = 0.0f;
        const bool isNum =
            (key == "max_hp" || key == "move_speed" ||
             key == "max_stamina" || key == "cooldown" ||
             key == "stamina_cost" || key == "mana_cost" || key == "power" ||
             key == "range" || key == "damage_mult" || key == "cc_seconds");

        if (SpellDef* sp = spellFor(section)) {
            if (key == "id") sp->id = val;
            else if (key == "name") sp->name = val;
            else if (key == "flavor") sp->flavor = val;
            else if (key == "effect") sp->effectKind = val;
            else if (isNum && parseFloat(val, f)) {
                if (key == "cooldown") sp->cooldownSec = f;
                else if (key == "stamina_cost") sp->staminaCost = f;
                else if (key == "mana_cost") sp->manaCost = f;
                else if (key == "power") sp->effectPower = f;
                else if (key == "range") sp->range = f;
            } else if (isNum) {
                r.error = "line " + std::to_string(lineNo) +
                          ": bad number '" + val + "'";
                return r;
            }
            // Unknown keys inside ability sections are ignored (forward
            // compatibility for future fields).
        } else if (section == "rightclick") {
            HeavyAttackKind k;
            if (key == "kind" && parseHeavyKind(val, k))
                r.def.rightClick.kind = k;
            else if (key == "kind") {
                r.error = "line " + std::to_string(lineNo) +
                          ": unknown rightclick kind '" + val + "'";
                return r;
            } else if (key == "name")
                r.def.rightClick.name = val;
            else if (key == "cc")
                r.def.rightClick.ccType = val;
            else if (isNum && parseFloat(val, f)) {
                if (key == "damage_mult") r.def.rightClick.damageMult = f;
                else if (key == "range") r.def.rightClick.range = f;
                else if (key == "cc_seconds") r.def.rightClick.ccSeconds = f;
            } else if (isNum) {
                r.error = "line " + std::to_string(lineNo) +
                          ": bad number '" + val + "'";
                return r;
            }
        } else if (section == "passive") {
            if (key == "id") r.def.passiveId = val;
            else if (key == "desc") r.def.passiveDesc = val;
        } else if (section.empty()) {
            if (key == "id") r.def.id = val;
            else if (key == "display_name") r.def.displayName = val;
            else if (key == "flavor") r.def.flavor = val;
            else if (key == "melee_combo") r.def.meleeComboId = val;
            else if (key == "rmb_ability") r.def.rmbAbilityId = val;
            else if (isNum && parseFloat(val, f)) {
                if (key == "max_hp") r.def.maxHp = f;
                else if (key == "move_speed") r.def.moveSpeed = f;
                else if (key == "max_stamina") r.def.maxStamina = f;
            } else if (isNum) {
                r.error = "line " + std::to_string(lineNo) +
                          ": bad number '" + val + "'";
                return r;
            }
        }
        // Unknown sections/keys are ignored for forward compatibility.
    }

    if (r.def.id.empty()) {
        r.error = "missing required 'id'";
        return r;
    }
    r.ok = true;
    return r;
}

} // namespace cultulhu
