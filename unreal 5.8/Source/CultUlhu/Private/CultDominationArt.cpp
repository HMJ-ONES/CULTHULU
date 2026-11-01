#include "CultDominationArt.h"
#include "CultDominationRoute.h"
#include "ProceduralMeshComponent.h"
#include "KismetProceduralMeshLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

namespace {
TAutoConsoleVariable<int32> Art(TEXT("cult.Domination.Art"),1,TEXT("Original Crown/Hollow architecture and route inlays. Zero omits decoration, retaining gameplay overlooks."));
const FName ArtTag(TEXT("CultDominationArt"));
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
 AActor* Finish(UWorld* W,FVector Origin,FLinearColor Stone,bool Solid,const FName& Kind) {
  auto* A=W->SpawnActor<AActor>(Origin,FRotator::ZeroRotator);
  auto* Mesh=NewObject<UProceduralMeshComponent>(A); A->SetRootComponent(Mesh); A->AddInstanceComponent(Mesh);
  Mesh->SetMobility(EComponentMobility::Movable); Mesh->SetCollisionProfileName(Solid?TEXT("BlockAll"):TEXT("NoCollision")); Mesh->SetCollisionObjectType(ECC_WorldStatic);
  Mesh->bUseComplexAsSimpleCollision=true; Mesh->bUseAsyncCooking=false; Mesh->SetCastShadow(true); Mesh->RegisterComponent(); A->SetActorLocation(Origin);
  A->SetActorEnableCollision(Solid); A->Tags.Append({TEXT("MatchArena"),ArtTag,Kind});
  for(int S=0;S<3;++S) if(Parts[S].V.Num()) {
   auto& P=Parts[S]; Mesh->CreateMeshSection_LinearColor(S,P.V,P.I,P.N,P.UV,TArray<FLinearColor>(),P.Tangents,Solid);
   auto* Base=LoadObject<UMaterialInterface>(nullptr,S==2?TEXT("/Game/CultUlhu/Materials/M_AttackSignal"):TEXT("/Game/CultUlhu/Materials/M_MatchStone"));
   auto* M=UMaterialInstanceDynamic::Create(Base,A); M->SetVectorParameterValue(TEXT("Color"),S==0?Stone:S==1?FLinearColor(.29f,.34f,.32f):FLinearColor(.58f,.57f,.43f)); M->SetScalarParameterValue(TEXT("Masonry"),S==0?1.f:0.f); M->SetScalarParameterValue(TEXT("Intensity"),.28f); Mesh->SetMaterial(S,M);
  }
  return A;
 }
};
void Chevron(FSection& S,FVector Center,FVector Forward,FVector Across) {
 S.Beam(Center-Forward*380-Across*550,Center+Forward*380,65,65);
 S.Beam(Center+Forward*380,Center-Forward*380+Across*550,65,65);
}
}
void CultDominationArt::Build(UWorld* W) {
 if(!W||!Art.GetValueOnGameThread()) return;
 for(TActorIterator<AActor> It(W);It;++It) if(It->ActorHasTag(ArtTag)) return;
 for(int Side:{-1,1}) {
  const bool Crown=Side>0; const FLinearColor Stone=Crown?FLinearColor(.10f,.16f,.19f):FLinearColor(.17f,.13f,.17f);
  // Solid skyline structures sit on the existing perimeter. They do not add
  // obstacles to the keeps, outer ramps or full-height underground paths.
  FAssembly Landmark;
  for(int X:{-1,1}) {
   Landmark.Parts[0].Box(FVector(X*5100,0,Crown?11600:10400),FVector(850,700,Crown?3000:1800));
   Landmark.Parts[1].Box(FVector(X*5100,-Side*30,Crown?14600:12200),FVector(1000,780,170));
  }
  if(Crown) {
   for(int K=0;K<14;++K) {
    const float A=FMath::DegreesToRadians(14.f+K*152.f/14),B=FMath::DegreesToRadians(14.f+(K+1)*152.f/14);
    const FVector P(FMath::Cos(A)*5500,0,13500+FMath::Sin(A)*5500),Q(FMath::Cos(B)*5500,0,13500+FMath::Sin(B)*5500);
    Landmark.Parts[0].Beam(P,Q,850,1100); Landmark.Parts[1].Beam(P+FVector(0,-Side*580,0),Q+FVector(0,-Side*580,0),130,130);
   }
   for(int K=-1;K<=1;++K) {
    const float X=K*2900,Z=K?18500:20800;
    Landmark.Parts[0].Beam(FVector(X,0,Z-1600),FVector(X+K*500,0,Z+1700),720,1000);
    Landmark.Parts[1].Box(FVector(X+K*500,0,Z+2000),FVector(480,600,500),FRotator::ZeroRotator,.05f);
    Landmark.Parts[2].Beam(FVector(X,-Side*520,Z-800),FVector(X+K*300,-Side*520,Z+900),90,90);
   }
  } else {
   for(int K=-2;K<=2;++K) {
    const FVector A(K*2500,0,12000),B(K*1700,0,16700+(2-FMath::Abs(K))*1100),C(K*900,0,21100-FMath::Abs(K)*1000);
    Landmark.Parts[0].Beam(A,B,650,1050); Landmark.Parts[0].Beam(B,C,500,900);
    Landmark.Parts[1].Box(C+FVector(0,0,250),FVector(430,520,500),FRotator::ZeroRotator,.05f);
    Landmark.Parts[1].Beam(A+FVector(0,-Side*570,0),B+FVector(0,-Side*570,0),110,110);
   }
   Landmark.Parts[2].Beam(FVector(-1500,-Side*600,15800),FVector(0,-Side*600,14800),100,100);
   Landmark.Parts[2].Beam(FVector(0,-Side*600,14800),FVector(1500,-Side*600,15800),100,100);
   Landmark.Parts[2].Beam(FVector(-1500,-Side*600,15800),FVector(0,-Side*600,16800),100,100);
   Landmark.Parts[2].Beam(FVector(0,-Side*600,16800),FVector(1500,-Side*600,15800),100,100);
  }
  Landmark.Finish(W,FVector(0,Side*30000,0),Stone,true,Crown?TEXT("CultCrownLandmark"):TEXT("CultHollowLandmark"));
  for(int Team:{-1,1}) {
   FAssembly Keep;
   // Shallow stone ribs/fascias dress existing solid walls and roof edges.
   // The lower passage and the entire original roof floor remain open.
   for(int Edge:{-1,1}) {
    Keep.Parts[1].Box(FVector(Edge*4930,0,5500),FVector(90,4000,400));
    Keep.Parts[1].Box(FVector(0,Edge*3930,5500),FVector(5000,90,400));
    for(int K=-2;K<=2;++K) Keep.Parts[0].Box(FVector(K*2100,Edge*3995,2600),FVector(180,30,2400));
    if(Crown) {
     Keep.Parts[2].Beam(FVector(-1000,Edge*4015,2300),FVector(1000,Edge*4015,2300),75,40);
     for(int K=-1;K<=1;++K) Keep.Parts[2].Beam(FVector(K*700,Edge*4015,2300),FVector(K*700,Edge*4015,K?3200:3700),75,40);
    } else {
     const FVector A(-900,Edge*4015,2800),B(0,Edge*4015,3500),C(900,Edge*4015,2800),D(0,Edge*4015,2100);
     for(auto Pair:{TPair<FVector,FVector>(A,B),TPair<FVector,FVector>(B,C),TPair<FVector,FVector>(C,D),TPair<FVector,FVector>(D,A)}) Keep.Parts[2].Beam(Pair.Key,Pair.Value,75,40);
    }
   }
   for(int End:{-1,1}) for(int K=0;K<6;++K) {
    const float Y=-2500+K*1000,Z=5270+600*(1-FMath::Abs(Y)/3000);
    Keep.Parts[1].Box(FVector(End*4990,Y,Z),FVector(30,430,140),FRotator(0,0,Y>0?12:-12));
   }
   Keep.Finish(W,FVector(Team*10500,Side*12500,0),Stone,false,TEXT("CultDominationKeepSkin"));
  }
 }
 FAssembly Inlays;
 for(int Side:{-1,1}) for(int Team:{-1,1}) {
  const FVector Start(Team*10500,Side*26500,0),End(Team*10500,Side*16500,6000),D=(End-Start).GetSafeNormal();
  const FVector Normal=FVector(0,Side*.6f,1).GetSafeNormal();
  for(float T:{.15f,.28f,.41f}) Chevron(Inlays.Parts[2],FMath::Lerp(Start,End,T)+Normal*20,D,FVector::ForwardVector);
  const FVector Bottom(Team*FCultDominationRoute::JunctionX,Side*FCultDominationRoute::BranchY,FCultDominationRoute::Depth),Top(Team*FCultDominationRoute::CrestX,Side*FCultDominationRoute::BranchY,0);
  const FVector Down=(Bottom-Top).GetSafeNormal(),Up=FVector(-Team*5000.f,0,9500.f).GetSafeNormal();
  for(float T:{.08f,.22f}) Chevron(Inlays.Parts[2],FMath::Lerp(Top,Bottom,T)+Up*25,Down,FVector::RightVector);
 }
 auto* Guides=Inlays.Finish(W,FVector::ZeroVector,FLinearColor(.12f,.17f,.18f),false,TEXT("CultDominationRouteInlays"));
 Guides->FindComponentByClass<UProceduralMeshComponent>()->SetCastShadow(false);
}
FString CultDominationArt::RouteHint(FVector P) {
 const TCHAR* Keep=P.Y>=0?TEXT("CROWN"):TEXT("HOLLOW");
 if(P.Z<-800) return TEXT("CATACOMB | Keep exits return to the fight | Resurface to capture");
 if(P.Z>4800&&FMath::Abs(P.Y)>6200&&FMath::Abs(P.Y)<10800&&FMath::Abs(P.X)<3200) return TEXT("SEAL OVERLOOK | Cross above cover | Drop down to contest");
 if(P.Z>650&&P.Z<1500&&FMath::Abs(P.Y)>4100&&FMath::Abs(P.Y)<5900&&FMath::Abs(P.X)>2100&&FMath::Abs(P.X)<4500) return TEXT("FIRING PERCH | Aim at seal edges | Leave height to capture");
 if(P.Z>4800&&FMath::Abs(P.Y)>9000) return FString::Printf(TEXT("%s HIGH WALK | Cross the bridge to the seal overlook"),Keep);
 if(FMath::Abs(P.Y)>21000&&FMath::Abs(P.X)>7200&&FMath::Abs(P.X)<14000) return FString::Printf(TEXT("%s RAMP | Climb to the high walk and seal overlook"),Keep);
 if(FMath::Abs(P.X)>13000&&FMath::Abs(P.X)<17500&&FMath::Abs(FMath::Abs(P.Y)-12500)<2500) return TEXT("CRYPT ENTRANCE | Sheltered flank through the opposite keep");
 return FString();
}
#if WITH_EDITOR
void CultDominationArt::Compare(UWorld* W,bool Refined) { for(TActorIterator<AActor> It(W);It;++It) if(It->ActorHasTag(ArtTag)) It->SetActorHiddenInGame(!Refined); }
#endif
