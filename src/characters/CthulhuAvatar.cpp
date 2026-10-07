// Cthulhu Avatar: "Cthulhu, the Dreaming God".
//
// Tuning rationale (baseline: a cultist has 100 HP, a civilian 50 HP, and
// a stock melee hit deals 10 — see combat/Attacks.h tuning notes):
//  - 500 HP makes the avatar a raid-boss in free-roam: a coordinated 5v5
//    team can still burn it down, but nothing kills it by accident.
//  - 6.0 m/s is fast for a giant — it should feel inevitable, not clumsy.
//  - Q "Tentacle Slam" (120 power, 8 m) clears civilian crowds in one
//    slam and takes ~4 hits to drop an equal avatar; 8 s cooldown keeps
//    it from being spammable, 25 stamina is a quarter of the bar.
//  - F "Nightmare Veil" is pure control: no damage, 4 s of Fear in a
//    12 m aura — the setup tool for slams and escapes. 15 s cooldown.
//  - R "Call of the Deep" is the ultimate: summons 4 Deep Ones and
//    buffs nearby cultists. 45 s cooldown and half the stamina bar mean
//    it is a once-per-fight decision.
//  - Right-click "Wave of Domination" is a full state-machine kit
//    (see characters/abilities/WaveOfDomination.h): a 25 m mind-control
//    wave catches up to 5 victims, levitates them while RMB is held,
//    slams them into buildings/entities for 150 damage, launches them
//    with LMB for 300 damage, or drops them gently on RMB release
//    (lethal above 8 m). Targeted RMB instead stuns for 2.5 s.
//    7 s cooldown from cast.
//  - Passive "Dreamer's Presence" is a hook only: nearby foes accumulate
//    dread while the avatar looms; the engine binding converts full
//    dread into fear/rout effects. Behavior binding arrives with the
//    engine, so only id+desc live here.

#include "characters/CthulhuAvatar.h"

namespace cultulhu {

CharacterDef makeCthulhuAvatar() {
    CharacterDef d;
    d.id = "cthulhu_avatar";
    d.displayName = "Cthulhu, the Dreaming God";
    d.flavor = "The dreaming god walks. Cities drown in his shadow.";

    d.maxHp = 500.0f;
    d.moveSpeed = 6.0f;
    d.maxStamina = 100.0f;

    d.qAbility.id = "tentacle_slam";
    d.qAbility.name = "Tentacle Slam";
    d.qAbility.flavor = "A mountain of tentacles crashes down.";
    d.qAbility.cooldownSec = 8.0f;
    d.qAbility.staminaCost = 25.0f;
    d.qAbility.manaCost = 0.0f;
    d.qAbility.effectKind = "aoe_damage";
    d.qAbility.effectPower = 120.0f;  // flat AoE damage
    d.qAbility.range = 8.0f;

    d.fAbility.id = "nightmare_veil";
    d.fAbility.name = "Nightmare Veil";
    d.fAbility.flavor = "Reality thins; all who look upon him flee.";
    d.fAbility.cooldownSec = 15.0f;
    d.fAbility.staminaCost = 35.0f;
    d.fAbility.manaCost = 0.0f;
    d.fAbility.effectKind = "fear_aura";
    d.fAbility.effectPower = 4.0f;  // fear duration in seconds
    d.fAbility.range = 12.0f;

    d.rAbility.id = "call_of_the_deep";
    d.rAbility.name = "Call of the Deep";
    d.rAbility.flavor = "The ocean answers: Deep Ones rise to serve.";
    d.rAbility.cooldownSec = 45.0f;
    d.rAbility.staminaCost = 50.0f;
    d.rAbility.manaCost = 0.0f;
    d.rAbility.effectKind = "summon";
    d.rAbility.effectPower = 4.0f;  // Deep Ones summoned (+ cultist buff)
    d.rAbility.range = 20.0f;

    d.meleeComboId = "eldritch_flurry";

    d.rightClick.kind = HeavyAttackKind::MindControl;
    d.rightClick.name = "Wave of Domination";
    d.rightClick.damageMult = 0.4f;
    d.rightClick.range = 25.0f;   // wave range; see WaveOfDomination tuning
    d.rightClick.ccType = "Stun";  // direct-target alternate cast
    d.rightClick.ccSeconds = 2.5f;
    // Full state-machine kit (wave -> levitate -> slam/launch/drop).
    d.rmbAbilityId = "wave_of_domination";

    d.passiveId = "dreamers_presence";
    d.passiveDesc =
        "Dreamer's Presence: foes near the avatar accumulate dread; at "
        "full dread they break and flee. (Behavior hook; engine binds it.)";

    return d;
}

} // namespace cultulhu
