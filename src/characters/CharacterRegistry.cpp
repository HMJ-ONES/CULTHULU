// CharacterRegistry implementation.

#include "characters/CharacterRegistry.h"

namespace cultulhu {

bool CharacterRegistry::registerCharacter(CharacterDef def) {
    if (def.id.empty()) return false;
    if (chars_.count(def.id) != 0) return false;
    chars_.emplace(def.id, std::move(def));
    return true;
}

const CharacterDef* CharacterRegistry::get(const std::string& id) const {
    auto it = chars_.find(id);
    return it == chars_.end() ? nullptr : &it->second;
}

std::vector<std::string> CharacterRegistry::list() const {
    std::vector<std::string> out;
    out.reserve(chars_.size());
    for (const auto& [id, def] : chars_) out.push_back(id);
    return out;
}

} // namespace cultulhu
