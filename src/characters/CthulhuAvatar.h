#pragma once

// The first playable character of CULT-ULHU: the Cthulhu Avatar.
// Built purely from the CharacterDef data model — the worked example for
// the character-authoring guide (see the wave 7 README notes).

#include "characters/CharacterDef.h"

namespace cultulhu {

// Returns the full definition of "Cthulhu, the Dreaming God".
CharacterDef makeCthulhuAvatar();

} // namespace cultulhu
