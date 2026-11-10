#include "CultSkillAttack.h"
#include "CultAbilityFX.h"
#include "CultReplicatedBody.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
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
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

namespace {
bool FriendlyBody(AActor* Source,AActor* Other) {
 if(auto* Attack=Cast<ACultSkillAttack>(Other)) return Attack->IsGuarding()&&Attack->GetOwner()==Source;
 const auto* Unit=Cast<ACultMatchUnit>(Other);
 const bool Body=Cast<ACultUlhuPlayerPawn>(Other)||Cast<ACultPopulation>(Other)||(Unit&&!Unit->IsStructure());
 return Body&&ACultSkillAttack::Alive(Other)&&!ACultSkillAttack::Enemy(Source,Other);
}
}
ACultSkillAttack::ACultSkillAttack() {
 bReplicates=true; SetReplicateMovement(true); SetNetUpdateFrequency(20.f); SetNetCullDistanceSquared(100000.f*100000.f); CreateDefaultSubobject<UCultReplicatedBody>(TEXT("NetworkBody"));
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
 Super::BeginPlay(); BornAt=GetWorld()->GetTimeSeconds(); Origin=Previous=GetActorLocation(); Direction.Normalize(); if(Shape==EShape::Sweep&&GetOwner()) SweepOffset=Origin-GetOwner()->GetActorLocation();
 if((Shape==EShape::Sweep||bTrackMuzzle)&&GetOwner()) SweepOffset=Origin-GetOwner()->GetActorLocation();
 if(HasAuthority()&&WardLocked) WardChargeRemaining=Windup;
 if(HasAuthority()&&FadeSeconds>0) {
  const auto* State=GetWorld()->GetGameState();
  FadeAt=(State?State->GetServerWorldTimeSeconds():BornAt)+Windup+Duration;
 }
 BuildPresentation();
 OnRep_RockGuard();
}

void ACultSkillAttack::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
 Super::GetLifetimeReplicatedProps(OutLifetimeProps);
 DOREPLIFETIME(ACultSkillAttack,WardTarget); DOREPLIFETIME(ACultSkillAttack,WardLocked); DOREPLIFETIME(ACultSkillAttack,WardChargeRemaining);
 DOREPLIFETIME_CONDITION(ACultSkillAttack,SignalStyle,COND_InitialOnly);
 DOREPLIFETIME_CONDITION(ACultSkillAttack,AbilityLook,COND_InitialOnly);
 DOREPLIFETIME_CONDITION(ACultSkillAttack,bTrackMuzzle,COND_InitialOnly);
 DOREPLIFETIME_CONDITION(ACultSkillAttack,RockExtents,COND_InitialOnly);
 DOREPLIFETIME_CONDITION(ACultSkillAttack,FadeAt,COND_InitialOnly);
 DOREPLIFETIME_CONDITION(ACultSkillAttack,FadeSeconds,COND_InitialOnly);
 DOREPLIFETIME(ACultSkillAttack,bRockGuard);
}
void ACultSkillAttack::ConfigureProjectileFlight(float BaseReach,bool LongFlight) {
 // Long rounds stay solid through the playable engagement. A generous cap
 // still collects missed shots sent out into empty sky.
 Reach=LongFlight?FMath::Max(BaseReach*2.8f,Speed*18.f):BaseReach*1.35f;
 Duration=Reach/FMath::Max(1.f,Speed);
 FalloffStart=LongFlight?BaseReach*.65f:0.f;
 FadeSeconds=LongFlight?0.f:Duration*.22f;
}
float ACultSkillAttack::ProjectileDamageScale(FVector Contact) const {
 if(FalloffStart<=0) return 1.f;
 const float Distance=Travel+FVector::Dist(Previous,Contact);
 const float FalloffSpan=FalloffStart*((2.8f-.65f)/.65f);
 const float Progress=FMath::Clamp((Distance-FalloffStart)/FMath::Max(1.f,FalloffSpan),0.f,1.f);
 return FMath::Lerp(1.f,.4f,Progress);
}
float ACultSkillAttack::FlightVisibility() const {
 if(FadeSeconds<=0) return 1.f;
 if(HasAuthority()) {
  const float Remaining=FMath::Min((Reach-Travel)/FMath::Max(1.f,Speed),Duration-FMath::Max(0.f,Age-Windup));
  return FMath::Clamp(Remaining/FadeSeconds,0.f,1.f);
 }
 const auto* State=GetWorld()->GetGameState();
 const float Now=State?State->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds();
 return FMath::Clamp((FadeAt-Now)/FadeSeconds,0.f,1.f);
}
void ACultSkillAttack::OnRep_WardCountdown() { WardCountdownReceivedAt=GetWorld()->GetTimeSeconds(); }
float ACultSkillAttack::WardReleaseRemaining() const {
 if(HasAuthority()) return WardChargeRemaining;
 const auto* PC=GetWorld()->GetFirstPlayerController(); const auto* State=PC?PC->PlayerState.Get():nullptr;
 const float Transit=State?State->GetPingInMilliseconds()*.0005f:0.f;
 return FMath::Max(0.f,WardChargeRemaining-(GetWorld()->GetTimeSeconds()-WardCountdownReceivedAt)-Transit);
}

