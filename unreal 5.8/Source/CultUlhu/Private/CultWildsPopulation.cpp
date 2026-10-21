#include "CultLivingLandscape.h"
#include "CultFeralBeast.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
namespace {
void InstanceMaterials(UInstancedStaticMeshComponent* C,const FString& Name) { FString Id=Name.Replace(TEXT("-"),TEXT("_")); for(int I=0;I<C->GetNumMaterials();++I) { FString Path=FString::Printf(TEXT("/Game/CultUlhu/Materials/M_Living_%s_%d"),*Id,I); if(auto* M=LoadObject<UMaterialInterface>(nullptr,*Path)) C->SetMaterial(I,M); } }
}

void PopulateCultWilds(UWorld* World)
{
 auto* Land=ACultLivingLandscape::Find(World); if(!Land) return;
 FRandomStream R(FMath::Rand());
 const TArray<FString> Names={TEXT("cave-rock-tall-a"),TEXT("rocks-tall"),TEXT("tree-dead-large"),TEXT("pine-dead-crooked"),TEXT("dark-crystal-cluster")};
 for(int Type=0;Type<Names.Num();++Type) {
  const FString Path=TEXT("/Game/CultUlhu/World/")+Names[Type]+TEXT("/StaticMeshes/")+Names[Type];
  auto* Mesh=LoadObject<UStaticMesh>(nullptr,*Path); if(!Mesh) continue;
  auto* C=NewObject<UInstancedStaticMeshComponent>(Land); Land->AddInstanceComponent(C); C->SetupAttachment(Land->GetRootComponent()); C->SetStaticMesh(Mesh); InstanceMaterials(C,Names[Type]); C->SetForceDisableNanite(true); C->SetCollisionProfileName(TEXT("BlockAll")); C->RegisterComponent();
  for(int I=0;I<110;++I) {
   FVector P(R.FRandRange(-165000.f,165000.f),R.FRandRange(-165000.f,165000.f),0);
   bool NearCave=false; for(auto& Cave:ACultLivingLandscape::CaveSites()) if(FVector::Dist2D(P,Cave)<16000) NearCave=true; if(NearCave) continue;
   if(P.Size2D()<24000 || FVector::Dist2D(P,FVector(75000,65000,0))<18000 || FVector::Dist2D(P,FVector(-85000,70000,0))<18000) continue;
   if(!ACultLivingLandscape::Surface(World,P,P,.55f)) continue;
   const float Height=Type<2?R.FRandRange(3500.f,13000.f):Type==4?R.FRandRange(1000.f,2500.f):R.FRandRange(1800.f,4000.f);
   const float Scale=Height/FMath::Max(1.f,Mesh->GetBounds().BoxExtent.Z*2);
   P.Z-=Mesh->GetBoundingBox().Min.Z*Scale;
   C->AddInstance(FTransform(FRotator(0,R.FRandRange(0,360),0),P,FVector(Scale)),true);
  }
 }
 // Rock mass wraps the walk-through vaults; entrances remain clear for giant vessels.
 if(auto* Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/CultUlhu/World/cave-wall-rock/StaticMeshes/cave-wall-rock"))) {
  auto* C=NewObject<UInstancedStaticMeshComponent>(Land); Land->AddInstanceComponent(C); C->SetupAttachment(Land->GetRootComponent()); C->SetStaticMesh(Mesh); InstanceMaterials(C,TEXT("cave-wall-rock")); C->SetForceDisableNanite(true); C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->RegisterComponent();
  const FVector Size=Mesh->GetBoundingBox().GetSize();
  auto Mass=[&](FVector P,FVector Dimensions) { const FVector Scale=Dimensions/Size; P.Z-=Mesh->GetBoundingBox().Min.Z*Scale.Z; C->AddInstance(FTransform(FRotator::ZeroRotator,P,Scale),true); };
  for(auto& Cave:ACultLivingLandscape::CaveSites()) for(int D=0;D<5;++D) {
   for(int Side:{-1,1}) Mass(Cave+FVector(Side*6500,-6500+D*3200,1400),FVector(5000,4200,9000));
   Mass(Cave+FVector(0,-6500+D*3200,7400),FVector(11000,4200,2500));
  }
 }
 TArray<FVector> Encounters=ACultLivingLandscape::CaveSites(); Encounters.Add(FVector(9000,9000,0)); Encounters.Add(FVector(-30000,20000,0)); Encounters.Add(FVector(50000,-30000,0));
 const TArray<FString> Species={TEXT("skittering_ghoul"),TEXT("charnel_imp"),TEXT("pale_wight"),TEXT("ossified_brute")};
 for(const auto& Center:Encounters) for(int I=0;I<5;++I) {
  FVector P; if(!ACultLivingLandscape::Surface(World,Center+FVector(R.FRandRange(-2500.f,2500.f),R.FRandRange(-2500.f,2500.f),0),P)) continue;
  FTransform T(P+FVector(0,0,300)); auto* B=World->SpawnActorDeferred<ACultFeralBeast>(ACultFeralBeast::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn); B->SetSpecies(Species[I%4]); B->FinishSpawning(T);
 }
 UE_LOG(LogTemp,Log,TEXT("[CultUlhu] Living wilds: six creature territories, three caves, instanced forests and rock spires."));
}
