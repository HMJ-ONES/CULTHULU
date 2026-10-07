#pragma once

#include "core/EventBus.h"
#include "core/GameClock.h"

namespace cultulhu {

// Base class for game modes (5v5 capture-the-point, 5v5 MOBA defense, ...).
class GameMode {
public:
    GameMode(EventBus& bus, GameClock& clock) : bus_(bus), clock_(clock) {}
    virtual ~GameMode() = default;

    virtual void update(double dt) = 0;
    virtual bool isOver() const = 0;
    // Winning team index (0/1), or -1 if no winner yet.
    virtual int winner() const { return -1; }

protected:
    EventBus& bus_;
    GameClock& clock_;
};

} // namespace cultulhu
