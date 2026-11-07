#pragma once

#include "CoreMinimal.h"

namespace CultPrimary {
enum class EDelivery : uint8 { Melee, Bolt, CorrosiveArc, Cannon };
enum class EAccent : uint8 { Maul, Lunge, Rake, Coil, Recoil, Pulse, Chant, Hover };
enum class ESignal : uint8 { Default, Brine, Acid, Gold, Violet, Ember, Stone };

struct FProfile {
 const TCHAR* Name = TEXT("Melee combo");
 EDelivery Delivery = EDelivery::Melee;
 EAccent Accent = EAccent::Maul;
 ESignal Signal = ESignal::Default;
 float Recovery = .68f;
 float Windup = .12f;
 float Damage = 9.f;
 float Reach = 0.f;
 float Speed = 0.f;
 float Radius = 0.f;
 bool Ranged() const { return Delivery != EDelivery::Melee; }
};

inline FProfile Find(const FString& Id) {
 using D = EDelivery;
 using A = EAccent;
 using S = ESignal;
 if(Id == TEXT("cthulhu_avatar")) return {TEXT("Dreamer's Claws"),D::Melee,A::Maul,S::Default,.64f,.12f,9.f};
 if(Id == TEXT("dagon")) return {TEXT("Undertow Rake"),D::Melee,A::Lunge,S::Brine,.68f,.12f,9.5f};
 if(Id == TEXT("nyarlathotep")) return {TEXT("Maskbreak"),D::Melee,A::Lunge,S::Gold,.58f,.12f,8.f};
 if(Id == TEXT("rhan_tegoth")) return {TEXT("Fossil Pincer"),D::Melee,A::Maul,S::Stone,.86f,.12f,11.f};
 if(Id == TEXT("shoggoths")) return {TEXT("Protoplasm Maul"),D::Melee,A::Rake,S::Violet,.76f,.12f,10.5f};
 if(Id == TEXT("yig")) return {TEXT("Coiled Blades"),D::Melee,A::Coil,S::Brine,.60f,.12f,8.5f};
 if(Id == TEXT("bokrug")) return {TEXT("Sarnath Spit"),D::CorrosiveArc,A::Lunge,S::Brine,1.04f,.21f,6.5f,4850,5100,74};
 if(Id == TEXT("mother_hydra")) return {TEXT("Caustic Brine"),D::CorrosiveArc,A::Chant,S::Brine,1.20f,.25f,6.5f,5000,4800,82};
 if(Id == TEXT("shub_niggurath")) return {TEXT("Rot Spore"),D::CorrosiveArc,A::Rake,S::Acid,1.13f,.24f,6.f,4900,4600,95};
 if(Id == TEXT("mi_go")) return {TEXT("Yuggoth Dart"),D::Bolt,A::Recoil,S::Acid,.76f,.17f,6.5f,5350,7100,43};
 if(Id == TEXT("nodens")) return {TEXT("Abyssal Harpoon"),D::Bolt,A::Lunge,S::Brine,.98f,.23f,8.f,5650,6800,54};
 if(Id == TEXT("hastur")) return {TEXT("Yellow Sign"),D::Bolt,A::Chant,S::Gold,1.02f,.27f,8.5f,5700,4600,82};
 if(Id == TEXT("sghllor")) return {TEXT("Prismatic Pulse"),D::Bolt,A::Hover,S::Violet,.70f,.14f,6.f,5200,6300,49};
 if(Id == TEXT("ghatanothoa")) return {TEXT("Obsidian Cannon"),D::Cannon,A::Maul,S::Ember,1.95f,.47f,14.f,6600,5600,130};
 if(Id == TEXT("nug_and_yeb")) return {TEXT("Twin Embers"),D::Cannon,A::Coil,S::Ember,1.76f,.40f,13.f,6150,5400,98};
 if(Id == TEXT("yog_sothoth")) return {TEXT("Gate Collapse"),D::Cannon,A::Hover,S::Violet,2.08f,.52f,15.f,6900,5200,150};
 return {};
}
}
