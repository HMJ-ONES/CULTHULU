#pragma once

#include "core/Events.h"

#include <algorithm>
#include <functional>
#include <unordered_map>
#include <vector>

namespace cultulhu {

// Simple synchronous publish/subscribe bus. Systems subscribe to the event
// types they care about; publishers never need to know their listeners.
class EventBus {
public:
    using Handler = std::function<void(const GameEvent&)>;
    // 8 PM bug-fix (2026-10-12): subscription handle. subscribe() returns
    // one; unsubscribe() removes that handler. 0 is never a valid handle.
    // Existing callers may ignore the return value — nothing else changes.
    using Subscription = uint64_t;

    Subscription subscribe(EventType type, Handler h);
    void unsubscribe(Subscription sub);
    void publish(const GameEvent& e);

    // Number of handlers registered (useful for tests).
    size_t handlerCount(EventType type) const;

private:
    struct Entry {
        Subscription id;
        Handler h;
    };
    std::unordered_map<int, std::vector<Entry>> handlers_;
    Subscription nextId_ = 1;
};

} // namespace cultulhu
