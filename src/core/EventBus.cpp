#include "core/EventBus.h"

namespace cultulhu {

void EventBus::subscribe(EventType type, Handler h) {
    handlers_[static_cast<int>(type)].push_back(std::move(h));
}

void EventBus::publish(const GameEvent& e) {
    auto it = handlers_.find(static_cast<int>(e.type));
    if (it == handlers_.end()) return;
    // Copy the handler list so a handler may (un)subscribe safely.
    auto handlers = it->second;
    for (auto& h : handlers) h(e);
}

size_t EventBus::handlerCount(EventType type) const {
    auto it = handlers_.find(static_cast<int>(type));
    return (it == handlers_.end()) ? 0 : it->second.size();
}

} // namespace cultulhu
