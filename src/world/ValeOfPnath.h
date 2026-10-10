#pragma once

// Wave 13: the Vale of Pnath — a signature deep-dungeon zone.
//
// Concept (Lovecraft's Dreamlands, public domain; see
// assets/creatures/LEGAL_NAMES.md — only Lovecraft-original terms are
// used): a vast lightless abyss in the underworld. Dholes (on the legal
// allowlist) tunnel through it; even ghouls fear what nests in its
// depths. Utter darkness, strange echoes, wrongness. This file adds no
// story retelling — only the game beats: descent, dread, dholes.
//
// Design:
//   * Extends DungeonInstance: bigger grid (72x72), up to 20 rooms, and a
//     per-room DEPTH value (0 at the mouth, growing along the corridor
//     chain) that reads as vertical descent.
//   * Vale hazards (HazardType::AbyssPit / Whispers / DholeTunnel), fired
//     through the virtual fireHazard() hook:
//       - AbyssPit: bottomless pit; fall damage scales with depth.
//       - Whispers: maddening whispers; fear scales with dread.
//       - DholeTunnel: telegraphed burrower ambush — first traversal
//         publishes DholeTremors (the warning), the next traversal of
//         the same room triggers DholeAmbush.
//   * Escalating dread: dreadAt(room) = depth/maxDepth in [0,1]; deeper
//     rooms hit harder, spawn stronger, and feed more Fear exertion.
//   * The deepest room is the relic vault: a cursed artifact waits there
//     (valeRelicSpot()); callers spawn the guardian (usually a Dhole)
//     at guardianSpawnPos() and register it via setBossId().
//   * Entrance: entered through a surface fissure or from the deepest
//     cave tier (entranceKind(); default "fissure").
//   * Dhole model slot: no CC0 dhole model exists anywhere surveyed, so
//     ModelCatalog maps "dhole" to "" (procedural serpent/worm-like
//     fallback). See the README bestiary note.

#include "world/Dungeon.h"

#include <string>
#include <vector>

namespace cultulhu {

class ValeOfPnath : public DungeonInstance {
public:
    static constexpr int VALE_WIDTH = 72;
    static constexpr int VALE_HEIGHT = 72;
    static constexpr int VALE_MAX_ROOMS = 20;

    // Wave-13 tuning (creative-liberty numbers, documented in README).
    static constexpr float ABYSS_PIT_BASE_DAMAGE = 20.0f;
    static constexpr float ABYSS_PIT_PER_DEPTH = 12.0f;
    static constexpr float WHISPER_BASE_FEAR = 4.0f;
    static constexpr float WHISPER_DREAD_FEAR = 16.0f;
    static constexpr float DHOLE_AMBUSH_BASE_DAMAGE = 55.0f;
    static constexpr float DHOLE_AMBUSH_DREAD_DAMAGE = 45.0f;

    ValeOfPnath(EventBus& bus, uint64_t id, uint64_t seed, Vec3 entrancePos,
                const std::string& entranceKind = "fissure")
        : DungeonInstance(bus, id, seed, entrancePos, VALE_WIDTH,
                          VALE_HEIGHT, DEFAULT_CELL, DeferGenerate{}),
          entranceKind_(entranceKind) {
        generate();
    }

    const std::string& entranceKind() const { return entranceKind_; }

    // Depth of a room: 0 at the mouth, growing along the corridor chain.
    int roomDepth(size_t roomIndex) const {
        return roomIndex < roomDepths_.size()
                   ? roomDepths_[roomIndex]
                   : 0;
    }
    int maxDepth() const { return maxDepth_; }
    int deepestRoomIndex() const { return deepestRoom_; }

    // Dread in [0,1]: depth/maxDepth. Drives whisper fear, ambush damage,
    // and spawn strength for callers.
    float dreadAt(int roomIndex) const;

    // The relic vault: deepest room's relic spot and guardian spawn.
    Vec3 valeRelicSpot() const;
    Vec3 guardianSpawnPos() const;

    // Wave 16: seize the vault's cursed relic. Publishes ValeRelicClaimed
    // once (the "What Lies Beneath" achievement hook); returns false if
    // the vault was already emptied. Callers grant the relic's power via
    // RelicSystem::seizeRelic (which publishes RelicClaimed).
    bool claimRelic(uint64_t entityId);
    bool relicClaimed() const { return relicClaimed_; }

    // Optional: link a Dhole entity id so ambushes are attributed to it
    // (sourceId of DholeAmbush). 0 = the tunnel itself strikes.
    void setDholeEntityId(uint64_t id) { dholeEntityId_ = id; }
    uint64_t dholeEntityId() const { return dholeEntityId_; }

protected:
    void doGenerate() override;
    void fireHazard(DungeonHazard& h, uint64_t entityId, int roomIndex,
                    Vec3 roomPos) override;

private:
    void placeValeHazards();
    void assignDepths();

    std::string entranceKind_;
    std::vector<int> roomDepths_;
    int maxDepth_ = 0;
    int deepestRoom_ = 0;
    uint64_t dholeEntityId_ = 0;
    bool relicClaimed_ = false;
};

} // namespace cultulhu
