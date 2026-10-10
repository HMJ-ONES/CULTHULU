#pragma once

// Registry of playable character archetypes (wave 7).
//
// The driver registers characters at startup (built-ins like the Cthulhu
// Avatar) and the user adds more later one by one. Lookup is by string id.
// Duplicate or empty ids are rejected so two definitions can never fight
// over the same key.

#include "characters/CharacterDef.h"

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace cultulhu {

class CharacterRegistry {
public:
    // Registers a character. Returns false (and stores nothing) when the
    // id is empty or already taken.
    bool registerCharacter(CharacterDef def);

    // Lookup by id; nullptr when unknown.
    const CharacterDef* get(const std::string& id) const;

    // All registered ids, in registration (sorted-map) order.
    std::vector<std::string> list() const;

    size_t count() const { return chars_.size(); }

private:
    std::map<std::string, CharacterDef> chars_;
};

} // namespace cultulhu
