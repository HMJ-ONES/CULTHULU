#include "CultSkillAttack.h"
#include "CultCombatCredit.h"
#include "CultUlhuPlayerPawn.h"
#include "CultPopulation.h"
#include "CultFeralBeast.h"
#include "CultDestructible.h"
#include "CultMatchUnit.h"
#include "CultMatchDirector.h"
#include "CultBlastFX.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

namespace {
bool FriendlyBody(AActor* Source,AActor* Other) {
 const auto* Unit=Cast<ACultMatchUnit>(Other);
 const bool Body=Cast<ACultUlhuPlayerPawn>(Other)||Cast<ACultPopulation>(Other)||(Unit&&!Unit->IsStructure());
 return Body&&ACultSkillAttack::Alive(Other)&&!ACultSkillAttack::Enemy(Source,Other);
}
}
ACultSkillAttack::ACultSkillAttack() {
 PrimaryActorTick.bCanEverTick=true; RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("AttackOrigin"));
 Visual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AttackVisual")); Visual->SetupAttachment(RootComponent); Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision); Visual->SetCastShadow(false);
}
bool ACultSkillAttack::Alive(AActor* A) { if(!IsValid(A)) return false; if(auto* U=Cast<ACultMatchUnit>(A)) return U->Health>0&&!U->IsInTransit(); if(auto* P=Cast<ACultPopulation>(A)) return P->GetHealth()>0; if(auto* B=Cast<ACultFeralBeast>(A)) return !B->IsDead(); if(auto* P=Cast<ACultUlhuPlayerPawn>(A)) return P->GetHealth()>0; if(auto* B=Cast<ACultDestructible>(A)) return !B->IsDestroyed(); return false; }
bool ACultSkillAttack::Enemy(AActor* S,AActor* A) {
 if(!Alive(A)||A==S) return false;
 int F=-2; if(auto* U=Cast<ACultMatchUnit>(S)) F=U->Team; if(auto* P=Cast<ACultUlhuPlayerPawn>(S)) F=P->GetFaction(); if(auto* P=Cast<ACultPopulation>(S)) F=P->GetFaction();
 int Other=-1; if(auto* U=Cast<ACultMatchUnit>(A)) Other=U->Team; if(auto* P=Cast<ACultUlhuPlayerPawn>(A)) Other=P->GetFaction(); if(auto* P=Cast<ACultPopulation>(A)) Other=P->GetFaction(); if(auto* P=Cast<ACultDestructible>(A)) Other=P->GetFaction();
 return F<0||Other!=F;
}
bool ACultSkillAttack::Clear(UWorld* W,FVector A,FVector B,AActor* S,AActor* T) {
 FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(SkillCover),false,S); Q.AddIgnoredActor(T);
 for(TActorIterator<AActor> It(W);It;++It) if(FriendlyBody(S,*It)) Q.AddIgnoredActor(*It);
 return !W->LineTraceSingleByChannel(H,A,B,ECC_Visibility,Q);
}
float ACultSkillAttack::HealthOf(AActor* A) {
 if(!IsValid(A)) return 0;
 if(auto* U=Cast<ACultMatchUnit>(A)) return FMath::Max(0.f,U->Health);
 if(auto* N=Cast<ACultPopulation>(A)) return FMath::Max(0.f,N->GetHealth());
 if(auto* N=Cast<ACultFeralBeast>(A)) return N->IsDead()?0.f:FMath::Max(0.f,N->GetHealth());
 if(auto* N=Cast<ACultUlhuPlayerPawn>(A)) return FMath::Max(0.f,N->GetHealth());
 if(auto* N=Cast<ACultDestructible>(A)) return N->IsDestroyed()?0.f:FMath::Max(0.f,N->GetHealth());
 return 0;
}
void ACultSkillAttack::Hurt(AActor* A,float D,AActor* S,FVector P) {
 const float Before=HealthOf(A);
 int F=-1; if(auto* C=Cast<ACultUlhuPlayerPawn>(S)) F=C->GetFaction(); if(auto* C=Cast<ACultPopulation>(S)) F=C->GetFaction();
 if(auto* U=Cast<ACultMatchUnit>(A)) U->Hurt(D,S);
 else if(auto* C=Cast<ACultPopulation>(A)) C->ReceiveAttack(D,F,false,Cast<ACultFeralBeast>(S)!=nullptr);
 else if(auto* Beast=Cast<ACultFeralBeast>(A)) Beast->ReceiveCombatDamage(D,S);
 else if(auto* Avatar=Cast<ACultUlhuPlayerPawn>(A)) Avatar->ReceiveDamage(D,S);
 else if(auto* Building=Cast<ACultDestructible>(A)) Building->ApplyDamage(D,P,F,S);
 if(auto* Summon=Cast<ACultPopulation>(S)) if(Summon->IsSummoned()) if(auto* Summoner=CultCombatCredit::Hero(S)) Summoner->SummonLandedDamage(FMath::Clamp(Before-HealthOf(A),0.f,Before));
 if(!Cast<ACultUlhuPlayerPawn>(A)) if(auto* Player=CultCombatCredit::Hero(S)) {
  const float After=HealthOf(A); Player->RecordCombatHit(A,P,FMath::Clamp(Before-After,0.f,Before),After<=0);
 }
}
void ACultSkillAttack::BeginPlay() {
 Super::BeginPlay(); BornAt=GetWorld()->GetTimeSeconds(); Origin=Previous=GetActorLocation(); Direction.Normalize(); if(Shape==EShape::Sweep&&GetOwner()) AttachToActor(GetOwner(),FAttachmentTransformRules::KeepWorldTransform);
 BuildPresentation();
}

