#include "CultUlhuPlayerPawn.h"
#include "CultCombatAudio.h"
#include "EngineUtils.h"
#include "CultSkillAttack.h"
#include "CultMeleeGeometry.h"
#include "CultKitRuntime.h"
#include "CultBlastFX.h"
#include "CultFeralBeast.h"
#include "CultPopulation.h"
#include "CultMatchDirector.h"
#include "CultMatchUnit.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"

namespace {
constexpr float StandardDashTime=.22f,StandardDashSpeed=3.5f;
void DriveDash(ACultUlhuPlayerPawn* Pawn,FVector Direction,float Speed,float Duration,float ExitSpeed) {
 auto* Movement=Pawn->GetCharacterMovement(); Movement->RemoveRootMotionSource(TEXT("CultDash"));
 auto Motion=MakeShared<FRootMotionSource_ConstantForce>(); Motion->InstanceName=TEXT("CultDash"); Motion->Priority=500; Motion->AccumulateMode=ERootMotionAccumulateMode::Override; Motion->Force=Direction*Speed; Motion->Duration=Duration; Motion->Settings.SetFlag(ERootMotionSourceSettingsFlags::IgnoreZAccumulate); Motion->FinishVelocityParams.Mode=ERootMotionFinishVelocityMode::ClampVelocity; Motion->FinishVelocityParams.ClampVelocity=ExitSpeed; Movement->ApplyRootMotionSource(Motion);
}
}
bool ACultUlhuPlayerPawn::CanDash() const {
 if(auto* M=ACultMatchDirector::Find(GetWorld())) if(!M->CanAct(this)) return false;
 return !Control_.Stop&&!Control_.Fear&&DashCooldown_<=0&&Kit_&&Kit_->StaminaValue()>=18;
}
float ACultUlhuPlayerPawn::DashTravelDistance() const { return WalkSpeed_*StandardDashSpeed*StandardDashTime; }
bool ACultUlhuPlayerPawn::TryDash(FVector Direction) {
 if(!HasAuthority()) ServerDash(Direction.GetSafeNormal2D());
 if(!CanDash()||Direction.IsNearlyZero()||!Kit_->SpendStamina(18)) return false;
 if(IsBot()) GetCharacterMovement()->bRunPhysicsWithNoController=true;
 if(auto* M=ACultMatchDirector::Find(GetWorld())) M->OnAction(this);
 DashDirection_=Direction.GetSafeNormal2D(); DashSpeed_=WalkSpeed_*StandardDashSpeed; DashTime_=StandardDashTime; EvadeTime_=.18f; DashCooldown_=1.15f;
 if(auto* Match=ACultMatchDirector::Find(GetWorld())) if(Match->Mode()==ECultPlayMode::Dotgoo) DashCooldown_=8.f;
 // Dashing cancels the arm instead of carrying an active swing across the whole dash.
 for(TActorIterator<ACultSkillAttack> It(GetWorld());It;++It) if(It->GetOwner()==this&&It->Shape==ACultSkillAttack::EShape::Sweep) It->Destroy();
 SwingTime_=0; MeleeBuffer_=0;
 if(!IsBot()) PlayCultCombatCue(GetWorld(),ECultCombatCue::Dash,.18f);
 DriveDash(this,DashDirection_,DashSpeed_,DashTime_,WalkSpeed_); return true;
}
FCultCombatHUDData ACultUlhuPlayerPawn::CombatHUD() const {
 FCultCombatHUDData H; H.Health=GetHealth(); H.MaxHealth=GetMaxHealth(); H.DashCooldown=DashCooldown_; H.Swinging=IsSwinging();
 if(Kit_) { H.RmbHint=Kit_->MinionThrowHint(); H.Name=UTF8_TO_TCHAR(Kit_->Def.displayName.c_str()); H.Stamina=Kit_->StaminaValue(); H.MaxStamina=Kit_->Def.maxStamina; for(int I=0;I<3;++I) { const auto& S=I==0?Kit_->Def.qAbility:I==1?Kit_->Def.fAbility:Kit_->Def.rAbility; H.AbilityNames[I]=UTF8_TO_TCHAR(S.name.c_str()); H.Cooldowns[I]=Kit_->Cooldown(I); } } return H;
}
void ACultUlhuPlayerPawn::StartMelee(float Damage,float Range,float Windup,float Duration) {
 const auto Profile=CultMeleeGeometry::Describe(this,Range);
 const FVector Origin=Profile.Origin;
 FVector Aim=CameraAim().ImpactPoint; FVector Direction=(Aim-Origin).GetSafeNormal();
 if(Direction.IsNearlyZero()) Direction=AimRotation().Vector();
 const FTransform T(Direction.Rotation(),Origin); auto* A=GetWorld()->SpawnActorDeferred<ACultSkillAttack>(ACultSkillAttack::StaticClass(),T,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 if(!A) return; A->Shape=ACultSkillAttack::EShape::Sweep; A->Direction=Direction; A->Reach=Profile.Reach; A->Radius=Profile.Radius; A->Windup=Windup; A->Duration=Duration; A->HalfAngle=bNextMeleeLeft_?-52.f:52.f; bNextMeleeLeft_=!bNextMeleeLeft_;
 TWeakObjectPtr<ACultUlhuPlayerPawn> Weak(this); A->Hit=[Weak,Damage](AActor* Target,FVector Point) { if(!Weak.IsValid()) return; if(auto* Pawn=Cast<ACultUlhuPlayerPawn>(Target)) if(Pawn->IsEvading()) { Pawn->ConfirmEvade(); return; } float D=Cast<ACultUlhuPlayerPawn>(Target)?Damage*(10.f/135.f):Damage;
 if(auto* M=ACultMatchDirector::Find(Weak->GetWorld())) { D=Damage*(Cast<ACultUlhuPlayerPawn>(Target)?15.f/135.f:Cast<ACultMatchUnit>(Target)&&!Cast<ACultMatchUnit>(Target)->IsStructure()?30.f/135.f:15.f/135.f)*M->DamageMultiplier(Weak.Get()); } if(auto* P=Cast<ACultPopulation>(Target)) if(!P->IsCultist()&&!P->IsSummoned()) D=FMath::Max(D,P->GetHealth()); ACultSkillAttack::Hurt(Target,D,Weak.Get(),Point); if(Weak->Kit_) Weak->Kit_->MeleeContact(Target); };
 SwingTime_=Windup+Duration; A->FinishSpawning(T);
}

void ACultUlhuPlayerPawn::ResolveSkillHit(const cultulhu::SpellDef& S,AActor* T,FVector Point,int Slot) { if(Kit_) Kit_->ResolveSpatial(S,T,Point,Slot); }

void ACultUlhuPlayerPawn::AbilityDash(FVector Direction,float Distance) {
 for(TActorIterator<ACultSkillAttack> It(GetWorld());It;++It) if(It->GetOwner()==this&&It->Shape==ACultSkillAttack::EShape::Sweep) It->Destroy();
 SwingTime_=0; MeleeBuffer_=0;
 if(!IsBot()) PlayCultCombatCue(GetWorld(),ECultCombatCue::Dash,.18f); if(IsBot()) GetCharacterMovement()->bRunPhysicsWithNoController=true; DashDirection_=Direction.GetSafeNormal2D(); DashSpeed_=Distance/.3f; DashTime_=.3f; EvadeTime_=.18f; DriveDash(this,DashDirection_,DashSpeed_,DashTime_,WalkSpeed_); }
