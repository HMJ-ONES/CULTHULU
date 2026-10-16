#include "characters/abilities/RmbAbility.h"
#include "characters/abilities/WaveOfDomination.h"

#include <memory>

namespace cultulhu {

// New characters register their RMB kits here; CharacterDef.rmbAbilityId
// names which one to build. Unknown ids return nullptr (the game falls
// back to the plain HeavyAttackDef numbers).
std::unique_ptr<RmbAbility> createRmbAbility(const std::string& id) {
    if (id == "wave_of_domination") {
        return std::make_unique<WaveOfDomination>();
    }
    return nullptr;
}

} // namespace cultulhu