void ACultSkillAttack::ShowSegment(FVector A,FVector B,float Width) { Visual->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder"))); Visual->SetWorldLocation((A+B)*.5); Visual->SetWorldRotation(FRotationMatrix::MakeFromZ(B-A).Rotator()); Visual->SetWorldScale3D(FVector(Width/50,Width/50,FVector::Dist(A,B)/100)); }
void ACultSkillAttack::Sweep(FVector A,FVector B,float R) {
 FCollisionQueryParams Q(SCENE_QUERY_STAT(SkillSweep),false,GetOwner()); Q.AddIgnoredActor(this);
 for(TActorIterator<AActor> It(GetWorld());It;++It) if(FriendlyBody(GetOwner(),*It)||(TargetFilter&&Alive(*It)&&!TargetFilter(*It))) Q.AddIgnoredActor(*It);
 TArray<FHitResult> Hits; GetWorld()->SweepMultiByChannel(Hits,A,B,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(R),Q);
 // Tip chords can lie beyond a wall even when every radial trace is blocked.
 // A melee contact must also remain connected to the actual arm origin.
 for(const auto& H:Hits) { auto* Other=H.GetActor(); if(Enemy(GetOwner(),Other)&&!Struck.Contains(Other)&&(Shape!=EShape::Sweep||Clear(GetWorld(),Origin,H.ImpactPoint,GetOwner(),Other))) { Struck.Add(Other); if(Hit) Hit(Other,H.ImpactPoint); } if(H.bBlockingHit) { if(Shape==EShape::Bolt) { if(Finished) Finished(H.ImpactPoint); Destroy(); } break; } }
}
bool ACultSkillAttack::AreaContact(EShape Shape,FVector Origin,FVector Direction,float Reach,float Radius,float HalfAngle,AActor* Target,FVector& Point) {
  Point=Target->GetActorLocation(); float Body=0; if(auto* C=Target->FindComponentByClass<UCapsuleComponent>()) {
   Body=C->GetScaledCapsuleRadius();
   if(Shape==EShape::GroundDisk) Point.Z=FMath::Clamp(Origin.Z,Point.Z-C->GetScaledCapsuleHalfHeight(),Point.Z+C->GetScaledCapsuleHalfHeight());
   else { FVector Closest; if(C->GetClosestPointOnCollision(Origin,Closest)>=0) Point=Closest; Body=0; }
  }
  if(auto* B=Cast<ACultDestructible>(Target)) Point=B->GetComponentsBoundingBox(true).GetClosestPointTo(Origin);
  const FVector Offset=Point-Origin;
  bool In=Shape==EShape::GroundDisk?Offset.Size2D()<=Reach+Body&&FMath::Abs(Offset.Z)<=Radius:Offset.Size()<=Reach+Body;
  if(Shape==EShape::Cone) {
   const float Cos=FMath::Cos(FMath::DegreesToRadians(HalfAngle));
   In=In&&(Offset.IsNearlyZero()||FVector::DotProduct(Offset.GetSafeNormal(),Direction)>=Cos);
   // A giant capsule can intersect a cone even when its nearest point to the
   // caster lies outside the cone. Test actual collider points near the committed axis.
   if(!In) if(auto* Capsule=Target->FindComponentByClass<UCapsuleComponent>()) for(int I=0;I<=8;++I) {
    FVector Candidate; if(Capsule->GetClosestPointOnCollision(Origin+Direction*Reach*(I/8.f),Candidate)<0) continue;
    const FVector Delta=Candidate-Origin;
    if(Delta.SizeSquared()<=FMath::Square(Reach+1)&&(Delta.IsNearlyZero()||FVector::DotProduct(Delta.GetSafeNormal(),Direction)>=Cos)) { In=true; Point=Candidate; break; }
   }
  }
  return In;
}
void ACultSkillAttack::Area() {
 for(TActorIterator<AActor> It(GetWorld());It;++It) if(Enemy(GetOwner(),*It)) {
  FVector Point;
  if(AreaContact(Shape,Origin,Direction,Reach,Radius,HalfAngle,*It,Point)&&Clear(GetWorld(),Origin+FVector(0,0,40),Point,GetOwner(),*It)&&!Struck.Contains(*It)) { Struck.Add(*It); if(Hit) Hit(*It,Point); }
 }
}
void ACultSkillAttack::Tick(float Dt) {
 Super::Tick(Dt); if(!Alive(GetOwner())) { Destroy(); return; }
 // Spawned during an actor tick, an attack can tick again in that same frame.
 // Its first delta must exclude time before creation, especially on load hitches.
 if(FirstTick) { Dt=FMath::Min(Dt,FMath::Max(0.f,GetWorld()->GetTimeSeconds()-BornAt)); FirstTick=false; }
 if(Shape==EShape::Sweep) Origin=GetActorLocation(); const float Old=Age; Age+=Dt;
 if(Age<Windup) { UpdatePresentation(); return; }
 const float Active=FMath::Min(Age-Windup,Duration);
 if(Shape==EShape::Bolt) {
  float Step=FMath::Min(Speed*(Age-FMath::Max(Old,Windup)),Reach-Travel); FVector Next=Previous+Direction*Step; Sweep(Previous,Next,Radius); Travel+=Step; Previous=Next; Visual->SetWorldLocation(Next); if(Travel>=Reach) { if(Finished) Finished(Next); Destroy(); }
 } else if(Shape==EShape::Sweep) {
  const float Start=FMath::Max(0.f,Old-Windup), End=Active;
  const int Steps=FMath::Max(1,FMath::CeilToInt((End-Start)/.012f));
  for(int I=0;I<=Steps;++I) { const float T=FMath::Lerp(Start,End,I/float(Steps))/Duration; FVector D=Direction.RotateAngleAxis(FMath::Lerp(-HalfAngle,HalfAngle,T),FVector::UpVector); FVector Tip=Origin+D*Reach; if(I>0) Sweep(Previous,Tip,Radius); Sweep(Origin+D*Radius,Tip,Radius);
   Previous=Tip; }
 } else if(Old<=Windup) { if(Shape==EShape::Beam) { Sweep(Origin,Origin+Direction*Reach,Radius); ShowSegment(Origin,Origin+Direction*Reach,Radius); } else Area(); }
 if(!IsActorBeingDestroyed()) UpdatePresentation();
 if(Age>=Windup+Duration) { if(Finished&&Shape!=EShape::Bolt) Finished(Origin); Destroy(); }
}
