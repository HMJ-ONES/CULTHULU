#pragma once

#include <string_view>

namespace CultSecondary {
enum class EDelivery { Bolt, Arc, Pair, Fan, Beam, Disk };
struct FProfile {
 std::string_view Id;
 const char* Name;
 EDelivery Delivery;
 float Reach, Radius, Speed, Windup, Damage, Cooldown;
 bool LongFlight=false;
 bool Corrosive=false;
};

inline constexpr FProfile Profiles[] = {
 {"bokrug", "Sarnath Stone", EDelivery::Arc, 3800, 115, 5100, .27f, 112, 7},
 {"dagon", "Riptide Fan", EDelivery::Fan, 2700, 65, 4400, .21f, 138, 6},
 {"ghatanothoa", "Obsidian Eruption", EDelivery::Disk, 1050, 180, 0, .68f, 111, 9},
 {"hastur", "Yellow Curtain", EDelivery::Beam, 5500, 110, 0, .48f, 96, 8},
 {"mi_go", "Yuggoth Culture", EDelivery::Arc, 5100, 48, 6800, .19f, 72, 7, false, true},
 {"mother_hydra", "Brood Volley", EDelivery::Fan, 4500, 48, 5800, .34f, 144, 8},
 {"nodens", "Abyssal Spear", EDelivery::Bolt, 6600, 28, 8300, .28f, 115, 7, true},
 {"nug_and_yeb", "Crossing Fangs", EDelivery::Pair, 3600, 53, 5700, .16f, 116, 6},
 {"nyarlathotep", "Black Pharaoh's Step", EDelivery::Disk, 790, 120, 0, .43f, 100, 7},
 {"rhan_tegoth", "Fossil Cannon", EDelivery::Bolt, 5400, 150, 4700, .43f, 141, 9, true},
 {"sghllor", "Prism Fracture", EDelivery::Beam, 8000, 30, 0, .36f, 122, 8},
 {"shoggoths", "Protoplasm Glob", EDelivery::Arc, 3200, 190, 4000, .32f, 132, 8},
 {"shub_niggurath", "Briar Burst", EDelivery::Disk, 1320, 115, 0, .82f, 96, 8},
 {"yig", "Viper Venom", EDelivery::Pair, 4550, 31, 7100, .14f, 83, 7, false, true},
 {"yog_sothoth", "Gate Collapse", EDelivery::Disk, 960, 250, 0, .92f, 145, 10},
};

inline const FProfile* Find(std::string_view Id) {
 for(const auto& Profile:Profiles) if(Profile.Id==Id) return &Profile;
 return nullptr;
}
}
