#include "CultMatchBackdrop.h"
#include "ProceduralMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HAL/IConsoleManager.h"

namespace {
TAutoConsoleVariable<int32> Backdrop(TEXT("cult.Match.Backdrop"),1,TEXT("Irregular exterior rock range. Zero spawns the legacy cone backdrop."));
constexpr int Sides=96,Rings=5;
const FName RangeTag(TEXT("CultMatchBackdropRange")),LegacyTag(TEXT("CultMatchBackdropLegacy")),MobaTag(TEXT("CultMatchBackdropMoba"));
UMaterialInstanceDynamic* Material(AActor* A,FLinearColor Color,bool Rock) {
 auto* Base=LoadObject<UMaterialInterface>(nullptr,Rock?TEXT("/Game/CultUlhu/Materials/M_MatchStone"):TEXT("/Game/CultUlhu/Materials/M_ArenaSurface"));
 auto* M=UMaterialInstanceDynamic::Create(Base,A); M->SetVectorParameterValue(TEXT("Color"),Color); M->SetScalarParameterValue(TEXT("Masonry"),0); return M;
}
void Legacy(UWorld* W,bool Moba) {
 for(int I=0;I<24;++I) {
  const float Angle=I*2*PI/24;
  auto* A=W->SpawnActor<AStaticMeshActor>(FVector(FMath::Cos(Angle)*(Moba?42000:34500),FMath::Sin(Angle)*(Moba?25000:32500),3500),FRotator::ZeroRotator);
  auto* C=A->GetStaticMeshComponent(); C->SetMobility(EComponentMobility::Movable); C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cone")));
  C->SetWorldScale3D(FVector(35,35,100)); C->SetMaterial(0,Material(A,FLinearColor(.08f,.11f,.14f),false)); C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  A->Tags.Append({TEXT("MatchArena"),LegacyTag}); if(Moba) A->Tags.Add(MobaTag);
 }
}
AActor* Range(UWorld* W,bool Moba) {
 auto* A=W->SpawnActor<AActor>(); auto* Mesh=NewObject<UProceduralMeshComponent>(A,TEXT("ExteriorRockRange"));
 A->SetRootComponent(Mesh); A->AddInstanceComponent(Mesh); Mesh->SetMobility(EComponentMobility::Movable); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->SetCastShadow(false); Mesh->RegisterComponent();
 A->Tags.Append({TEXT("MatchArena"),RangeTag}); if(Moba) A->Tags.Add(MobaTag);
 // All toes are outside even the playable rectangle's corners. No hidden
 // collision, navigation, scoring, lighting or player-space occluders are added.
 const FVector2D Radius=Moba?FVector2D(58000,40000):FVector2D(50000,48000);
 auto Point=[&](int Ring,int Side) {
  const float Angle=(Side%Sides)*2*PI/Sides;
  const float Crown=.5f+.24f*FMath::Sin(5*Angle+.8f)+.16f*FMath::Cos(11*Angle-.4f)+.10f*FMath::Sin(19*Angle);
  const float Heights[]={-1800,4200+6200*Crown,9500+15500*Crown,7000+10000*Crown,-1800};
  const float Extent[]={1.f,1.10f,1.24f,1.43f,1.64f};
  const float Break=Ring==0||Ring==4?0.f:.035f*FMath::Sin(7*Angle+Ring*.9f);
  // Offset intermediate strata around the ellipse to break radial fan faces.
  const float Offset=Ring==0||Ring==4?0.f:.013f*FMath::Sin(9*Angle+Ring);
  return FVector(FMath::Cos(Angle+Offset)*Radius.X*(Extent[Ring]+Break),FMath::Sin(Angle+Offset)*Radius.Y*(Extent[Ring]+Break),Heights[Ring]);
 };
 for(int Band=0;Band<Rings;++Band) {
  TArray<FVector> V,N; TArray<int32> T; TArray<FVector2D> UV; TArray<FLinearColor> Colors;
  auto Tri=[&](FVector X,FVector Y,FVector Z) {
   const int Index=V.Num(); const auto Normal=-FVector::CrossProduct(Y-X,Z-X).GetSafeNormal();
   V.Append({X,Y,Z}); N.Append({Normal,Normal,Normal}); T.Append({Index,Index+1,Index+2});
   for(const auto P:{X,Y,Z}) { UV.Add(FVector2D(P.X,P.Y)/2500); Colors.Add(FLinearColor::White); }
  };
  for(int I=0;I<Sides;++I) {
   const int Inner=Band==4?4:Band,Outer=Band==4?0:Band+1;
   const auto P=Point(Inner,I),Q=Point(Inner,I+1),R=Point(Outer,I),S=Point(Outer,I+1);
   Tri(P,S,R); Tri(P,Q,S);
  }
  Mesh->CreateMeshSection_LinearColor(Band,V,T,N,UV,Colors,TArray<FProcMeshTangent>(),false);
  const float Shade[]={.8f,1.f,.88f,.72f,.6f}; Mesh->SetMaterial(Band,Material(A,FLinearColor(.065f,.095f,.11f)*Shade[Band],true));
 }
 return A;
}
}
void CultMatchBackdrop::Build(UWorld* W,bool Moba) { if(Backdrop.GetValueOnGameThread()) Range(W,Moba); else Legacy(W,Moba); }
#if WITH_EDITOR
void CultMatchBackdrop::Compare(UWorld* W,bool Refined) {
 bool Moba=false,HasRange=false,HasLegacy=false;
 for(TActorIterator<AActor> It(W);It;++It) if(It->ActorHasTag(RangeTag)||It->ActorHasTag(LegacyTag)) {
  Moba|=It->ActorHasTag(MobaTag); HasRange|=It->ActorHasTag(RangeTag); HasLegacy|=It->ActorHasTag(LegacyTag);
 }
 if(Refined&&!HasRange) Range(W,Moba);
 if(!Refined&&!HasLegacy) Legacy(W,Moba);
 for(TActorIterator<AActor> It(W);It;++It) if(It->ActorHasTag(RangeTag)||It->ActorHasTag(LegacyTag)) {
  TArray<UMeshComponent*> Meshes; It->GetComponents(Meshes); for(auto* C:Meshes) C->SetVisibility(It->ActorHasTag(RangeTag)==Refined);
 }
}
#endif
