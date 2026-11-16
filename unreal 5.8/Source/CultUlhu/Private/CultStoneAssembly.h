#pragma once
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "ProceduralMeshComponent.h"
#include "KismetProceduralMeshLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "CultMatchSurfaces.h"

// Shared merged primitives for original arena architectural details.
namespace CultStoneAssembly {
struct FSection {
 TArray<FVector> V,N; TArray<int32> I; TArray<FVector2D> UV; TArray<FProcMeshTangent> Tangents;
 void Box(FVector Center,FVector Half,FRotator Rotation=FRotator::ZeroRotator,float TopScale=1) {
  TArray<FVector> Vertices,Normals; TArray<int32> Indices; TArray<FVector2D> Tex; TArray<FProcMeshTangent> T;
  UKismetProceduralMeshLibrary::GenerateBoxMesh(Half,Vertices,Indices,Normals,Tex,T);
  if(TopScale!=1) { for(auto& P:Vertices) if(P.Z>0) { P.X*=TopScale; P.Y*=TopScale; } UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Vertices,Indices,Tex,Normals,T); }
  const int Base=V.Num(); const FQuat Q=Rotation.Quaternion();
  for(int K=0;K<Vertices.Num();++K) { V.Add(Center+Q.RotateVector(Vertices[K])); N.Add(Q.RotateVector(Normals[K])); UV.Add(Tex[K]); Tangents.Add(FProcMeshTangent(Q.RotateVector(T[K].TangentX),T[K].bFlipTangentY)); }
  for(int Index:Indices) I.Add(Base+Index);
 }
 void Beam(FVector A,FVector B,float Width,float Depth) { Box((A+B)*.5f,FVector(FVector::Dist(A,B)*.5f,Depth*.5f,Width*.5f),FRotationMatrix::MakeFromX(B-A).Rotator()); }
};
struct FAssembly {
 FSection Parts[3];
 FName Tag=TEXT("CultDominationArt"); bool Metal=false;
 AActor* Finish(UWorld* W,FVector Origin,FLinearColor Stone,bool Solid,const FName& Kind) {
  auto* A=W->SpawnActor<AActor>(Origin,FRotator::ZeroRotator);
  auto* Mesh=NewObject<UProceduralMeshComponent>(A); A->SetRootComponent(Mesh); A->AddInstanceComponent(Mesh);
  Mesh->SetMobility(EComponentMobility::Movable); Mesh->SetCollisionProfileName(Solid?TEXT("BlockAll"):TEXT("NoCollision")); Mesh->SetCollisionObjectType(ECC_WorldStatic);
  Mesh->bUseComplexAsSimpleCollision=true; Mesh->bUseAsyncCooking=false; Mesh->SetCastShadow(true); Mesh->RegisterComponent(); A->SetActorLocation(Origin);
  A->SetActorEnableCollision(Solid); A->Tags.Append({TEXT("MatchArena"),Tag,Kind});
  for(int S=0;S<3;++S) if(Parts[S].V.Num()) {
   auto& P=Parts[S]; Mesh->CreateMeshSection_LinearColor(S,P.V,P.I,P.N,P.UV,TArray<FLinearColor>(),P.Tangents,Solid);
   auto* Base=S==2&&!Metal?LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CultUlhu/Materials/M_AttackSignal")):CultMatchSurfaces::StoneMaterial(true);
   auto* M=UMaterialInstanceDynamic::Create(Base,A); M->SetVectorParameterValue(TEXT("Color"),S==0?Stone:S==1?FLinearColor(.29f,.34f,.32f):(Metal?FLinearColor(.34f,.27f,.15f):FLinearColor(.58f,.57f,.43f))); M->SetScalarParameterValue(TEXT("Masonry"),S==0?1.f:0.f); M->SetScalarParameterValue(TEXT("SurfaceStyle"),S==0?0.f:S==1?1.f:2.f); M->SetScalarParameterValue(TEXT("Intensity"),.28f); Mesh->SetMaterial(S,M);
  }
  return A;
 }
};
}
