// CULT-ULHU lightweight regression tests: Message::fields is now an
// insertion-ordered vector (wave 36), not std::map<string,string> —
// ~half the per-snapshot heap churn on the host. This pins the
// behavior-preserving contract:
//
//   1. encodeFields walks insertion order: a HostSnapshot encodes as
//      "tick=…;n=…;e0=…;e1=…;" (the documented {tick,n,e0,…} layout).
//   2. Map replace-if-present semantics survive: assigning an existing
//      key overwrites, and a hand-built payload with duplicate keys
//      decodes to the LAST value.
//   3. Missing keys behave as before: fieldStr/fieldInt fall back to
//      their defaults.
//   4. Full snapshot path round-trips: encodeSnapshot -> wire encode ->
//      decodeFrame -> decodeSnapshot yields the same entities.

#include "net/Netcode.h"

#include <iostream>
#include <string>

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

int main() {
    // ---------- 1. insertion order -----------------------------------
    {
        Message m{MsgType::HostSnapshot, {}};
        m.fields["tick"] = "7";
        m.fields["n"] = "2";
        m.fields["e0"] = "10,1.0,2.0,3.0,100.0,5";
        m.fields["e1"] = "11,4.0,5.0,6.0,80.0,0";
        std::string payload = encodeFields(m.fields);
        CHECK(payload ==
              "tick=7;n=2;e0=10,1.0,2.0,3.0,100.0,5;e1=11,4.0,5.0,6.0,80.0,0;");
    }

    // ---------- 2. replace-if-present / duplicate keys ----------------
    {
        Message m{MsgType::ChatMsg, {}};
        m.fields["text"] = "first";
        m.fields["text"] = "second";
        CHECK(m.fields.size() == 1);
        CHECK(fieldStr(m, "text") == "second");

        Fields d = decodeFields("k=a;k=b;k=c;");
        CHECK(d.size() == 1);
        const std::string* v = d.find("k");
        CHECK(v && *v == "c");
    }

    // ---------- 3. missing keys --------------------------------------
    {
        Message m{MsgType::HostSnapshot, {}};
        m.fields["tick"] = "3";
        CHECK(fieldStr(m, "nope") == "");
        CHECK(fieldStr(m, "nope", "dflt") == "dflt");
        CHECK(fieldInt(m, "nope", 42) == 42);
        CHECK(m.fields.find("nope") == nullptr);
    }

    // ---------- 4. snapshot round-trip --------------------------------
    {
        std::vector<SnapshotEntity> ents = {
            {1, 10.0f, 0.0f, -5.0f, 100.0f, 3},
            {2, 0.0f, 1.0f, 2.0f, 55.5f, 0},
        };
        auto bytes = encode(encodeSnapshot(77, ents));
        auto dm = decodeFrame(bytes.data(), bytes.size());
        CHECK(dm.has_value());
        CHECK(dm->type == MsgType::HostSnapshot);
        uint32_t tick = 0;
        std::vector<SnapshotEntity> back;
        CHECK(decodeSnapshot(*dm, tick, back));
        CHECK(tick == 77);
        CHECK(back.size() == 2);
        CHECK(back[0].id == 1 && back[1].id == 2);
        CHECK(back[0].state == 3);
        // field order on the wire matches the documented layout
        std::string payload(reinterpret_cast<const char*>(bytes.data() + 5),
                            bytes.size() - 5);
        CHECK(payload.rfind("tick=", 0) == 0);
    }

    if (failures == 0) std::cout << "ALL OK\n";
    return failures == 0 ? 0 : 1;
}
