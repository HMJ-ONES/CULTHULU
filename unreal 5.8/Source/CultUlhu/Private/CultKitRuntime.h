#pragma once
#include "CoreMinimal.h"
#include "CultCanonicalKit.h"
#include "combat/Combos.h"
#include "combat/CrowdControl.h"
#include "input/InputManager.h"
#include "characters/abilities/RmbAbility.h"
#include "entities/Entity.h"
#include <memory>

class ACultUlhuPlayerPawn;
class FCultKitRuntime
{
public:
 explicit FCultKitRuntime(ACultUlhuPlayerPawn* InOwner);
 ~FCultKitRuntime();
 bool UsesMinionThrow() const;
 FString MinionThrowHint() const;
 bool Initialize(const FString& Id);
 void Tick(float Delta);
 void AdvanceCooldowns(float Dt) { for(float& C:Cooldowns) C=FMath::Max(0.f,C-Dt); HeavyCooldown=FMath::Max(0.f,HeavyCooldown-Dt); }
 void SetCooldown(int Slot,float Value) { if(Slot>=0&&Slot<3) Cooldowns[Slot]=Value; }
 bool Cast(int Slot);
 bool CanHeal(AActor* Target) const;
 bool CastHeal(const cultulhu::SpellDef& Spell,int Slot);
 void Heavy(bool bPress);
 bool LeftClick(); // consumes LMB when the original RMB state machine owns it
 float BeginMelee();
 void MeleeContact(AActor* Actor);
 bool SpendStamina(float Amount) { if(Stamina.value<Amount) return false; Stamina.value-=Amount; return true; }
 float StaminaValue() const { return Stamina.value; }
 float Cooldown(int Slot) const { return Cooldowns[Slot]; }
 void ResolveSpatial(cultulhu::SpellDef Spell,AActor* Target,FVector Point,int Slot);
 void EmitCast(const cultulhu::SpellDef& Spell,int Slot);
 bool LaunchSpatial(const cultulhu::SpellDef& Spell,int Slot,FVector Aim);
 void Mouse(float X, float Y);
 float Damage(float Amount);
 void Heal(float Amount);
 float Hp() const { return Caster ? Caster->hp() : 0.f; }
 float MaxHp() const { return Def.maxHp; }
 bool Sprint(bool Wanted, float Dt) { return Stamina.update(Wanted, Dt); }
 FString Status() const;
 cultulhu::CharacterDef Def;
 FVector LastEffectLocation = FVector::ZeroVector;
private:
 friend struct FCultCombatFeelTestAccess;
 friend struct FCultMobaMechanicsTestAccess;
 struct Mirror { TWeakObjectPtr<AActor> Actor; std::unique_ptr<cultulhu::Entity> Core; float BeforeHp = 0; FVector BeforePosition; };
 ACultUlhuPlayerPawn* Owner;
 std::unique_ptr<cultulhu::Entity> Caster;
 std::unique_ptr<cultulhu::Entity> WorldImpact;
 std::unique_ptr<cultulhu::RmbAbility> Rmb;
 std::vector<Mirror> Mirrors;
 cultulhu::ActiveEffects Effects;
 cultulhu::StaminaGauge Stamina;
 cultulhu::ComboTracker Combo;
 float Cooldowns[3] = {};
 float HeavyCooldown = 0;
 float BuffTime = 0;
 float BuffMult = 1;
 TWeakObjectPtr<class ACultMatchUnit> HeldMinion;
 float HoldTime=0; bool ThrowOnReady=false;
 void MinionThrowInput(bool Press);
 void TickMinionThrow(float Dt);
 void LaunchMinion();
 void CancelMinionThrow();
 bool MeleeRewarded=false;
 void Snapshot();
 void Apply();
 cultulhu::Entity* Target(float MaxRange, FVector& Aim);
 cultulhu::RmbContext Context();
};
