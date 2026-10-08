#include "world/ValeOfPnath.h"

#include <algorithm>

namespace cultulhu {

void ValeOfPnath::doGenerate() {
    // Bigger, deeper dungeon: up to VALE_MAX_ROOMS rooms, slightly larger
    // rooms than the base generator, chained mouth -> depths.
    rooms_.clear();
    for (int attempt = 0; attempt < 240 && rooms_.size() < VALE_MAX_ROOMS;
         ++attempt) {
        DungeonRoom r;
        r.w = rng_.intRange(5, 12);
        r.h = rng_.intRange(5, 12);
        r.x = rng_.intRange(1, width_ - r.w - 1);
        r.y = rng_.intRange(1, height_ - r.h - 1);
        bool ok = true;
        for (const auto& o : rooms_)
            if (r.overlaps(o)) { ok = false; break; }
        if (!ok) continue;
        rooms_.push_back(r);
        carveRoom(r);
    }
    for (size_t i = 1; i < rooms_.size(); ++i)
        carveCorridor(rooms_[i - 1].centerX(), rooms_[i - 1].centerY(),
                       rooms_[i].centerX(), rooms_[i].centerY());

    if (!rooms_.empty()) {
        int ex = rooms_[0].centerX(), ey = rooms_[0].centerY();
        tiles_[static_cast<size_t>(ey * width_ + ex)] = 'E';
    }
    relicSpots_.clear();
    for (size_t i = 1; i < rooms_.size(); ++i)
        relicSpots_.push_back(
            cellToWorld(rooms_[i].centerX(), rooms_[i].centerY()));

    assignDepths();
    placeValeHazards();
}

void ValeOfPnath::assignDepths() {
    // Depth grows along the corridor chain: each room is 1-2 deeper than
    // the previous, so the layout reads as a descent. Seeded RNG keeps it
    // deterministic.
    roomDepths_.assign(rooms_.size(), 0);
    maxDepth_ = 0;
    deepestRoom_ = 0;
    for (size_t i = 1; i < rooms_.size(); ++i) {
        roomDepths_[i] = roomDepths_[i - 1] + rng_.intRange(1, 2);
        if (roomDepths_[i] > maxDepth_) {
            maxDepth_ = roomDepths_[i];
            deepestRoom_ = static_cast<int>(i);
        }
    }
}

void ValeOfPnath::placeValeHazards() {
    // Seeded rolls per room (entrance room stays safe): 15% abyss pit,
    // 15% maddening whispers, 12% dhole tunnel. Deeper rooms are more
    // likely to be hazardous: the roll threshold scales with depth.
    hazards_.clear();
    for (size_t i = 1; i < rooms_.size(); ++i) {
        float depthBias = maxDepth_ > 0
                              ? 1.0f + 0.8f * (float)roomDepths_[i] /
                                             (float)maxDepth_
                              : 1.0f;
        float roll = rng_.uniform(0.0f, 1.0f);
        DungeonHazard h;
        h.roomIndex = static_cast<int>(i);
        if (roll < 0.15f * depthBias) {
            h.type = HazardType::AbyssPit;
        } else if (roll < 0.30f * depthBias) {
            h.type = HazardType::Whispers;
        } else if (roll < 0.42f * depthBias) {
            h.type = HazardType::DholeTunnel;
        } else {
            continue;
        }
        hazards_.push_back(std::move(h));
    }
}

float ValeOfPnath::dreadAt(int roomIndex) const {
    if (roomIndex < 0 || maxDepth_ <= 0) return 0.0f;
    size_t i = static_cast<size_t>(roomIndex);
    if (i >= roomDepths_.size()) return 0.0f;
    return (float)roomDepths_[i] / (float)maxDepth_;
}

Vec3 ValeOfPnath::valeRelicSpot() const {
    if (deepestRoom_ <= 0 || relicSpots_.empty()) return entrancePos_;
    // relicSpots_[i-1] corresponds to rooms_[i].
    size_t i = static_cast<size_t>(deepestRoom_) - 1;
    if (i >= relicSpots_.size()) return entrancePos_;
    return relicSpots_[i];
}

Vec3 ValeOfPnath::guardianSpawnPos() const {
    if (deepestRoom_ < 0 ||
        static_cast<size_t>(deepestRoom_) >= rooms_.size())
        return entrancePos_;
    const DungeonRoom& r = rooms_[static_cast<size_t>(deepestRoom_)];
    return cellToWorld(r.centerX(), r.centerY());
}

void ValeOfPnath::fireHazard(DungeonHazard& h, uint64_t entityId,
                            int roomIndex, Vec3 roomPos) {
    float dread = dreadAt(h.roomIndex);
    int depth = roomDepth(static_cast<size_t>(h.roomIndex));
    switch (h.type) {
        case HazardType::AbyssPit: {
            // Once per entity per room; damage scales with depth.
            if (std::find(h.victims.begin(), h.victims.end(), entityId) !=
                h.victims.end())
                break;
            h.victims.push_back(entityId);
            GameEvent e(EventType::AbyssPitFall);
            e.sourceId = entityId;
            e.targetId = id_;
            e.amount = ABYSS_PIT_BASE_DAMAGE + depth * ABYSS_PIT_PER_DEPTH;
            e.pos = roomPos;
            bus_.publish(e);
            break;
        }
        case HazardType::Whispers: {
            // Maddening whispers hit every traversal; fear scales w/ dread.
            GameEvent e(EventType::MaddeningWhispers);
            e.sourceId = id_;
            e.targetId = entityId;
            e.amount = WHISPER_BASE_FEAR + dread * WHISPER_DREAD_FEAR;
            e.pos = roomPos;
            bus_.publish(e);
            break;
        }
        case HazardType::DholeTunnel: {
            // Two-stage: first traversal = tremors (telegraph), the next
            // traversal of the same room = the ambush.
            if (!h.primed) {
                h.primed = true;
                GameEvent e(EventType::DholeTremors);
                e.sourceId = id_;
                e.targetId = static_cast<uint64_t>(h.roomIndex);
                e.pos = roomPos;
                bus_.publish(e);
                break;
            }
            h.primed = false; // resets: the tunnel can telegraph again
            GameEvent e(EventType::DholeAmbush);
            e.sourceId = dholeEntityId_ != 0 ? dholeEntityId_ : id_;
            e.targetId = entityId;
            e.amount = DHOLE_AMBUSH_BASE_DAMAGE + dread * DHOLE_AMBUSH_DREAD_DAMAGE;
            e.tag = "ambush";
            e.pos = roomPos;
            bus_.publish(e);
            break;
        }
        default:
            // Base types (spike pits etc.) and None/Count: delegate.
            DungeonInstance::fireHazard(h, entityId, roomIndex, roomPos);
            break;
    }
}

} // namespace cultulhu
