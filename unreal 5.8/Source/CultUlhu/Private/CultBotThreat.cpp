#include "CultBotThreat.h"
#include "CultSkillAttack.h"
#include "CultUlhuPlayerPawn.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"

namespace {
bool ProjectedHit(ACultSkillAttack* A,ACultUlhuPlayerPawn* P,float& Time,FVector& Escape) {
 using Shape=ACultSkillAttack::EShape;
 if(!A->Hit||!ACultSkillAttack::Alive(A->GetOwner())||!ACultSkillAttack::Enemy(A->GetOwner(),P)||(A->TargetFilter&&!A->TargetFilter(P))) return false;
 auto* Capsule=P->GetCapsuleComponent(); const FVector Centre=P->GetActorLocation();
 const float Body=Capsule->GetScaledCapsuleRadius(),Spine=Capsule->GetScaledCapsuleHalfHeight()-Body;
 const FVector Bottom=Centre-FVector(0,0,Spine),Top=Centre+FVector(0,0,Spine),Origin=A->GetActorLocation();
 Time=A->TimeUntilActive(); FVector Visible=Origin,Contact=Centre; bool In=false;
 Escape=FVector::CrossProduct(A->Direction,FVector::UpVector).GetSafeNormal2D();
 if(A->Shape==Shape::Bolt||A->Shape==Shape::Beam) {
  if(A->Shape==Shape::Beam&&!A->IsWindingUp()) return false;
  const FVector Start=A->Shape==Shape::Bolt?A->ProjectilePosition():Origin;
  const float Remaining=FMath::Max(0.f,A->Reach-float(FVector::Dist(Start,Origin)));
  FVector RayPoint,BodyPoint; FMath::SegmentDistToSegmentSafe(Start,Start+A->Direction*Remaining,Bottom,Top,RayPoint,BodyPoint);
  In=FVector::DistSquared(RayPoint,BodyPoint)<=FMath::Square(Body+A->Radius);
  if(A->Shape==Shape::Bolt) { if(A->Speed<=0) return false; Time+=FMath::Max(0.f,float(FVector::DotProduct(RayPoint-Start,A->Direction))-Body-A->Radius)/A->Speed; }
  Visible=Start; Contact=RayPoint;
  if(In&&!ACultSkillAttack::Clear(P->GetWorld(),Start,RayPoint,A->GetOwner(),P)) return false;
 } else {
  if(!A->IsWindingUp()) return false;
  if(A->Shape==Shape::Sweep) {
   for(int I=0;I<=16&&!In;++I) {
    const FVector D=A->Direction.RotateAngleAxis(FMath::Lerp(-A->HalfAngle,A->HalfAngle,I/16.f),FVector::UpVector);
    FVector Arm,BodyPoint; FMath::SegmentDistToSegmentSafe(Origin+D*A->Radius,Origin+D*A->Reach,Bottom,Top,Arm,BodyPoint);
    In=FVector::DistSquared(Arm,BodyPoint)<=FMath::Square(Body+A->Radius); Contact=Arm;
   }
  } else if(A->Shape==Shape::GroundDisk) {
   Contact=Centre; Contact.Z=FMath::Clamp(Origin.Z,Centre.Z-Capsule->GetScaledCapsuleHalfHeight(),Centre.Z+Capsule->GetScaledCapsuleHalfHeight());
   In=FVector::Dist2D(Contact,Origin)<=A->Reach+Body&&FMath::Abs(Contact.Z-Origin.Z)<=A->Radius;
   Escape=(Centre-Origin).GetSafeNormal2D();
  } else {
   Capsule->GetClosestPointOnCollision(Origin,Contact); const auto Offset=Contact-Origin;
   In=Offset.Size()<=A->Reach;
   if(A->Shape==Shape::Cone) {
    const float Cos=FMath::Cos(FMath::DegreesToRadians(A->HalfAngle)); In=In&&(Offset.IsNearlyZero()||FVector::DotProduct(Offset.GetSafeNormal(),A->Direction)>=Cos);
    for(int I=0;I<=8&&!In;++I) { FVector Point; if(Capsule->GetClosestPointOnCollision(Origin+A->Direction*A->Reach*(I/8.f),Point)<0) continue; const auto Delta=Point-Origin;
     if(Delta.SizeSquared()<=FMath::Square(A->Reach+1)&&(Delta.IsNearlyZero()||FVector::DotProduct(Delta.GetSafeNormal(),A->Direction)>=Cos)) { In=true; Contact=Point; }
    }
   } else Escape=(Centre-Origin).GetSafeNormal2D();
  }
  if(In&&!ACultSkillAttack::Clear(P->GetWorld(),Origin+FVector(0,0,40),Contact,A->GetOwner(),P)) return false;
 }
 if(!In||Time>.55f) return false;
 // Observe the current attack volume/projectile, not a caster behind solid cover.
 if(!ACultSkillAttack::Clear(P->GetWorld(),Centre,Visible+FVector(0,0,60),P,A->GetOwner())) return false;
 if(Escape.IsNearlyZero()) Escape=P->GetActorRightVector();
 return true;
}
}
bool CultBotThreat::SafeDash(ACultUlhuPlayerPawn* P,FVector Direction) {
 const FVector D=Direction.GetSafeNormal2D(); if(D.IsNearlyZero()) return false;
 auto* Move=P->GetCharacterMovement(); auto* Cap=P->GetCapsuleComponent();
 const auto Shape=FCollisionShape::MakeCapsule(Cap->GetScaledCapsuleRadius()-2,Cap->GetScaledCapsuleHalfHeight()-2);
 FCollisionQueryParams Q(SCENE_QUERY_STAT(BotDashLanding),false,P); const FCollisionObjectQueryParams Static(ECC_WorldStatic);
 FVector Previous=P->GetActorLocation();
 for(int I=1;I<=3;++I) {
  FVector Probe=P->GetActorLocation()+D*P->DashTravelDistance()*(I/3.f); Probe.Z=Previous.Z;
  const float Step=Move->MaxStepHeight+100; FHitResult Floor,Path,CentreFloor;
  if(!P->GetWorld()->SweepSingleByObjectType(Floor,Probe+FVector(0,0,Step),Probe-FVector(0,0,Step),FQuat::Identity,Static,Shape,Q)||Floor.bStartPenetrating||!Move->IsWalkable(Floor)) return false;
  const FVector Feet=Probe-FVector(0,0,Cap->GetScaledCapsuleHalfHeight());
  if(!P->GetWorld()->LineTraceSingleByObjectType(CentreFloor,Feet+FVector(0,0,Step),Feet-FVector(0,0,Step),Static,Q)||!Move->IsWalkable(CentreFloor)) return false;
  const FVector Next=Floor.Location+FVector(0,0,3);
  if(P->GetWorld()->SweepSingleByObjectType(Path,Previous,Next,FQuat::Identity,Static,Shape,Q)&&(Path.bStartPenetrating||!Move->IsWalkable(Path))) return false;
  Previous=Next;
 }
 return true;
}
bool CultBotThreat::Avoiding(const FCultBotThreatMemory& Memory,float Now) { return Memory.Seen.IsValid()&&Now<Memory.EscapeUntil; }
bool CultBotThreat::Respond(ACultUlhuPlayerPawn* P,FCultBotThreatMemory& M,float Now,int Side) {
 if(Avoiding(M,Now)||Now<M.NextObservation) return false;
 M.NextObservation=Now+.06f;
 ACultSkillAttack* Threat=nullptr; FVector Escape; float Soon=.56f;
 for(TActorIterator<ACultSkillAttack> It(P->GetWorld());It;++It) {
  float Time; FVector Direction; if(ProjectedHit(*It,P,Time,Direction)&&Time<Soon) { Soon=Time; Threat=*It; Escape=Direction; }
 }
 if(!Threat) { M.Seen.Reset(); M.EscapeUntil=0; return false; }
 if(M.Seen!=Threat) { M.Seen=Threat; M.Since=Now; M.EscapeUntil=0; return false; }
 if(Now-M.Since<.16f+((Side%5)%3)*.025f||!P->CanDash()) return false;
 if(Threat->Shape==ACultSkillAttack::EShape::Bolt||Threat->Shape==ACultSkillAttack::EShape::Beam||Threat->Shape==ACultSkillAttack::EShape::Sweep||Threat->Shape==ACultSkillAttack::EShape::Cone) Escape*=Side%2?1.f:-1.f;
 for(float Angle:{0.f,180.f,45.f,-45.f}) {
  const FVector Direction=Escape.RotateAngleAxis(Angle,FVector::UpVector);
  if(SafeDash(P,Direction)&&P->TryDash(Direction)) { M.Escape=Direction; M.EscapeUntil=Now+Soon+.1f; return true; }
 }
 return false;
}
