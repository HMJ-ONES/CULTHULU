#include "CultDominationFinish.h"
#include "CultStoneAssembly.h"
#include "CultDominationRoute.h"
#include "EngineUtils.h"
#include "Components/MeshComponent.h"
#include "HAL/IConsoleManager.h"
using namespace CultStoneAssembly;
namespace {
const FName FinishTag(TEXT("CultDominationRefinement"));
void Crest(FSection& S,FVector C,int Side,float Scale=1) {
 auto P=[&](float X,float Z) { return C+FVector(X*Scale,0,Z*Scale); };
 if(Side>0) {
  S.Beam(P(-550,0),P(550,0),45,30);
  for(int K=-1;K<=1;++K) S.Beam(P(K*450,0),P(K*450,K?550:850),45,30);
 } else {
  const FVector V[]={P(-550,350),P(0,850),P(550,350),P(0,-150)};
  for(int K=0;K<4;++K) S.Beam(V[K],V[(K+1)%4],45,30);
 }
}
void Present(UWorld* W,bool Refined) {
 for(TActorIterator<AActor> It(W);It;++It) {
  if(It->ActorHasTag(FinishTag)) It->SetActorHiddenInGame(!Refined);
  if(It->ActorHasTag(TEXT("CultDominationKeepSkin"))) It->SetActorHiddenInGame(Refined);
 }
}
}
void CultDominationFinish::Build(UWorld* W) {
 if(!W) return;
 for(TActorIterator<AActor> It(W);It;++It) if(It->ActorHasTag(FinishTag)) return;
 FAssembly A; A.Tag=FinishTag; A.Metal=true;
 for(int Side:{-1,1}) for(int Home:{-1,1}) {
  const FVector Keep(Home*10500,Side*12500,0);
  for(int Edge:{-1,1}) {
   const float Y=Keep.Y+Edge*4020;
   // Closed, raised stone panels: these never pretend to be open passages.
   A.Parts[1].Box(FVector(Keep.X,Y,300),FVector(4950,45,110));
   A.Parts[1].Box(FVector(Keep.X,Y,4720),FVector(4950,45,90));
   for(int K=-1;K<=1;++K) {
    const float X=Keep.X+K*3100;
    A.Parts[0].Box(FVector(X,Y,2480),FVector(130,45,2100),FRotator::ZeroRotator,.55f);
    for(int E:{-1,1}) {
     A.Parts[1].Beam(FVector(X+E*1000,Y,1100),FVector(X+E*1000,Y,3100),75,50);
     A.Parts[1].Beam(FVector(X+E*1000,Y,3100),FVector(X,Y,4200),75,50);
    }
    A.Parts[1].Beam(FVector(X-1000,Y,1100),FVector(X+1000,Y,1100),75,50);
    Crest(A.Parts[2],FVector(X,Y+Edge*35,2200),Side,.8f);
   }
  }
  // The real crypt mouth stays open. Carved header ribs remain on the slab.
  for(int End:{-1,1}) {
   const float X=Keep.X+End*5015;
   for(int E:{-1,1}) {
    A.Parts[1].Beam(FVector(X,Keep.Y+E*2350,5160),FVector(X,Keep.Y+E*1200,5540),110,45);
    A.Parts[1].Beam(FVector(X,Keep.Y+E*1200,5540),FVector(X,Keep.Y,5900),110,45);
   }
   A.Parts[2].Box(FVector(X+End*20,Keep.Y,5640),FVector(25,110,240),FRotator::ZeroRotator,.55f);
  }
  const FVector Wall(Home*4300,Side*5830,0);
  A.Parts[1].Box(Wall+FVector(0,0,180),FVector(1660,30,90));
  A.Parts[1].Box(Wall+FVector(0,0,3400),FVector(1660,30,90));
  for(int E:{-1,1}) A.Parts[0].Box(Wall+FVector(E*1450,0,1740),FVector(140,40,1400),FRotator::ZeroRotator,.5f);
  Crest(A.Parts[2],Wall+FVector(0,-Side*30,1700),Side,1.1f);
  const FVector Perch=FCultDominationRoute::Perch(Home,Side);
  A.Parts[1].Box(Perch+FVector(0,-Side*850,8),FVector(1160,45,8));
  A.Parts[2].Box(Perch+FVector(0,-Side*805,12),FVector(1050,12,6));
  const FVector Start=FCultDominationRoute::PerchRamp(Home,Side),End(Home*4500,Side*5000,900),D=(End-Start).GetSafeNormal();
  const FVector N=FVector(-D.Z*D.X,0,D.SizeSquared2D()).GetSafeNormal();
  for(float T:{.24f,.55f}) {
   const FVector C=FMath::Lerp(Start,End,T)+N*15,Across=FVector::RightVector;
   A.Parts[2].Beam(C-D*180-Across*320,C+D*180,12,40);
   A.Parts[2].Beam(C+D*180,C-D*180+Across*320,12,40);
  }
 }
 // Calm, non-emissive central inlay; the rectangular border remains the rule.
 for(float R:{1750.f,2700.f}) for(int K=0;K<48;++K) {
  const float T=K*2*PI/48,U=(K+1)*2*PI/48;
  A.Parts[2].Beam(FVector(FMath::Cos(T)*R,FMath::Sin(T)*R,14),FVector(FMath::Cos(U)*R,FMath::Sin(U)*R,14),8,70);
 }
 for(int K=0;K<8;++K) {
  const float T=K*PI/4; const FVector D(FMath::Cos(T),FMath::Sin(T),0),S(-D.Y,D.X,0),C=D*2150+FVector(0,0,14);
  A.Parts[2].Beam(C-D*200-S*160,C+D*200,8,45); A.Parts[2].Beam(C+D*200,C-D*200+S*160,8,45);
 }
 auto* Actor=A.Finish(W,FVector::ZeroVector,FLinearColor(.115f,.155f,.17f),false,FinishTag);
 Actor->FindComponentByClass<UProceduralMeshComponent>()->SetCastShadow(false);
 Present(W,CultMatchSurfaces::RefinedDomination());
}
#if WITH_EDITOR
int CultDominationFinish::Compare(UWorld* W,bool Refined) {
 IConsoleManager::Get().FindConsoleVariable(TEXT("cult.Domination.Refinement"))->Set(Refined?1:0,ECVF_SetByCode);
 const int Count=CultMatchSurfaces::Apply(W,true,true);
 for(TActorIterator<AActor> It(W);It;++It) if(It->ActorHasTag(TEXT("CultDominationArt"))||It->ActorHasTag(FinishTag)) {
  TInlineComponentArray<UMeshComponent*> Meshes(*It);
  for(auto* Mesh:Meshes) for(int Slot=0;Slot<(It->ActorHasTag(FinishTag)?3:2);++Slot) if(auto* Old=Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(Slot))) {
   auto* Mat=UMaterialInstanceDynamic::Create(CultMatchSurfaces::StoneMaterial(true),*It);
   Mat->SetVectorParameterValue(TEXT("Color"),Old->K2_GetVectorParameterValue(TEXT("Color"))); Mat->SetScalarParameterValue(TEXT("Masonry"),Slot==0?1.f:0.f); Mat->SetScalarParameterValue(TEXT("SurfaceStyle"),float(Slot)); Mesh->SetMaterial(Slot,Mat);
  }
 }
 Present(W,Refined); return Count;
}
#endif
