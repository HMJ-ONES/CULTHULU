#pragma once

#include <string>
#include <unordered_map>

namespace cultulhu {

// Named model slots: the engine-agnostic code never touches raw art files; it
// refers to slots. The engine binding (Unreal/Unity) resolves each bound slot
// to the actual skeletal mesh. Everything runs fine with slots unbound.
enum class ModelSlot {
    PlayerAvatar,
    Cultist,
    Monstrosity,
    Civilian,
    Adventurer,
    Creature,
    Sorcerer,
    Mimic,
    GreatOldOne,
    Count
};

inline const char* modelSlotName(ModelSlot s) {
    switch (s) {
        case ModelSlot::PlayerAvatar: return "PlayerAvatar";
        case ModelSlot::Cultist:      return "Cultist";
        case ModelSlot::Monstrosity:  return "Monstrosity";
        case ModelSlot::Civilian:     return "Civilian";
        case ModelSlot::Adventurer:   return "Adventurer";
        case ModelSlot::Creature:     return "Creature";
        case ModelSlot::Sorcerer:     return "Sorcerer";
        case ModelSlot::Mimic:        return "Mimic";
        case ModelSlot::GreatOldOne:  return "GreatOldOne";
        case ModelSlot::Count:        return "Count";
    }
    return "Unknown";
}

// Skeleton description for a slot: which skeleton the model was rigged with
// and which retarget profile the engine should use (e.g. a Mixamo profile).
struct RigDefinition {
    std::string skeletonName;     // e.g. "mixamo_x_bot"
    int boneCount = 0;
    std::string retargetProfile;  // e.g. "MixamoToUnreal"
};

// Binds model file paths (FBX) and rig definitions to slots. Unbound slots
// are normal: procedural/animation fallbacks keep the game running.
class AssetManager {
public:
    // Convention: rigged FBX files live under this directory.
    static const char* modelsDir() { return "assets/models"; }

    void bindModel(ModelSlot slot, std::string path) {
        models_[slot] = std::move(path);
    }
    void bindRig(ModelSlot slot, RigDefinition rig) {
        rigs_[slot] = std::move(rig);
    }

    bool isBound(ModelSlot slot) const {
        auto it = models_.find(slot);
        return it != models_.end() && !it->second.empty();
    }
    // Empty string when unbound.
    std::string modelPath(ModelSlot slot) const {
        auto it = models_.find(slot);
        return it != models_.end() ? it->second : std::string();
    }
    bool hasRig(ModelSlot slot) const { return rigs_.count(slot) > 0; }
    const RigDefinition* rig(ModelSlot slot) const {
        auto it = rigs_.find(slot);
        return it != rigs_.end() ? &it->second : nullptr;
    }

    size_t boundCount() const { return models_.size(); }

private:
    struct SlotHash {
        size_t operator()(ModelSlot s) const noexcept {
            return static_cast<size_t>(s);
        }
    };
    std::unordered_map<ModelSlot, std::string, SlotHash> models_;
    std::unordered_map<ModelSlot, RigDefinition, SlotHash> rigs_;
};

} // namespace cultulhu
