#pragma once

// Wave 7: dungeons & caves. A DungeonInstance is a seeded procedural
// underground layout linked to the surface by an entrance entity: entities
// move between the surface roster and the dungeon's inside roster with
// enter()/exit(). Relic/artifact spawn points sit inside the layout, and a
// designated boss id completes the dungeon when it dies (DungeonCompleted).
//
// Layout algorithms (both deterministic from the instance seed):
//   DungeonInstance: "rooms + L-corridors" — up to 12 non-overlapping
//     rectangular rooms are placed by rejection sampling, then consecutive
//     rooms are joined by L-shaped corridors (random elbow order).
//   CaveInstance: "drunkard's walk" — a random walk carves tunnels from the
//     center, then several circular chambers are stamped at random points,
//     giving an organic cave feel.

#include "core/EventBus.h"
#include "core/Events.h"
#include "core/RNG.h"
#include "core/Vec3.h"

#include <cstdint>
#include <vector>

namespace cultulhu {

struct DungeonRoom {
    int x = 0, y = 0; // top-left cell
    int w = 0, h = 0; // size in cells

    int centerX() const { return x + w / 2; }
    int centerY() const { return y + h / 2; }
    bool overlaps(const DungeonRoom& o, int pad = 1) const {
        return x - pad < o.x + o.w && x + w + pad > o.x &&
               y - pad < o.y + o.h && y + h + pad > o.y;
    }
};

class DungeonInstance {
public:
    static constexpr int DEFAULT_WIDTH = 48;
    static constexpr int DEFAULT_HEIGHT = 48;
    static constexpr float DEFAULT_CELL = 4.0f; // world units per cell

    DungeonInstance(EventBus& bus, uint64_t id, uint64_t seed,
                    Vec3 entrancePos,
                    int width = DEFAULT_WIDTH, int height = DEFAULT_HEIGHT,
                    float cellSize = DEFAULT_CELL)
        : DungeonInstance(bus, id, seed, entrancePos, width, height, cellSize,
                          DeferGenerate{}) {
        generate();
    }

    virtual ~DungeonInstance() = default;

    uint64_t id() const { return id_; }
    uint64_t seed() const { return seed_; }
    Vec3 entrancePos() const { return entrancePos_; }

    // The surface entity (usually an EntityType::Building flagged as an
    // entrance, or a plain Entity) whose position links surface<->dungeon.
    // The caller creates/owns the entity; the dungeon just records the link.
    void setEntranceEntity(uint64_t entityId) { entranceEntity_ = entityId; }
    uint64_t entranceEntity() const { return entranceEntity_; }

    int width() const { return width_; }
    int height() const { return height_; }
    const std::vector<char>& tiles() const { return tiles_; } // '#'/'E'/'.'
    char tile(int x, int y) const { return tiles_[y * width_ + x]; }
    bool isFloor(int x, int y) const {
        if (x < 0 || y < 0 || x >= width_ || y >= height_) return false;
        char c = tile(x, y);
        return c == '.' || c == 'E';
    }
    const std::vector<DungeonRoom>& rooms() const { return rooms_; }
    const std::vector<Vec3>& relicSpots() const { return relicSpots_; }

    // World-space position of a cell (dungeon is centered on entrancePos).
    Vec3 cellToWorld(int x, int y) const {
        return Vec3(entrancePos_.x + (x - width_ / 2) * cellSize_,
                    entrancePos_.y,
                    entrancePos_.z + (y - height_ / 2) * cellSize_);
    }

    // Occupancy: entities at the entrance on the surface are registered,
    // then moved inside/outside. Emits DungeonEntered/DungeonExited.
    void registerSurfaceEntity(uint64_t entityId);
    bool enter(uint64_t entityId); // surface -> inside; false if not waiting
    bool exit(uint64_t entityId);  // inside -> surface; false if not inside
    bool isInside(uint64_t entityId) const;
    size_t insideCount() const { return insideIds_.size(); }

    // Boss: designate the guardian entity id; when onEntityDied sees it,
    // the dungeon completes and DungeonCompleted is published.
    void setBossId(uint64_t bossId) { bossId_ = bossId; }
    uint64_t bossId() const { return bossId_; }
    void onEntityDied(uint64_t entityId);
    bool completed() const { return completed_; }

    // (Re)runs the procedural generator. Called automatically by the ctor.
    void generate() { doGenerate(); }

protected:
    struct DeferGenerate {};
    DungeonInstance(EventBus& bus, uint64_t id, uint64_t seed,
                    Vec3 entrancePos, int width, int height, float cellSize,
                    DeferGenerate);

    virtual void doGenerate(); // rooms + L-corridors

    void carve(int x, int y);
    void carveRoom(const DungeonRoom& r);
    void carveCorridor(int x0, int y0, int x1, int y1);

    EventBus& bus_;
    RNG rng_;

    uint64_t id_;
    uint64_t seed_;
    Vec3 entrancePos_;
    int width_, height_;
    float cellSize_;

    uint64_t entranceEntity_ = 0;
    std::vector<char> tiles_;
    std::vector<DungeonRoom> rooms_;
    std::vector<Vec3> relicSpots_;

    std::vector<uint64_t> surfaceIds_;
    std::vector<uint64_t> insideIds_;

    uint64_t bossId_ = 0;
    bool completed_ = false;
};

// Organic variant: drunkard's-walk tunnels + stamped circular chambers.
class CaveInstance : public DungeonInstance {
public:
    CaveInstance(EventBus& bus, uint64_t id, uint64_t seed, Vec3 entrancePos,
                 int width = DEFAULT_WIDTH, int height = DEFAULT_HEIGHT,
                 float cellSize = DEFAULT_CELL)
        : DungeonInstance(bus, id, seed, entrancePos, width, height, cellSize,
                          DeferGenerate{}) {
        generate();
    }

protected:
    void doGenerate() override;
};

} // namespace cultulhu
