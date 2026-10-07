#pragma once

#include "core/Vec3.h"
#include "entities/Entity.h"

#include <cstdint>
#include <vector>

namespace cultulhu {

class EventBus;

struct Trap {
    uint64_t id = 0;
    Vec3 pos;
    bool armed = true;
    int captured = 0;
};

// Trickery-belief trap system: traps capture civilians and adventurers.
// Sprung traps publish TrapSprung (power + fear via the belief system).
// Traps are single-use (creative liberty; see README).
class TrapSystem {
public:
    explicit TrapSystem(EventBus& bus);

    uint64_t placeTrap(Vec3 pos);
    const Trap* get(uint64_t id) const;
    size_t trapCount() const { return traps_.size(); }

    // Returns true if the victim was captured. Only civilians and
    // adventurers can be captured; the trap disarms after springing.
    bool springTrap(uint64_t trapId, Entity& victim);

private:
    EventBus& bus_;
    std::vector<Trap> traps_;
    uint64_t nextId_ = 1;
};

} // namespace cultulhu
