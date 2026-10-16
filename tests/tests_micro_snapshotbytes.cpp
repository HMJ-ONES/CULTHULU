// CULT-ULHU lightweight regression tests: encodeSnapshotBytes is the
// 20 Hz host-snapshot fast path — direct-to-wire framing that must stay
// byte-identical to encode(encodeSnapshot(tick, ents)) while skipping the
// intermediate Message/Fields. Pins:
//
//   1. Byte identity across entity counts (0, 1, 20, 100), tick edges
//      (0, 77, UINT32_MAX), negative/large coords, state byte edges.
//   2. Wire framing intact: decodeFrame + decodeSnapshot round-trips
//      through the fast-path bytes.

#include "net/Netcode.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using namespace cultulhu;
using namespace cultulhu::net;

static int failures = 0;
#define CHECK(cond)                                        \
    do {                                                   \
        if (!(cond)) {                                     \
            ++failures;                                    \
            std::cout << "FAIL line " << __LINE__ << ": "  \
                      << #cond << "\n";                     \
        }                                                  \
    } while (0)

static std::vector<SnapshotEntity> makeEnts(size_t n) {
    std::vector<SnapshotEntity> ents;
    for (size_t i = 0; i < n; ++i) {
        SnapshotEntity e;
        e.id = static_cast<uint32_t>(1000 + i * 7);
        e.x = -1234.56f + static_cast<float>(i) * 10.5f;
        e.y = 2.0f;
        e.z = 98765.43f - static_cast<float>(i);
        e.hp = 100.0f - static_cast<float>(i % 11) * 9.1f;
        e.state = static_cast<uint8_t>(i % 256);
        ents.push_back(e);
    }
    return ents;
}

int main() {
    // ---------- 1. byte identity vs the Message path -----------------
    for (size_t n : {0u, 1u, 2u, 20u, 100u}) {
        for (uint32_t tick : {0u, 77u, 4294967295u}) {
            auto ents = makeEnts(n);
            auto slow = encode(encodeSnapshot(tick, ents));
            auto fast = encodeSnapshotBytes(tick, ents);
            CHECK(slow == fast);
        }
    }
    // state byte edges + extreme coords
    {
        std::vector<SnapshotEntity> ents = {
            {1, 0.0f, 0.0f, 0.0f, 0.0f, 0},
            {2, -0.005f, 1e10f, -1e10f, 999.9f, 255},
            {4294967295u, 3.402823e38f, -3.4e38f, 0.001f, 55.5f, 128},
        };
        CHECK(encode(encodeSnapshot(5, ents)) == encodeSnapshotBytes(5, ents));
    }

    // ---------- 2. fast-path bytes decode like the slow path --------
    {
        auto ents = makeEnts(20);
        auto fast = encodeSnapshotBytes(77, ents);
        auto dm = decodeFrame(fast.data(), fast.size());
        CHECK(dm.has_value());
        CHECK(dm->type == MsgType::HostSnapshot);
        uint32_t tick = 0;
        std::vector<SnapshotEntity> back;
        CHECK(decodeSnapshot(*dm, tick, back));
        CHECK(tick == 77);
        CHECK(back.size() == 20);
        CHECK(back[0].id == 1000 && back[19].id == 1000 + 19 * 7);
        CHECK(back[7].state == 7);
        // field order on the wire matches the documented layout
        std::string payload(reinterpret_cast<const char*>(fast.data() + 5),
                            fast.size() - 5);
        CHECK(payload.rfind("tick=", 0) == 0);
        CHECK(payload.find(";n=20;") != std::string::npos);
    }

    if (failures == 0) std::cout << "ALL OK\n";
    return failures == 0 ? 0 : 1;
}
