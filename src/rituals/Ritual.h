#pragma once

#include "core/Events.h"

namespace cultulhu {

class EventBus;

// Interruptible ritual with a progress bar. Subclasses define what happens
// on completion vs interruption (e.g. Sacrifice: completed => power up,
// interrupted => power down).
class Ritual {
public:
    Ritual(double durationSeconds, EventBus& bus)
        : duration_(durationSeconds), bus_(bus) {}
    virtual ~Ritual() = default;

    void start() { active_ = true; interrupted_ = false; elapsed_ = 0.0; }
    void interrupt();

    // Advance the ritual. Returns true if it completed this tick.
    bool update(double dt);

    bool active() const { return active_; }
    bool wasInterrupted() const { return interrupted_; }
    float progress() const {
        return duration_ > 0.0 ? static_cast<float>(elapsed_ / duration_) : 1.0f;
    }

protected:
    virtual void onComplete() = 0;
    virtual void onInterrupted() = 0;

    EventBus& bus_;

private:
    double duration_;
    double elapsed_ = 0.0;
    bool active_ = false;
    bool interrupted_ = false;
};

// Sacrifice: a cultist dedicates their death to Cthulhu (30s ritual).
class SacrificeRitual : public Ritual {
public:
    explicit SacrificeRitual(EventBus& bus) : Ritual(30.0, bus) {}
protected:
    void onComplete() override;
    void onInterrupted() override;
};

// Necromancy: dark ritual empowering sorcerers (45s ritual).
class NecromancyRitual : public Ritual {
public:
    explicit NecromancyRitual(EventBus& bus) : Ritual(45.0, bus) {}
protected:
    void onComplete() override;
    void onInterrupted() override;
};

} // namespace cultulhu
