#include "combat/KitCaster.h"

#include "characters/CharacterDef.h"
#include "combat/Attacks.h"
#include "combat/CrowdControl.h"
#include "entities/Entity.h"

#include <cmath>
#include <sstream>

namespace cultulhu {

namespace {

// Summon species per kit spell id (lore-appropriate spawn names).
std::string summonSpecies(const std::string& spellId) {
    if (spellId == "brood_surge" || spellId == "woods_give_birth")
        return "dark young";
    if (spellId == "rise_deep_ones" || spellId == "matriarchs_call" ||
        spellId == "tide_of_the_brood")
        return "deep one";
    if (spellId == "cats_of_ulthar") return "ulthar cat";
    if (spellId == "call_of_the_deep") return "deep one";
    return "spawned horror";
}

std::string fmtDmg(float d) {
    std::ostringstream ss;
    ss << static_cast<int>(d + 0.5f);
    return ss.str();
}

} // namespace

KitCastResult castKitSpell(const SpellDef& spell, Entity& caster,
                           Entity* target,
                           const std::vector<Entity*>& foes,
                           BeliefSystem& beliefs, EventBus& bus,
                           ActiveEffects& fx, float powerMult) {
    KitCastResult r;
    const std::string& kind = spell.effectKind;
    const float power = spell.effectPower * powerMult;

    if (kind == "aoe_damage" || kind == "projectile") {
        if (target == nullptr || !target->alive()) {
            r.message = spell.name + " fizzles — no target in reach.";
            return r;
        }
        const float dmg = castSpell(caster.id(), caster.type(), *target, power,
                                    DamageType::Shadow, CCType::Count, 0.0f,
                                    beliefs, bus, fx,
                                    EventType::CivilianSlain, powerMult);
        r.damageDealt = dmg;
        r.ok = true;
        r.message = spell.name + " crashes down for " + fmtDmg(dmg) + "!";
        if (kind == "aoe_damage" && spell.range > 0.0f) {
            // Splash: half damage to other foes in the radius.
            int splashed = 0;
            for (Entity* f : foes) {
                if (f == target || !f->alive()) continue;
                if (f->position().distance(target->position()) >
                    spell.range)
                    continue;
                const float sd = castSpell(
                    caster.id(), caster.type(), *f, power * 0.5f,
                    DamageType::Shadow, CCType::Count, 0.0f, beliefs, bus, fx,
                    EventType::CivilianSlain, powerMult);
                r.damageDealt += sd;
                ++splashed;
            }
            if (splashed > 0)
                r.message += " (" + std::to_string(splashed) + " caught in the blast)";
        }
        return r;
    }

    if (kind == "fear_aura") {
        int caught = 0;
        for (Entity* f : foes) {
            if (!f->alive()) continue;
            if (f->position().distance(caster.position()) > spell.range)
                continue;
            fx.apply(f->id(), CCType::Fear, spell.effectPower, beliefs, bus);
            ++caught;
        }
        r.ok = true;
        r.message = spell.name + ": " + std::to_string(caught) +
                    " foes break and flee!";
        return r;
    }

    if (kind == "stun") {
        if (target == nullptr || !target->alive()) {
            r.message = spell.name + " fizzles — no target in reach.";
            return r;
        }
        castSpell(caster.id(), caster.type(), *target, 0.0f,
                  DamageType::Shadow, CCType::Stun, spell.effectPower, beliefs,
                  bus, fx, EventType::CivilianSlain, powerMult);
        r.ok = true;
        r.message = spell.name + " — the target is held fast (" +
                    fmtDmg(spell.effectPower) + "s)!";
        return r;
    }

    if (kind == "summon") {
        const int n = static_cast<int>(spell.effectPower);
        if (n <= 0) {
            r.message = spell.name + " fizzles — nothing answers.";
            return r;
        }
        r.summons.emplace_back(summonSpecies(spell.id), n);
        r.ok = true;
        r.message = spell.name + ": " + std::to_string(n) + " " +
                    summonSpecies(spell.id) +
                    (n > 1 ? "s rise to serve!" : " rises to serve!");
        return r;
    }

    if (kind == "buff") {
        r.ok = true;
        r.buffMult = 1.3f;
        r.buffSeconds = spell.effectPower;
        r.message = spell.name + " — power surges (+30% damage, " +
                    fmtDmg(spell.effectPower) + "s)!";
        return r;
    }

    if (kind == "debuff") {
        if (target == nullptr || !target->alive()) {
            r.message = spell.name + " fizzles — no target in reach.";
            return r;
        }
        fx.apply(target->id(), CCType::Slow, spell.effectPower, beliefs, bus);
        r.ok = true;
        r.message = spell.name + " — the target withers and slows (" +
                    fmtDmg(spell.effectPower) + "s)!";
        return r;
    }

    if (kind == "heal") {
        Entity& patient = (target && target->alive()) ? *target : caster;
        patient.heal(power);
        r.ok = true;
        r.message = spell.name + " knits flesh: +" + fmtDmg(power) + " HP.";
        return r;
    }

    if (kind == "shield") {
        caster.addWard(power);
        r.ok = true;
        r.message = spell.name + " — a ward settles (" + fmtDmg(power) +
                    " HP absorbed).";
        return r;
    }

    if (kind == "dash") {
        Vec3 dir{1, 0, 0};
        if (target && target->alive()) {
            dir = Vec3{target->position().x - caster.position().x, 0.0f,
                       target->position().z - caster.position().z};
            const float len = std::sqrt(dir.x * dir.x + dir.z * dir.z);
            if (len > 0.001f) {
                dir.x /= len;
                dir.z /= len;
            }
        }
        r.dash = Vec3{dir.x * spell.effectPower, 0.0f,
                      dir.z * spell.effectPower};
        r.ok = true;
        r.message = spell.name + " — the world blurs past!";
        return r;
    }

    if (kind == "pull") {
        int dragged = 0;
        for (Entity* f : foes) {
            if (!f->alive()) continue;
            Vec3 to = Vec3{caster.position().x - f->position().x, 0.0f,
                           caster.position().z - f->position().z};
            const float dist = std::sqrt(to.x * to.x + to.z * to.z);
            if (dist > spell.range || dist < 0.001f) continue;
            const float drag =
                spell.effectPower < dist ? spell.effectPower : dist;
            to.x /= dist;
            to.z /= dist;
            f->setPosition(Vec3{f->position().x + to.x * drag,
                               f->position().y, f->position().z + to.z * drag});
            ++dragged;
        }
        r.ok = true;
        r.message = spell.name + " — " + std::to_string(dragged) +
                    " foes dragged from the dark!";
        return r;
    }

    r.message = spell.name + " fizzles — unknown effect '" + kind + "'.";
    return r;
}

} // namespace cultulhu
