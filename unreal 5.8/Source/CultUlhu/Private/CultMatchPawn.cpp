#include "CultUlhuPlayerController.h"
#include "CultUlhuPlayerPawn.h"
#include "CultMatchDirector.h"
#include "CultKitRuntime.h"
#include "CultSkillAttack.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
void ACultUlhuPlayerPawn::SetMatchDead() {
 PreviousShift_=false; ShiftHeldSeconds_=.18f; ShiftDirection_=FVector::ZeroVector;
 bMatchDead_=true; MeleeBuffer_=SwingTime_=DashTime_=0; bRmbHeld_=false;
 GetCharacterMovement()->RemoveRootMotionSource(TEXT("CultDash")); GetCharacterMovement()->DisableMovement();
 SetActorHiddenInGame(true); SetActorEnableCollision(false);
}
void ACultUlhuPlayerPawn::RelocateMatch(FVector Feet) {
 const FVector Position=Feet+FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3);
 SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics); LastSafeLocation_=HomeLocation_=Position;
 GetCharacterMovement()->StopMovementImmediately(); GetCharacterMovement()->SetMovementMode(MOVE_Walking);
 if(auto* PC=Cast<APlayerController>(GetController())) {
  const FRotator Look(-12,GetFaction()?180:0,0); PC->SetControlRotation(Look);
  // Autonomous proxies do not consume ordinary replicated movement. Respawn
  // must also reach an idle owner, without waiting for their next move packet.
  if(HasAuthority()&&!PC->IsLocalController()) PC->ClientSetLocation(Position,Look);
 }
}
void ACultUlhuPlayerPawn::RespawnMatch(FVector Feet) {
 float CDs[3]={}; if(Kit_) for(int I=0;I<3;++I) CDs[I]=Kit_->Cooldown(I);
 delete Kit_; Kit_=new FCultKitRuntime(this); Kit_->Initialize(CharacterId_); for(int I=0;I<3;++I) Kit_->SetCooldown(I,CDs[I]); Health_=Kit_->Hp();
 bMatchDead_=false; Control_=FCultControlProjection(); MeleeBuffer_=SwingTime_=DashTime_=DashCooldown_=SmashCooldown_=0; EvadeTime_=0; bRmbHeld_=false;
 Tags.Remove(TEXT("CorpseRecorded")); SetActorHiddenInGame(false); SetActorEnableCollision(true);
 GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn,ECR_Overlap);
 GetCharacterMovement()->RemoveRootMotionSource(TEXT("CultDash")); RelocateMatch(Feet);
}
void ACultUlhuPlayerPawn::MoveMatchToward(FVector Goal,float Dt) {
 if(bMatchDead_||DashTime_>0||Control_.Stop||Control_.Stun) return;
 if(Control_.Fear) Goal=GetActorLocation()+(GetActorLocation()-Control_.Origin).GetSafeNormal2D()*6000;
 const FVector Direction=(Goal-FeetLocation()).GetSafeNormal2D(); if(Direction.IsNearlyZero()) return;
 const float Radius=GetSimpleCollisionRadius(); FCollisionQueryParams Q(SCENE_QUERY_STAT(MatchBotAvoid),false,this);
 for(float Turn:{0.f,40.f,-40.f,80.f,-80.f}) {
  FVector Heading=Direction.RotateAngleAxis(Turn,FVector::UpVector); FHitResult H;
  if(GetWorld()->SweepSingleByObjectType(H,GetActorLocation(),GetActorLocation()+Heading*(Radius+600),FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(Radius*.98f,GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-.2f),Q)&&!GetCharacterMovement()->IsWalkable(H)&&H.ImpactPoint.Z>FeetLocation().Z+GetCharacterMovement()->MaxStepHeight) continue;
  // Use the same floor finding, ramp stepping, gravity and swept capsule as the player.
  // Fixed Z teleports made elevated routes impossible and let bots hover off edges.
  GetCharacterMovement()->bRunPhysicsWithNoController=true;
  AddMovementInput(Heading,FMath::Min(1.f,float(FVector::Dist2D(Goal,FeetLocation()))/FMath::Max(1.f,WalkSpeed_*Dt)),true);
  SetActorRotation(Heading.Rotation()); break;
 }
}
void ACultUlhuPlayerPawn::HealMatch(float Amount) { if(HasAuthority()&&Kit_&&!bMatchDead_) { Kit_->Heal(Amount); Health_=Kit_->Hp(); } }
void ACultUlhuPlayerPawn::RecallMatch() { if(!HasAuthority()) { if(auto* PC=Cast<ACultUlhuPlayerController>(GetController())) PC->ServerMatchAction(0,FeetLocation()); return; } if(auto* M=ACultMatchDirector::Find(GetWorld())) M->StartRecall(this); }
void ACultUlhuPlayerPawn::RematchConfirm() { if(auto* M=ACultMatchDirector::Find(GetWorld())) M->Rematch(); }

