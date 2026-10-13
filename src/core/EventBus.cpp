#include "core/EventBus.h"

namespace cultulhu {

EventBus::Subscription EventBus::subscribe(EventType type, Handler h) {
    const Subscription id = nextId_++;
    handlers_[static_cast<int>(type)].push_back({id, std::move(h)});
    return id;
}

void EventBus::unsubscribe(Subscription sub) {
    if (sub == 0) return;
    for (auto& kv : handlers_) {
        auto& v = kv.second;
        v.erase(std::remove_if(v.begin(), v.end(),
                               [sub](const Entry& e) { return e.id == sub; }),
                v.end());
    }
}

void EventBus::publish(const GameEvent& e) {
    auto it = handlers_.find(static_cast<int>(e.type));
    if (it == handlers_.end()) return;
    // Copy the handler list so a handler may (un)subscribe safely.
    auto handlers = it->second;
    for (auto& h : handlers) h.h(e);
}

size_t EventBus::handlerCount(EventType type) const {
    auto it = handlers_.find(static_cast<int>(type));
    return (it == handlers_.end()) ? 0 : it->second.size();
}

} // namespace cultulhu
