#include "breeding/BreedingSystem.h"
#include "core/EventBus.h"
#include "core/RNG.h"

#include <string>

namespace cultulhu {

BreedingSystem::BreedingSystem(EventBus& bus, RNG& rng) : bus_(bus), rng_(rng) {}

bool BreedingSystem::compatible(Species a, Species b) {
    if (a == b) return true;
    // Compatibility matrix (creative liberty; see README).
    // Humans mix with Deep Ones and Beasts (classic hybrids); Ghouls mix with
    // everything except Humans; Horrors mix with everything except Humans.
    auto key = [](Species x, Species y) {
        int i = static_cast<int>(x), j = static_cast<int>(y);
        return i < j ? i * 16 + j : j * 16 + i;
    };
    switch (key(a, b)) {
        case 0 * 16 + 1: // Human x DeepOne
        case 0 * 16 + 3: // Human x Beast
        case 1 * 16 + 2: // DeepOne x Ghoul
        case 1 * 16 + 3: // DeepOne x Beast
        case 1 * 16 + 4: // DeepOne x Horror
        case 2 * 16 + 3: // Ghoul x Beast
        case 2 * 16 + 4: // Ghoul x Horror
        case 3 * 16 + 4: // Beast x Horror
            return true;
        default:
            return false;
    }
}

std::unique_ptr<Monstrosity> BreedingSystem::breed(Species a, Species b,
                                                  FactionId faction, Vec3 pos,
                                                  const std::string& name) {
    if (!active_ || !compatible(a, b)) return nullptr;

    // Wave 29: skill mitigates the feral chance; every birth can be named.
    bool feral = rng_.chance(effectiveFeralChance());
    std::string finalName =
        name.empty()
            ? std::string(speciesName(a)) + "-" + speciesName(b) + " hybrid"
            : name;
    auto m = std::make_unique<Monstrosity>(faction, pos, finalName, feral);

    GameEvent bred(EventType::MonstrosityBred);
    bred.sourceId = m->id();
    bred.faction = faction;
    bred.tag = finalName;
    bus_.publish(bred);

    if (feral) {
        GameEvent rampage(EventType::FeralRampage);
        rampage.sourceId = m->id();
        rampage.faction = faction;
        rampage.pos = pos;
        bus_.publish(rampage);
    }
    return m;
}

} // namespace cultulhu
