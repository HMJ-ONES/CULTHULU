// CULT-ULHU convenience-pass regression tests: the `status` kit line
// names the active kit.
//
// The driver's kit line used to read "kit: q READY f 3s r READY rmb READY"
// — cooldowns with no identity, so a new player (or anyone resuming a
// save) couldn't see WHO they walk as without typing `character select`.
// main.cpp isn't linkable from tests, so the formatting helper lives in
// src/driver/KitStatus.h (same pattern as SelectNudge.h). These pin the
// production formatting contract:
//
// 1. A real kit renders its displayName (e.g. Cthulhu's).
// 2. A null kit (no active kit) renders guidance, not a bare "none".
// 3. A kit with an empty displayName falls back to the raw kit id — the
//    name is missing, not the kit.
#include "driver/KitStatus.h"
#include "characters/CharacterDef.h"

#include <iostream>
#include <string>

using namespace cultulhu;

static int failures = 0;
#define CHECK(cond)                                        \
    do {                                                   \
        if (!(cond)) {                                     \
            std::cout << "FAIL line " << __LINE__ << ": "  \
                      << #cond << "\n";                     \
            ++failures;                                     \
        }                                                   \
    } while (0)

int main() {
    // 1. Real kit: display name wins.
    CharacterDef cthulhu;
    cthulhu.id = "cthulhu_avatar";
    cthulhu.displayName = "Cthulhu, the Dreaming God";
    CHECK(driver::kitStatusName(&cthulhu, cthulhu.id) ==
          "Cthulhu, the Dreaming God");

    // 2. No kit: guidance, not a bare "none".
    const std::string none = driver::kitStatusName(nullptr, "");
    CHECK(none.find("none") != std::string::npos);
    CHECK(none.find("character select") != std::string::npos);
    CHECK(none.find("chars") != std::string::npos);

    // 3. Empty displayName: raw kit id, never empty output.
    CharacterDef nameless;
    nameless.id = "yog_sothoth";
    nameless.displayName = "";
    CHECK(driver::kitStatusName(&nameless, nameless.id) == "yog_sothoth");

    if (failures == 0) std::cout << "kitstatus: all checks passed\n";
    return failures ? 1 : 0;
}
