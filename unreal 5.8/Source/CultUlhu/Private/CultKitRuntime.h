#pragma once
#include "CoreMinimal.h"
#include "CultCanonicalKit.h"
#include "CultPrimaryProfile.h"
#include "combat/Combos.h"
#include "combat/CrowdControl.h"
#include "input/InputManager.h"
#include "characters/abilities/RmbAbility.h"
#include "entities/Entity.h"
#include <memory>

class ACultUlhuPlayerPawn;
struct FCultCastArea;
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
 void SetHeavyCooldown(float Value) { HeavyCooldown=Value; }
 bool Cast(int Slot);
 FCultCastArea CastArea(int Slot) const;
 bool CanHeal(AActor* Target) const;
 bool CastHeal(const cultulhu::SpellDef& Spell,int Slot);
 void Heavy(bool bPress);
 FString HeavyHint() const;
 bool HasAimedSecondary() const;
 bool SecondaryReady() const { return HeavyCooldown<=0; }
 bool LeftClick(); // consumes LMB when the original RMB state machine owns it
 CultPrimary::FProfile Primary;
 bool LaunchPrimary();
 void ApplyCorrosion(AActor* Target);
 bool RockInput(bool Press);
 void ReleaseRock();
 void TickRock();
 float BeginMelee();
 int32 MeleeStage() const { return Combo.currentStage(); }
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
 float Ward() const { return Caster ? Caster->ward() : 0.f; }
 float MaxHp() const { return Def.maxHp; }
 bool Sprint(bool Wanted,float Dt) {
  const float RecoveryDt=FMath::Max(0.f,Dt-StaminaDelay);
  StaminaDelay=FMath::Max(0.f,StaminaDelay-Dt);
  return Stamina.update(Wanted,Wanted&&!Stamina.exhausted?Dt:RecoveryDt);
 }
 bool SpendFlight(float Rate,float Dt) {
  if(Stamina.exhausted||Stamina.value<=0) return false;
  Stamina.value=FMath::Max(0.f,Stamina.value-Rate*Dt);
  StaminaDelay=FMath::Max(StaminaDelay,.55f);
  Stamina.exhausted=Stamina.value<=0;
  return !Stamina.exhausted;
 }
 void DelayStamina(float Seconds) { StaminaDelay=FMath::Max(StaminaDelay,Seconds); }
 FString Status() const;
 cultulhu::CharacterDef Def;
 FVector LastEffectLocation = FVector::ZeroVector;
private:
 friend struct FCultSaveAccess;
 friend struct FCultCombatFeelTestAccess;
 friend struct FCultMobaMechanicsTestAccess;
 friend struct FCultPlayabilityAccess;
 struct Mirror { TWeakObjectPtr<AActor> Actor; std::unique_ptr<cultulhu::Entity> Core; float BeforeHp = 0; FVector BeforePosition; };
 ACultUlhuPlayerPawn* Owner;
 std::unique_ptr<cultulhu::Entity> Caster;
 std::unique_ptr<cultulhu::Entity> WorldImpact;
 std::unique_ptr<cultulhu::RmbAbility> Rmb;
 TWeakObjectPtr<class ACultChaosShield> ChaosShield;
 std::vector<Mirror> Mirrors;
 cultulhu::ActiveEffects Effects;
 cultulhu::StaminaGauge Stamina;
 float StaminaDelay=0;
 cultulhu::ComboTracker Combo;
 float Cooldowns[3] = {};
 float HeavyCooldown = 0;
 float BuffTime = 0;
 float BuffMult = 1;
 float WardTime = 0;
 TWeakObjectPtr<class ACultMatchUnit> HeldMinion;
 float HoldTime=0; bool ThrowOnReady=false;
 bool MinionThrowInput(bool Press);
 void TickMinionThrow(float Dt);
 void LaunchMinion();
 void CancelMinionThrow();
 cultulhu::AnimationState DominationPose=cultulhu::AnimationState::Idle;
 void UpdateDominationPose();
 struct FCorrosion { int Ticks=0; float UntilTick=.75f; };
 TMap<TWeakObjectPtr<AActor>,FCorrosion> Corrosion;
 void TickCorrosion(float Delta);
 bool MeleeRewarded=false;
 void Snapshot();
 void Apply();
 cultulhu::Entity* Target(float MaxRange, FVector& Aim);
 cultulhu::RmbContext Context();
};
