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

    void start() {
        active_ = true;
        interrupted_ = false;
        elapsed_ = 0.0;
        onStart(); // wave 18: rite-begin hook (e.g. sacrifice anim poses)
    }
    void interrupt();

    // Advance the ritual. Returns true if it completed this tick.
    bool update(double dt);

    bool active() const { return active_; }
    bool wasInterrupted() const { return interrupted_; }
    float progress() const {
        return duration_ > 0.0 ? static_cast<float>(elapsed_ / duration_) : 1.0f;
    }

protected:
    virtual void onStart() {} // wave 18: optional rite-begin hook
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

    // Wave 18: who performs the rite and who is sacrificed. The ids ride
    // on SacrificeStarted/Completed/Interrupted so the animation layer can
    // pose both participants (performer -> SacrificePerformer, victim ->
    // SacrificeVictim; both -> Idle on interrupt).
    void setPerformer(uint64_t entityId) { performerId_ = entityId; }
    void setVictim(uint64_t entityId) { victimId_ = entityId; }
    uint64_t performer() const { return performerId_; }
    uint64_t victim() const { return victimId_; }

protected:
    void onStart() override;
    void onComplete() override;
    void onInterrupted() override;

private:
    uint64_t performerId_ = 0;
    uint64_t victimId_ = 0;
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
