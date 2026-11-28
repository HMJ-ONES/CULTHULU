#include "CultChaosShield.h"
#include "CultAbilityFX.h"
#include "CultMatchUnit.h"
#include "CultPopulation.h"
#include "CultUlhuPlayerPawn.h"
#include "Components/BoxComponent.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"

namespace {
int ProjectileFaction(const AActor* Source) {
 if(auto* Pawn=Cast<ACultUlhuPlayerPawn>(Source)) return Pawn->GetFaction();
 if(auto* Unit=Cast<ACultMatchUnit>(Source)) return Unit->Team;
 if(auto* Follower=Cast<ACultPopulation>(Source)) return Follower->GetFaction();
 return INDEX_NONE;
}
}

ACultChaosShield::ACultChaosShield() {
 bReplicates=true;
 SetNetCullDistanceSquared(FMath::Square(100000.f));
 Collision=CreateDefaultSubobject<UBoxComponent>(TEXT("ProjectileSurface"));
 SetRootComponent(Collision);
 Collision->SetBoxExtent(FVector(45,HalfWidth,Height*.5f));
 Collision->SetCollisionObjectType(ECC_GameTraceChannel1);
 Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
 Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
 Collision->SetGenerateOverlapEvents(false);
 Collision->SetCanEverAffectNavigation(false);
 Surface=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Veil"));
 Surface->SetupAttachment(Collision);
 Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Surface->SetCastShadow(false);
 Surface->SetCanEverAffectNavigation(false);
}

void ACultChaosShield::Initialize(int InFaction,float Integrity) {
 Faction=InFaction;
 Remaining=Capacity=Integrity;
}

void ACultChaosShield::BeginPlay() {
 Super::BeginPlay();
 if(HasAuthority()) SetLifeSpan(Lifetime);
 if(GetNetMode()!=NM_DedicatedServer) {
  auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CultUlhu/Materials/M_AbilityFilament"));
  if(Material) Surface->SetMaterial(0,Material);
  UpdateAppearance();
 }
}

void ACultChaosShield::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
 Super::GetLifetimeReplicatedProps(OutLifetimeProps);
 DOREPLIFETIME_CONDITION(ACultChaosShield,Faction,COND_InitialOnly);
 DOREPLIFETIME_CONDITION(ACultChaosShield,Capacity,COND_InitialOnly);
 DOREPLIFETIME(ACultChaosShield,Remaining);
}

void ACultChaosShield::Absorb(float Amount) {
 if(!HasAuthority()||Remaining<=0||Amount<=0) return;
 Remaining=FMath::Max(0.f,Remaining-Amount);
 if(Remaining<=0) {
  if(auto* Pawn=Cast<ACultUlhuPlayerPawn>(GetOwner()))
   Pawn->EmitAbilityBurst(uint8(CultAbilityFX::ELook::Dread),uint8(CultAbilityFX::EEvent::Release),
    GetActorLocation(),GetActorForwardVector(),700.f);
  Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Destroy();
 } else {
  UpdateAppearance();
  ForceNetUpdate();
 }
}

bool ACultChaosShield::FirstHostile(UWorld* World,AActor* Source,FVector Start,FVector End,
 const FCollisionShape& Shape,FQuat Rotation,FHitResult& Hit) {
 FCollisionQueryParams Query(SCENE_QUERY_STAT(CultChaosShield),false,Source);
 const int Team=ProjectileFaction(Source);
 // Object sweeps stop at the first panel. Retrace past allied panels to
 // find the first hostile one along this same physical flight segment.
 while(World->SweepSingleByObjectType(Hit,Start,End,Rotation,
   FCollisionObjectQueryParams(ECC_GameTraceChannel1),Shape,Query)) {
  auto* Shield=Cast<ACultChaosShield>(Hit.GetActor());
  if(!Shield) return false;
  if(Team<0||Team!=Shield->Faction) return true;
  Query.AddIgnoredActor(Shield);
 }
 return false;
}

void ACultChaosShield::UpdateAppearance() {
 if(GetNetMode()==NM_DedicatedServer) return;
 TArray<FVector> Points,Normals;
 TArray<int32> Indices;
 TArray<FVector2D> UV;
 TArray<FLinearColor> Colors;
 const float Strength=FMath::Clamp(Remaining/FMath::Max(1.f,Capacity),0.f,1.f);
 const FLinearColor Violet(.48f,.25f,.78f,1.f);
 auto Strip=[&](FVector A,FVector B,FVector Across,float Alpha) {
  const int32 Index=Points.Num();
  Points.Append({A-Across,A+Across,B+Across,B-Across});
  Indices.Append({Index,Index+1,Index+2,Index,Index+2,Index+3});
  UV.Append({{.12f,0},{.88f,0},{.88f,1},{.12f,1}});
  for(int I=0;I<4;++I) { Normals.Add(FVector::ForwardVector); Colors.Add(FLinearColor(Violet.R,Violet.G,Violet.B,Alpha)); }
 };
 const float HalfHeight=Height*.5f;
 Strip(FVector(0,-HalfWidth,0),FVector(0,HalfWidth,0),FVector(0,0,HalfHeight),.16f+.12f*Strength);
 for(int Side:{-1,1}) {
  Strip(FVector(0,Side*(HalfWidth-23),-HalfHeight),FVector(0,Side*(HalfWidth-23),HalfHeight),FVector(0,23,0),.55f*Strength);
  Strip(FVector(0,-HalfWidth,Side*(HalfHeight-23)),FVector(0,HalfWidth,Side*(HalfHeight-23)),FVector(0,0,23),.55f*Strength);
  for(int Mark=-2;Mark<=2;++Mark) {
   const float Y=Mark*725.f,Z=Side*560.f;
   Strip(FVector(0,Y-175,Z),FVector(0,Y,Z+Side*225),FVector(0,17,0),.45f*Strength);
   Strip(FVector(0,Y,Z+Side*225),FVector(0,Y+175,Z),FVector(0,17,0),.45f*Strength);
  }
 }
 Surface->CreateMeshSection_LinearColor(0,Points,Indices,Normals,UV,Colors,{},false,false);
}
