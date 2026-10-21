#include "CultMatchUnit.h"
#include "CultMatchDirector.h"
#include "CultMobaRoute.h"
#include "CultMobaJungle.h"
#include "CultUlhuPlayerPawn.h"
#include "CultSkillAttack.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

bool ACultMatchUnit::IsHeldBy(const ACultUlhuPlayerPawn* Actor) const { return Transit==ETransit::Held&&Holder.Get()==Actor; }
bool ACultMatchUnit::TryLift(ACultUlhuPlayerPawn* Caster) {
 auto* M=ACultMatchDirector::Find(GetWorld());
 if(!M||M->Mode()!=ECultPlayMode::Dotgoo||!M->CanAct(Caster)||IsStructure()||Health<=0||IsInTransit()||Team==Caster->GetFaction()) return false;
 Pickup=GetActorLocation(); Holder=Caster; Transit=ETransit::Held; TransitTime=0; RejoiningLane=false;
 Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision); AttackTarget.Reset(); return true;
}
void ACultMatchUnit::StartFlight(ACultSkillAttack* Projectile) {
 if(Transit!=ETransit::Held||!Projectile) return;
 Transit=ETransit::Thrown; TransitTime=0; Flight=Projectile; AddTickPrerequisiteActor(Projectile);
}
bool ACultMatchUnit::UpdateTransit(float Dt) {
 if(Transit==ETransit::None) return false;
 TransitTime+=Dt;
 if(Transit==ETransit::Held) {
  auto* P=Holder.Get(); auto* M=ACultMatchDirector::Find(GetWorld());
  if(!P||P->IsIncapacitated()||P->GetHealth()<=0||!M||!M->CanAct(P)) { LandMiss(GetActorLocation()); return true; }
  const auto* F=M->Fighter(P); if(F&&(F->Recall>0||F->Invoke>0)) { LandMiss(GetActorLocation()); return true; }
  FVector Direction=P->AimRotation().Vector().GetSafeNormal2D(); if(Direction.IsNearlyZero()) Direction=P->GetActorForwardVector();
  FVector Anchor=P->FeetLocation()+Direction*(P->GetSimpleCollisionRadius()+600);
  Anchor.Z+=P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()*1.3f+FMath::Sin(TransitTime*4)*45;
  SetActorLocation(Anchor,false); SetActorRotation(FRotator(0,TransitTime*55,12));
 } else if(Flight.IsValid()) SetActorLocation(Flight->ProjectilePosition(),false);
 else { LandMiss(GetActorLocation()); return true; }
 Control.Tick(Dt); AnimateArtBody(Dt,false); return true;
}
void ACultMatchUnit::LandMiss(FVector Point) {
 if(Transit==ETransit::None) return;
 if(Flight.IsValid()) RemoveTickPrerequisiteActor(Flight.Get());
 const FVector Delta=(Point-Pickup).GetClampedToMaxSize2D(4500);
 FVector Desired=Pickup+FVector(Delta.X,Delta.Y,0);
 Desired.X=FMath::Clamp(Desired.X,-35000.,35000.); Desired.Y=FMath::Clamp(Desired.Y,-19000.,19000.);
 FVector Landing=Pickup; bool Found=false;
 for(int Ring=0;Ring<5&&!Found;++Ring) for(int I=0;I<(Ring?8:1)&&!Found;++I) {
  float A=I*PI/4; FVector Trial=Desired+FVector(FMath::Cos(A),FMath::Sin(A),0)*Ring*800;
  if(FVector::Dist2D(Trial,Pickup)>4500) continue;
  FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(SlingLanding),false,this);
  if(GetWorld()->LineTraceSingleByObjectType(H,Trial+FVector(0,0,10000),Trial-FVector(0,0,20000),FCollisionObjectQueryParams(ECC_WorldStatic),Q)&&H.GetActor()&&(H.GetActor()->ActorHasTag(TEXT("CultMatchFloor"))||H.GetActor()->ActorHasTag(TEXT("CultMatchWalkSurface")))) {
   Trial=H.ImpactPoint+FVector(0,0,Capsule->GetScaledCapsuleHalfHeight()+3);
   if(!GetWorld()->OverlapAnyTestByObjectType(Trial,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),Q)) { Landing=Trial; Found=true; }
  }
 }
 Transit=ETransit::None; Holder.Reset(); Flight.Reset(); Health=FMath::Max(1.f,Health*.5f);
 Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly); SetActorLocation(Landing,false); SetActorRotation(FRotator::ZeroRotator);
 RejoiningLane=true;
 RejoinPath=FCultMobaJungle::ReturnToLane(Team,Lane,Landing);
 RejoinPoint=RejoinPath[0];
 const auto Return=FCultMobaRoute::Project(Team,Lane,RejoinPath.Last());
 Waypoint=FMath::Min(FCultMobaRoute::Segments,FMath::CeilToInt(Return.Progress*FCultMobaRoute::Segments));
 AttackCooldown=1;
}
void ACultMatchUnit::ConsumeThrown(ACultUlhuPlayerPawn* Source) {
 if(Transit==ETransit::None) return;
 if(Flight.IsValid()) RemoveTickPrerequisiteActor(Flight.Get());
 Transit=ETransit::None; Flight.Reset(); Holder.Reset(); Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
 Hurt(Health+1,Source);
 if(Health>0) { Health=0; Die(); } // Also clean up if the match ended on this impact.
}
