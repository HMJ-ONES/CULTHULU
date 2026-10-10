#pragma once

#include "core/Events.h"

#include <functional>
#include <unordered_map>
#include <vector>

namespace cultulhu {

// Simple synchronous publish/subscribe bus. Systems subscribe to the event
// types they care about; publishers never need to know their listeners.
class EventBus {
public:
    using Handler = std::function<void(const GameEvent&)>;

    void subscribe(EventType type, Handler h);
    void publish(const GameEvent& e);

    // Number of handlers registered (useful for tests).
    size_t handlerCount(EventType type) const;

private:
    std::unordered_map<int, std::vector<Handler>> handlers_;
};

} // namespace cultulhu
