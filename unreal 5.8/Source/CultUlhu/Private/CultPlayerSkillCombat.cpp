#include "CultUlhuPlayerPawn.h"
#include "CultCombatAudio.h"
#include "CultAbilityFX.h"
#include "EngineUtils.h"
#include "CultSkillAttack.h"
#include "CultMeleeGeometry.h"
#include "CultMovementTuning.h"
#include "CultKitRuntime.h"
#include "CultCharacterModel.h"
#include "CultBlastFX.h"
#include "CultFeralBeast.h"
#include "CultPopulation.h"
#include "CultMatchDirector.h"
#include "CultMatchUnit.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"

namespace {
constexpr float StandardDashTime=.22f,StandardDashSpeed=3.5f/CultMovement::WalkIncrease;
void DriveDash(ACultUlhuPlayerPawn* Pawn,FVector Direction,float Speed,float Duration,float ExitSpeed) {
 auto* Movement=Pawn->GetCharacterMovement(); Movement->RemoveRootMotionSource(TEXT("CultDash")); Movement->RemoveRootMotionSource(TEXT("CultMeleeStep"));
 auto Motion=MakeShared<FRootMotionSource_ConstantForce>(); Motion->InstanceName=TEXT("CultDash"); Motion->Priority=500; Motion->AccumulateMode=ERootMotionAccumulateMode::Override; Motion->Force=Direction*Speed; Motion->Duration=Duration; Motion->Settings.SetFlag(ERootMotionSourceSettingsFlags::IgnoreZAccumulate); Motion->FinishVelocityParams.Mode=ERootMotionFinishVelocityMode::ClampVelocity; Motion->FinishVelocityParams.ClampVelocity=ExitSpeed; Movement->ApplyRootMotionSource(Motion);
}
}
bool ACultUlhuPlayerPawn::CanDash() const {
 if(auto* M=ACultMatchDirector::Find(GetWorld())) if(!M->CanAct(this)) return false;
 return !Control_.Stop&&!IsIncapacitated()&&DashCooldown_<=0&&Kit_&&Kit_->StaminaValue()+.01f>=Kit_->Def.maxStamina/3.f;
}
float ACultUlhuPlayerPawn::DashTravelDistance() const { return WalkSpeed_*StandardDashSpeed*StandardDashTime; }
bool ACultUlhuPlayerPawn::TryDash(FVector Direction) {
 if(!HasAuthority()) ServerDash(Direction.GetSafeNormal2D());
 if(!CanDash()||Direction.IsNearlyZero()) return false;
 Kit_->SpendStamina(FMath::Min(Kit_->StaminaValue(),Kit_->Def.maxStamina/3.f));
 Kit_->DelayStamina(1.85f);
 if(IsBot()) GetCharacterMovement()->bRunPhysicsWithNoController=true;
 if(auto* M=ACultMatchDirector::Find(GetWorld())) M->OnAction(this);
 DashDirection_=Direction.GetSafeNormal2D(); DashSpeed_=WalkSpeed_*StandardDashSpeed; DashTime_=StandardDashTime; EvadeTime_=.18f; DashCooldown_=1.15f;
 if(auto* Match=ACultMatchDirector::Find(GetWorld())) if(Match->Mode()==ECultPlayMode::Dotgoo) DashCooldown_=CultMovement::DotgooDashCooldown;
 if(!IsBot()) PlayCultCombatCue(GetWorld(),ECultCombatCue::Dash,.18f);
 if(HasAuthority()&&!IsSwinging()) MulticastMotionCue(1,StandardDashTime);
 DriveDash(this,DashDirection_,DashSpeed_,DashTime_,WalkSpeed_); return true;
}
FCultCombatHUDData ACultUlhuPlayerPawn::CombatHUD() const {
 FCultCombatHUDData H; H.Health=GetHealth(); H.MaxHealth=GetMaxHealth(); H.DashCooldown=DashCooldown_; H.Swinging=IsSwinging();
 if(Kit_) { H.RmbHint=Kit_->HeavyHint(); H.Name=UTF8_TO_TCHAR(Kit_->Def.displayName.c_str()); H.Stamina=Kit_->StaminaValue(); H.MaxStamina=Kit_->Def.maxStamina; for(int I=0;I<3;++I) { const auto& S=I==0?Kit_->Def.qAbility:I==1?Kit_->Def.fAbility:Kit_->Def.rAbility; H.AbilityNames[I]=UTF8_TO_TCHAR(S.name.c_str()); H.Cooldowns[I]=Kit_->Cooldown(I); } } return H;
}
void ACultUlhuPlayerPawn::StartMelee(float Damage,float Range,float Windup,float Duration) {
 const auto Profile=CultMeleeGeometry::Describe(this,Range);
 const FVector Origin=Profile.Origin;
 FVector Aim=CameraAim().ImpactPoint; FVector Direction=(Aim-Origin).GetSafeNormal();
 if(Direction.IsNearlyZero()) Direction=AimRotation().Vector();
 const FTransform T(Direction.Rotation(),Origin); auto* A=GetWorld()->SpawnActorDeferred<ACultSkillAttack>(ACultSkillAttack::StaticClass(),T,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 if(!A) return; A->Shape=ACultSkillAttack::EShape::Sweep; A->Direction=Direction; A->Reach=Profile.Reach; A->Radius=Profile.Radius; A->Windup=Windup; A->Duration=Duration; A->HalfAngle=bNextMeleeLeft_?-52.f:52.f; bNextMeleeLeft_=!bNextMeleeLeft_;
 A->bLimbStrike=true;
 A->ImpactPower=Damage/9.f;
 A->AbilityLook=uint8(CultAbilityFX::ELook::Blade);
 TWeakObjectPtr<ACultUlhuPlayerPawn> Weak(this); A->Hit=[Weak,Damage](AActor* Target,FVector Point) { if(!Weak.IsValid()) return; auto* Pawn=Cast<ACultUlhuPlayerPawn>(Target); if(Pawn&&Pawn->IsEvading()) { Pawn->ConfirmEvade(); return; } float D=Pawn?Damage/9.f*Pawn->GetMaxHealth()/150.f:Damage;
 if(auto* M=ACultMatchDirector::Find(Weak->GetWorld())) { D=Damage*(Cast<ACultUlhuPlayerPawn>(Target)?15.f/135.f:Cast<ACultMatchUnit>(Target)&&!Cast<ACultMatchUnit>(Target)->IsStructure()?30.f/135.f:15.f/135.f)*M->DamageMultiplier(Weak.Get()); } if(auto* P=Cast<ACultPopulation>(Target)) if(!P->IsCultist()&&!P->IsSummoned()) D=FMath::Max(D,P->GetHealth()); ACultSkillAttack::Hurt(Target,D,Weak.Get(),Point); if(Weak->Kit_) Weak->Kit_->MeleeContact(Target); };
 SwingTime_=Windup+Duration;
 const FVector MovementDirection=GetVelocity().SizeSquared2D()>FMath::Square(75.f)
  ?GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D():FVector::ZeroVector;
 const bool Retreat=FVector::DotProduct(MovementDirection,AimRotation().Vector().GetSafeNormal2D())<-.25f;
 const uint8 Combo=uint8(Kit_?FMath::Clamp(Kit_->MeleeStage(),1,3):1);
 const uint8 Variant=Combo|(Retreat?4:0)|(A->HalfAngle<0?8:0);
 PlayAttackMotion(CultAttackMotion::EMove::Melee,Windup+Duration*.5f,SwingTime_+.09f,Variant);
 auto* Movement=GetCharacterMovement();
 if(!MovementDirection.IsNearlyZero()&&!IsDashing()&&Movement->IsMovingOnGround()) {
  Movement->RemoveRootMotionSource(TEXT("CultMeleeStep"));
  const float Distance=FMath::Min(DashTravelDistance()*.68f,1350.f)*(Retreat?.38f:Combo==3?1.f:.88f);
  auto Motion=MakeShared<FRootMotionSource_ConstantForce>(); Motion->InstanceName=TEXT("CultMeleeStep");
  Motion->Priority=400; Motion->AccumulateMode=ERootMotionAccumulateMode::Override;
  Motion->Duration=.19f;
  Motion->Force=MovementDirection*Distance/Motion->Duration;
  Motion->Settings.SetFlag(ERootMotionSourceSettingsFlags::IgnoreZAccumulate);
  Motion->FinishVelocityParams.Mode=ERootMotionFinishVelocityMode::ClampVelocity;
  Motion->FinishVelocityParams.ClampVelocity=WalkSpeed_*CultMovement::SprintRatio;
  Movement->ApplyRootMotionSource(Motion);
 }
 A->FinishSpawning(T);
}

void ACultUlhuPlayerPawn::ResolveSkillHit(const cultulhu::SpellDef& S,AActor* T,FVector Point,int Slot) { if(Kit_) Kit_->ResolveSpatial(S,T,Point,Slot); }

