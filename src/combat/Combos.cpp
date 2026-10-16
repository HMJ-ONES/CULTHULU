#include "combat/Combos.h"

namespace cultulhu {

ComboTracker::ComboTracker(ComboDef def) : def_(std::move(def)) {}

void ComboTracker::onLmbClick(double timestamp) {
    if (!active_ || (timestamp - lastClickAt_) > def_.chainWindowSec) {
        // Fresh chain.
        active_ = true;
        stage_ = 0;
    } else if (stage_ + 1 < static_cast<int>(def_.stages.size())) {
        ++stage_;
    } else {
        // Combo end: the chain completed, start a fresh one.
        stage_ = 0;
    }
    lastClickAt_ = timestamp;
}

void ComboTracker::update(double timestamp) {
    if (active_ && (timestamp - lastClickAt_) > def_.chainWindowSec)
        reset();
}

const ComboStage* ComboTracker::stageDef() const {
    if (!active_ || stage_ < 0 ||
        stage_ >= static_cast<int>(def_.stages.size()))
        return nullptr;
    return &def_.stages[static_cast<size_t>(stage_)];
}

void ComboTracker::reset() {
    active_ = false;
    stage_ = 0;
}

} // namespace cultulhu