void ACultUlhuPlayerPawn::MarkCombat() { LastCombatAt_=GetWorld()->GetTimeSeconds(); }
void ACultUlhuPlayerPawn::UpdateRecovery(float Dt) {
 if(bMatchDead_||GetHealth()<=0) return;
 if(auto* M=ACultMatchDirector::Find(GetWorld())) if(!M->CanAct(this)) return;
 const float RecoverSeconds=FMath::Clamp(GetWorld()->GetTimeSeconds()-LastCombatAt_-5.f,0.f,Dt);
 HealMatch(GetMaxHealth()*.005f*RecoverSeconds);
}
float ACultUlhuPlayerPawn::BotRangedReach() const {
 if(!Kit_) return 0;
 float Reach=0;
 for(int I=0;I<3;++I) {
  const auto& S=I==0?Kit_->Def.qAbility:I==1?Kit_->Def.fAbility:Kit_->Def.rAbility;
  const auto& K=S.effectKind;
  // Use the authored kit's delivery types, never invent a ranged attack.
  if(I<2&&(K=="projectile"||K=="damage"||K=="stun"||K=="debuff"||K=="aoe_damage")&&S.id!="crushing_maw"&&S.id!="crusher_claw"&&S.id!="claw_of_the_sphinx") Reach=FMath::Max(Reach,S.range*100.f);
 }
 if(Kit_->Def.rightClick.kind!=cultulhu::HeavyAttackKind::MeleeHeavy&&!Kit_->UsesMinionThrow()) Reach=FMath::Max(Reach,Kit_->Def.rightClick.range*100.f);
 return Reach;
}
int ACultUlhuPlayerPawn::HealingAbilitySlot() const {
 if(!Kit_) return -1;
 for(int I=0;I<3;++I) if((I==0?Kit_->Def.qAbility:I==1?Kit_->Def.fAbility:Kit_->Def.rAbility).effectKind=="heal") return I;
 return -1;
}
float ACultUlhuPlayerPawn::HealingRange() const {
 int I=HealingAbilitySlot(); return I<0?0:(I==0?Kit_->Def.qAbility:I==1?Kit_->Def.fAbility:Kit_->Def.rAbility).range*100.f+GetSimpleCollisionRadius();
}
bool ACultUlhuPlayerPawn::CanHealActor(AActor* Target) const { return Kit_&&Kit_->CanHeal(Target); }

void ACultUlhuPlayerPawn::SummonLandedDamage(float Actual) {
 if(Actual<=0||GetHealth()<=0||bMatchDead_) return;
 MarkCombat();
 if(CharacterId_==TEXT("shub_niggurath")) HealMatch(Actual*.1f);
}

void ACultUlhuPlayerPawn::OnRep_MatchDead() {
 if(bMatchDead_) SetMatchDead(); else { SetActorHiddenInGame(false); SetActorEnableCollision(true); GetCharacterMovement()->SetMovementMode(MOVE_Walking); GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn,ECR_Overlap); }
}