void ACultSkillAttack::ShowSegment(FVector A,FVector B,float Width) { Visual->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder"))); Visual->SetWorldLocation((A+B)*.5); Visual->SetWorldRotation(FRotationMatrix::MakeFromZ(B-A).Rotator()); Visual->SetWorldScale3D(FVector(Width/50,Width/50,FVector::Dist(A,B)/100)); }
void ACultSkillAttack::Sweep(FVector A,FVector B,float R) {
 FCollisionQueryParams Q(SCENE_QUERY_STAT(SkillSweep),false,GetOwner()); Q.AddIgnoredActor(this);
 for(TActorIterator<AActor> It(GetWorld());It;++It) if(FriendlyBody(GetOwner(),*It)||(TargetFilter&&Alive(*It)&&!TargetFilter(*It))) Q.AddIgnoredActor(*It);
 TArray<FHitResult> Hits; GetWorld()->SweepMultiByChannel(Hits,A,B,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(R),Q);
 // Tip chords can lie beyond a wall even when every radial trace is blocked.
 // A melee contact must also remain connected to the actual arm origin.
 for(const auto& H:Hits) {
  auto* Other=H.GetActor();
  if(Enemy(GetOwner(),Other)&&!Struck.Contains(Other)&&(Shape!=EShape::Sweep||Clear(GetWorld(),Origin,H.ImpactPoint,GetOwner(),Other))) {
   Struck.Add(Other);
   if(Hit) Hit(Other,H.ImpactPoint);
   if(Shape==EShape::Sweep&&Struck.Num()==1) {
    EmitAbilityFX(uint8(CultAbilityFX::EEvent::Contact),H.ImpactPoint,-Direction,FMath::Max(130.f,Radius*3.2f));
   }
  }
  if(!H.bBlockingHit) continue;
  if(Shape==EShape::Bolt) {
   if(RockExtents.X>0&&!Hit) {
    EmitAbilityFX(uint8(CultAbilityFX::EEvent::Contact),H.ImpactPoint,FVector::UpVector,FMath::Min(780.f,Radius*1.8f));
    SetActorLocation(H.Location);
    SetActorTickEnabled(false);
    SetLifeSpan(.65f);
    return;
   }
   EmitAbilityFX(uint8(CultAbilityFX::EEvent::Contact),H.ImpactPoint,H.ImpactNormal,FMath::Clamp(Radius*3.2f,105.f,850.f));
   if(Finished) Finished(H.ImpactPoint);
   Destroy();
  }
  if(Shape==EShape::Beam) {
   Previous=H.ImpactPoint;
   EmitAbilityFX(uint8(CultAbilityFX::EEvent::Contact),Previous,H.ImpactNormal,FMath::Max(135.f,Radius*4.2f));
  }
  break;
 }
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
 Super::Tick(Dt);
 if(!HasAuthority()) {
  Age+=Dt;
  if(Shape==EShape::Bolt&&RockExtents.X<=0) { Previous=GetActorLocation(); Visual->SetWorldLocation(Previous); }
  UpdatePresentation();
  return;
 }
 if(!Alive(GetOwner())&&!(FalloffStart>0&&!IsWindingUp()&&IsValid(GetOwner()))) { Destroy(); return; }
 if(bRockGuard) { Age+=Dt; PositionGuard(); UpdateAbilityFilaments(); return; }
 if(Shape==EShape::Bolt&&bTrackMuzzle&&IsWindingUp()) {
  if(auto* Pawn=Cast<ACultUlhuPlayerPawn>(GetOwner()); Pawn&&(Pawn->IsIncapacitated()||Pawn->IsDashing())) { Destroy(); return; }
  Origin=Previous=GetOwner()->GetActorLocation()+SweepOffset;
  Direction=(PrimaryAim-Origin).GetSafeNormal();
  SetActorLocation(Origin);
 }
 // Spawned during an actor tick, an attack can tick again in that same frame.
 // Its first delta must exclude time before creation, especially on load hitches.
 if(FirstTick) { Dt=FMath::Min(Dt,FMath::Max(0.f,GetWorld()->GetTimeSeconds()-BornAt)); FirstTick=false; }
 if(Shape==EShape::Sweep) {
  Origin=GetOwner()->GetActorLocation()+SweepOffset;
  // Keep a step-assisted strike aligned to the committed world point as the
  // root advances. The point is fixed at input time, so opponents can evade.
  if(bTrackMuzzle) {
   const FVector Committed=(PrimaryAim-Origin).GetSafeNormal();
   if(!Committed.IsNearlyZero()) Direction=Committed;
  }
  SetActorLocation(Origin);
 }
 const float Old=Age; Age+=Dt;
 if(WardLocked) WardChargeRemaining=FMath::Max(0.f,Windup-Age);
 if(Age<Windup) { UpdatePresentation(); return; }
 const float Active=FMath::Min(Age-Windup,Duration);
 if(Shape==EShape::Bolt&&WardLocked) {
  auto* Target=WardTarget.Get();
  if(!Alive(Target)||!Enemy(GetOwner(),Target)) { Destroy(); return; }
  // A timed dash breaks this shot permanently; jumping and running do not.
  if(auto* Hero=Cast<ACultUlhuPlayerPawn>(Target)) if(Hero->IsDashing()) { Hero->ConfirmEvade(); Destroy(); return; }
  FVector Aim=Target->GetActorLocation();
  if(auto* Capsule=Target->FindComponentByClass<UCapsuleComponent>()) { FVector Closest; if(Capsule->GetClosestPointOnCollision(Previous,Closest)>=0) Aim=Closest; }
  const float Step=Speed*(Age-FMath::Max(Old,Windup));
  Direction=(Aim-Previous).GetSafeNormal();
  if(FVector::Dist(Previous,Aim)<=Step+Radius) { if(Hit) Hit(Target,Aim); if(Finished) Finished(Aim); Destroy(); return; }
  Previous+=Direction*Step; SetActorLocation(Previous); Visual->SetWorldLocation(Previous);
 } else if(Shape==EShape::Bolt) {
  FVector Segment;
  if(Gravity>0) {
   const float Start=FMath::Max(0.f,Old-Windup);
   if(LaunchVelocity.IsNearlyZero()) {
    const float Flight=FMath::Max(.05f,FMath::Min(float(FVector::Dist(PrimaryAim,Origin)),Reach*.93f)/Speed);
    LaunchVelocity=Direction*Speed+FVector(0,0,.5f*Gravity*Flight);
   }
   Segment=(LaunchVelocity-FVector(0,0,Gravity*(Active+Start)*.5f))*FMath::Max(0.f,Active-Start);
  } else {
   Segment=Direction*(Speed*FMath::Max(0.f,Active-FMath::Max(0.f,Old-Windup)));
  }
  Segment=Segment.GetClampedToMaxSize(FMath::Max(0.f,Reach-Travel));
  const FVector Next=Previous+Segment;
  Sweep(Previous,Next,Radius);
  if(IsActorBeingDestroyed()||!IsActorTickEnabled()) return;
  Travel+=Segment.Size();
  Previous=Next;
  SetActorLocation(Next);
  if(RockExtents.X<=0) Visual->SetWorldLocation(Next);
  if(Travel>=Reach-.01f||Active>=Duration) { if(Finished) Finished(Next); Destroy(); return; }
 } else if(Shape==EShape::Sweep) {
  const float Start=FMath::Max(0.f,Old-Windup), End=Active;
  const int Steps=FMath::Max(1,FMath::CeilToInt((End-Start)/.012f));
  for(int I=0;I<=Steps;++I) { const float T=FMath::Lerp(Start,End,I/float(Steps))/Duration; FVector D=Direction.RotateAngleAxis(FMath::Lerp(-HalfAngle,HalfAngle,T),FVector::UpVector); FVector Tip=Origin+D*Reach; if(I>0) Sweep(Previous,Tip,Radius); Sweep(Origin+D*Radius,Tip,Radius);
   Previous=Tip; }
 } else if(Old<=Windup) {
  if(Shape==EShape::Beam) {
   Previous=Origin+Direction*Reach;
   Sweep(Origin,Previous,Radius);
   ShowSegment(Origin,Previous,Radius);
  } else {
   Area();
   const float Size=Shape==EShape::Cone?Reach*.60f:Reach;
   EmitAbilityFX(uint8(CultAbilityFX::EEvent::Release),Origin,Direction,Size);
  }
 }
 if(!IsActorBeingDestroyed()) UpdatePresentation();
 if(Age>=Windup+Duration) { if(Finished&&Shape!=EShape::Bolt) Finished(Origin); Destroy(); }
}
