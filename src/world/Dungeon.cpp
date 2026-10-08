#include "world/Dungeon.h"

#include <algorithm>

namespace cultulhu {

const char* hazardTypeName(HazardType t) {
    switch (t) {
        case HazardType::None:    return "None";
        case HazardType::SpikePit: return "SpikePit";
        case HazardType::CaveIn:  return "CaveIn";
        case HazardType::Trapped: return "Trapped";
        case HazardType::Count:   return "Count";
    }
    return "Unknown";
}

DungeonInstance::DungeonInstance(EventBus& bus, uint64_t id, uint64_t seed,
                                 Vec3 entrancePos, int width, int height,
                                 float cellSize, DeferGenerate)
    : bus_(bus),
      rng_(seed),
      id_(id),
      seed_(seed),
      entrancePos_(entrancePos),
      width_(width),
      height_(height),
      cellSize_(cellSize),
      tiles_(static_cast<size_t>(width * height), '#') {}

void DungeonInstance::carve(int x, int y) {
    if (x < 1 || y < 1 || x >= width_ - 1 || y >= height_ - 1) return;
    tiles_[static_cast<size_t>(y * width_ + x)] = '.';
}

void DungeonInstance::carveRoom(const DungeonRoom& r) {
    for (int y = r.y; y < r.y + r.h; ++y)
        for (int x = r.x; x < r.x + r.w; ++x) carve(x, y);
}

void DungeonInstance::carveCorridor(int x0, int y0, int x1, int y1) {
    // L-shaped: random elbow order keeps layouts varied but deterministic.
    bool horizFirst = rng_.chance(0.5f);
    if (horizFirst) {
        int step = (x1 >= x0) ? 1 : -1;
        for (int x = x0; x != x1 + step; x += step) carve(x, y0);
        step = (y1 >= y0) ? 1 : -1;
        for (int y = y0; y != y1 + step; y += step) carve(x1, y);
    } else {
        int step = (y1 >= y0) ? 1 : -1;
        for (int y = y0; y != y1 + step; y += step) carve(x0, y);
        step = (x1 >= x0) ? 1 : -1;
        for (int x = x0; x != x1 + step; x += step) carve(x, y1);
    }
}

void DungeonInstance::doGenerate() {
    // Rejection-sample up to 12 non-overlapping rooms.
    rooms_.clear();
    for (int attempt = 0; attempt < 120 && rooms_.size() < 12; ++attempt) {
        DungeonRoom r;
        r.w = rng_.intRange(4, 10);
        r.h = rng_.intRange(4, 10);
        r.x = rng_.intRange(1, width_ - r.w - 1);
        r.y = rng_.intRange(1, height_ - r.h - 1);
        bool ok = true;
        for (const auto& o : rooms_)
            if (r.overlaps(o)) { ok = false; break; }
        if (!ok) continue;
        rooms_.push_back(r);
        carveRoom(r);
    }
    // Chain consecutive rooms with L-corridors.
    for (size_t i = 1; i < rooms_.size(); ++i)
        carveCorridor(rooms_[i - 1].centerX(), rooms_[i - 1].centerY(),
                       rooms_[i].centerX(), rooms_[i].centerY());

    if (!rooms_.empty()) {
        // Entrance room center is the dungeon mouth.
        int ex = rooms_[0].centerX(), ey = rooms_[0].centerY();
        tiles_[static_cast<size_t>(ey * width_ + ex)] = 'E';
    }
    // Relic spots: room centers past the entrance room.
    relicSpots_.clear();
    for (size_t i = 1; i < rooms_.size(); ++i)
        relicSpots_.push_back(
            cellToWorld(rooms_[i].centerX(), rooms_[i].centerY()));

    placeHazards();
}

void CaveInstance::doGenerate() {
    rooms_.clear();
    // Drunkard's walk from the center.
    int x = width_ / 2, y = height_ / 2;
    carve(x, y);
    const int steps = width_ * height_ / 3;
    static const int DX[4] = {1, -1, 0, 0};
    static const int DY[4] = {0, 0, 1, -1};
    for (int i = 0; i < steps; ++i) {
        int d = rng_.intRange(0, 3);
        x = std::max(1, std::min(width_ - 2, x + DX[d]));
        y = std::max(1, std::min(height_ - 2, y + DY[d]));
        carve(x, y);
    }
    // Stamp organic chambers; the first sits on the walk origin (center)
    // so the entrance is always on the tunnel network.
    for (int c = 0; c < 6; ++c) {
        int cx, cy;
        if (c == 0) {
            cx = width_ / 2;
            cy = height_ / 2;
        } else {
            cx = rng_.intRange(4, width_ - 5);
            cy = rng_.intRange(4, height_ - 5);
        }
        int rad = rng_.intRange(2, 4);
        for (int yy = cy - rad; yy <= cy + rad; ++yy)
            for (int xx = cx - rad; xx <= cx + rad; ++xx) {
                int dx = xx - cx, dy = yy - cy;
                if (dx * dx + dy * dy <= rad * rad) carve(xx, yy);
            }
        DungeonRoom r{cx - rad, cy - rad, rad * 2 + 1, rad * 2 + 1};
        rooms_.push_back(r);
    }
    if (!rooms_.empty()) {
        int ex = rooms_[0].centerX(), ey = rooms_[0].centerY();
        tiles_[static_cast<size_t>(ey * width_ + ex)] = 'E';
    }
    relicSpots_.clear();
    for (size_t i = 1; i < rooms_.size(); ++i)
        relicSpots_.push_back(
            cellToWorld(rooms_[i].centerX(), rooms_[i].centerY()));

    placeHazards();
}

void DungeonInstance::placeHazards() {
    // Seeded rolls: 18% spike pit, 12% hidden trap, 10% cave-in per room.
    // The entrance room (index 0) is always safe. Same seed -> same
    // hazards, because every roll comes from the seeded rng_.
    hazards_.clear();
    for (size_t i = 1; i < rooms_.size(); ++i) {
        float roll = rng_.uniform(0.0f, 1.0f);
        DungeonHazard h;
        h.roomIndex = static_cast<int>(i);
        if (roll < 0.18f) {
            h.type = HazardType::SpikePit;
        } else if (roll < 0.30f) {
            h.type = HazardType::Trapped;
        } else if (roll < 0.40f) {
            h.type = HazardType::CaveIn;
            h.triggerChance = CAVE_IN_TRIGGER_CHANCE;
        } else {
            continue;
        }
        hazards_.push_back(std::move(h));
    }
}

bool DungeonInstance::roomSealed(int roomIndex) const {
    for (const auto& h : hazards_)
        if (h.roomIndex == roomIndex && h.sealed) return true;
    return false;
}

int DungeonInstance::entityRoom(uint64_t entityId) const {
    auto it = entityRoom_.find(entityId);
    return it == entityRoom_.end() ? -1 : it->second;
}

bool DungeonInstance::traverseTo(uint64_t entityId, int roomIndex) {
    auto it = entityRoom_.find(entityId);
    if (it == entityRoom_.end()) return false; // not inside
    if (roomIndex < 0 || roomIndex >= static_cast<int>(rooms_.size()))
        return false;
    if (roomSealed(roomIndex)) return false; // passage blocked by a cave-in

    it->second = roomIndex;
    Vec3 roomPos =
        cellToWorld(rooms_[static_cast<size_t>(roomIndex)].centerX(),
                    rooms_[static_cast<size_t>(roomIndex)].centerY());

    for (auto& h : hazards_) {
        if (h.roomIndex != roomIndex) continue;
        switch (h.type) {
            case HazardType::SpikePit: {
                // Once per entity per room.
                if (std::find(h.victims.begin(), h.victims.end(), entityId) !=
                    h.victims.end())
                    break;
                h.victims.push_back(entityId);
                GameEvent e(EventType::SpikePitSprung);
                e.sourceId = entityId;
                e.targetId = id_;
                e.amount = SPIKE_PIT_DAMAGE;
                e.pos = roomPos;
                bus_.publish(e);
                break;
            }
            case HazardType::Trapped: {
                if (h.spent) break; // hidden trap fires once, ever
                h.spent = true;
                // Reuses TrapSprung, so the Trickery exertion feed applies.
                GameEvent e(EventType::TrapSprung);
                e.sourceId = entityId;
                e.targetId = id_;
                e.amount = DUNGEON_TRAP_DAMAGE;
                e.tag = "dungeon_trap";
                e.pos = roomPos;
                bus_.publish(e);
                break;
            }
            case HazardType::CaveIn: {
                if (h.sealed) break;
                if (!rng_.chance(h.triggerChance)) break;
                bool sealed = rng_.chance(CAVE_IN_SEAL_CHANCE);
                if (sealed) h.sealed = true;
                // Everyone currently in the room takes falling rock.
                for (const auto& kv : entityRoom_) {
                    if (kv.second != roomIndex) continue;
                    GameEvent e(EventType::CaveIn);
                    e.sourceId = id_;
                    e.targetId = kv.first;
                    e.amount = CAVE_IN_DAMAGE;
                    e.tag = sealed ? "sealed" : "rubble";
                    e.pos = roomPos;
                    bus_.publish(e);
                }
                break;
            }
            case HazardType::None:
            case HazardType::Count:
                break;
        }
    }
    return true;
}

void DungeonInstance::registerSurfaceEntity(uint64_t entityId) {
    if (entityId == 0 || isInside(entityId)) return;
    if (std::find(surfaceIds_.begin(), surfaceIds_.end(), entityId) ==
        surfaceIds_.end())
        surfaceIds_.push_back(entityId);
}

bool DungeonInstance::enter(uint64_t entityId) {
    auto it = std::find(surfaceIds_.begin(), surfaceIds_.end(), entityId);
    if (it == surfaceIds_.end()) return false;
    surfaceIds_.erase(it);
    insideIds_.push_back(entityId);
    entityRoom_[entityId] = 0; // entities arrive in the entrance room
    GameEvent e(EventType::DungeonEntered);
    e.sourceId = entityId;
    e.targetId = id_;
    e.pos = entrancePos_;
    bus_.publish(e);
    return true;
}

bool DungeonInstance::exit(uint64_t entityId) {
    auto it = std::find(insideIds_.begin(), insideIds_.end(), entityId);
    if (it == insideIds_.end()) return false;
    insideIds_.erase(it);
    entityRoom_.erase(entityId);
    surfaceIds_.push_back(entityId);
    GameEvent e(EventType::DungeonExited);
    e.sourceId = entityId;
    e.targetId = id_;
    e.pos = entrancePos_;
    bus_.publish(e);
    return true;
}

bool DungeonInstance::isInside(uint64_t entityId) const {
    return std::find(insideIds_.begin(), insideIds_.end(), entityId) !=
           insideIds_.end();
}

void DungeonInstance::onEntityDied(uint64_t entityId) {
    auto it = std::find(insideIds_.begin(), insideIds_.end(), entityId);
    if (it != insideIds_.end()) insideIds_.erase(it);
    entityRoom_.erase(entityId);
    if (completed_ || bossId_ == 0 || entityId != bossId_) return;
    completed_ = true;
    GameEvent e(EventType::DungeonCompleted);
    e.sourceId = bossId_;
    e.targetId = id_;
    e.pos = entrancePos_;
    bus_.publish(e);
}

} // namespace cultulhu
