#pragma once

// CULT-ULHU character.def parser (wave 7).
//
// character.def is a small human-editable text format:
//
//   # comment lines start with '#'
//   id = cthulhu_avatar
//   display_name = Cthulhu, the Dreaming God
//   flavor = The dreaming god walks.
//   max_hp = 500
//   move_speed = 6.0
//   max_stamina = 100
//   melee_combo = eldritch_flurry
//   rmb_ability = wave_of_domination   # optional state-machine kit id
//
//   [q]            # ability kit sections: [q], [f], [r]
//   id = tentacle_slam
//   name = Tentacle Slam
//   flavor = ...
//   cooldown = 8
//   stamina_cost = 25
//   mana_cost = 0
//   effect = aoe_damage
//   power = 120
//   range = 8
//
//   [rightclick]
//   kind = MindControl      # MeleeHeavy|MindControl|AcidSpit|EldritchGrasp
//   name = Wave of Domination
//   damage_mult = 0.4
//   range = 25
//   cc = Stun               # optional CC type name
//   cc_seconds = 2.5
//
//   [passive]
//   id = dreamers_presence
//   desc = ...

#include "characters/CharacterDef.h"

#include <string>
#include <vector>

namespace cultulhu {

struct ParseResult {
    CharacterDef def;
    bool ok = false;
    std::string error; // set when !ok
};

// Parse character.def text. Never throws; reports the first problem in
// error. Missing optional fields keep CharacterDef defaults.
ParseResult parseCharacterDef(const std::string& text);

} // namespace cultulhu
