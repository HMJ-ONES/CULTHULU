#include "CultJungleCliffs.h"
#include "CultMobaJungle.h"
#include "ProceduralMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"

namespace {
constexpr int Sides=36,Rings=5;
const float Heights[Rings]={0,.14f,.43f,.76f,1};
const float Widths[Rings]={1.06f,1.01f,.90f,.98f,1.f};
float Radius(float Angle,int Camp,int Ring) {
 // Bilateral functions preserve the left/right camps' mirrored geometry.
 // Both ramp approaches retain the original ellipse and a broad landing collar.
 const float Gate=FMath::SmoothStep(.4f,.8f,FMath::Abs(FMath::Cos(Angle)));
 const float Shape=.5f+.25f*FMath::Cos(6*Angle)+.25f*FMath::Sin(3*Angle);
 const float Break=Ring==Rings-1?0.f:.018f*FMath::Cos(10*Angle)*(Camp/2?1.f:-1.f);
 // Keep ledges beneath the four existing crown-rock groups, with larger
 // eroded insets between them rather than an almost circular perimeter.
 const float RockPocket=1-FMath::SmoothStep(.10f,.22f,FMath::Abs(FMath::Abs(FMath::Sin(Angle))-.54f));
 const float Contour=FMath::Max(1-.25f*Gate*Shape+Break*Gate,.95f*RockPocket);
 return Widths[Ring]*Contour;
}
UMaterialInstanceDynamic* Material(AActor* Owner,FLinearColor Color) {
 auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CultUlhu/Materials/M_MatchStone"));
 if(!Base) Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CultUlhu/Materials/M_ArenaSurface"));
 auto* M=UMaterialInstanceDynamic::Create(Base,Owner); M->SetVectorParameterValue(TEXT("Color"),Color); M->SetScalarParameterValue(TEXT("Masonry"),0); return M;
}
}
AActor* CultJungleCliffs::Build(UWorld* World,int Camp) {
 const double Began=FPlatformTime::Seconds();
 const auto Ground=FCultMobaJungle::Ground(Camp); auto* Actor=World->SpawnActor<AActor>();
 auto* Mesh=NewObject<UProceduralMeshComponent>(Actor,TEXT("JungleCliff")); Actor->SetRootComponent(Mesh);
 Mesh->SetMobility(EComponentMobility::Movable); Mesh->SetCollisionProfileName(TEXT("BlockAll")); Mesh->SetCollisionObjectType(ECC_WorldStatic);
 Mesh->bUseComplexAsSimpleCollision=true; Mesh->bUseAsyncCooking=false; Mesh->RegisterComponent(); Actor->SetActorLocation(FVector(Ground.X,Ground.Y,0));
 Actor->Tags.Append({TEXT("MatchArena"),TEXT("CultMobaJungleMesa"),TEXT("CultMatchWalkSurface"),TEXT("CultMatchStoneSurface"),TEXT("CultMatchRockSurface")});
 auto Point=[&](int Ring,int Side) {
  const float Angle=(Side%Sides)*2*PI/Sides,R=Radius(Angle,Camp,Ring);
  const float Break=Ring==0||Ring==Rings-1?0.f:65.f*FMath::Cos(4*Angle)*(Camp/2?1.f:-1.f);
  return FVector(FMath::Cos(Angle)*3500*R,FMath::Sin(Angle)*2500*R,Heights[Ring]*Ground.Z+Break);
 };
 for(int Section=0;Section<Rings;++Section) {
  TArray<FVector> V,N; TArray<int32> T; TArray<FVector2D> UV; TArray<FLinearColor> Colors;
  auto Triangle=[&](FVector A,FVector B,FVector C) {
   const auto Normal=-FVector::CrossProduct(B-A,C-A).GetSafeNormal(); const int I=V.Num();
   V.Append({A,B,C}); N.Append({Normal,Normal,Normal}); T.Append({I,I+1,I+2});
   for(auto P:{A,B,C}) { UV.Add(FVector2D(P.X,P.Y)/1000); Colors.Add(FLinearColor::White); }
  };
  for(int I=0;I<Sides;++I) {
   if(Section==0) { Triangle(FVector(0,0,Ground.Z),Point(Rings-1,I+1),Point(Rings-1,I)); Triangle(FVector::ZeroVector,Point(0,I),Point(0,I+1)); }
   else {
    const auto A=Point(Section,I),B=Point(Section,I+1),C=Point(Section-1,I),D=Point(Section-1,I+1);
    Triangle(A,D,C); Triangle(A,B,D);
   }
  }
  // The top and four cliff bands share exact vertices for watertight collision.
  Mesh->CreateMeshSection_LinearColor(Section,V,T,N,UV,Colors,TArray<FProcMeshTangent>(),true);
  const float Shade[]={1.06f,.72f,.94f,.82f,1.f}; Mesh->SetMaterial(Section,Material(Actor,FLinearColor(.075f,.12f,.105f)*Shade[Section]));
 }
 UE_LOG(LogTemp,Log,TEXT("[CultJungleCliffs] camp%d: 360 rendered/collision triangles, synchronous generation/cook %.2fms"),Camp,(FPlatformTime::Seconds()-Began)*1000);
 return Actor;
}

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
int CultJungleCliffs::LegacyComparison(UWorld* World,bool Legacy) {
 int Count=0;
 for(TActorIterator<AActor> It(World);It;++It) if(It->ActorHasTag(TEXT("CultMobaJungleMesa"))) {
  auto* Cliff=It->FindComponentByClass<UProceduralMeshComponent>(); if(!Cliff) continue;
  auto* Cylinder=It->FindComponentByClass<UStaticMeshComponent>();
  if(!Cylinder) {
   Cylinder=NewObject<UStaticMeshComponent>(*It,TEXT("LegacyCylinderComparison")); Cylinder->SetupAttachment(Cliff);
   Cylinder->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder"))); Cylinder->SetMobility(EComponentMobility::Movable);
   const float Height=Cliff->GetProcMeshSection(0)->ProcVertexBuffer[0].Position.Z;
   Cylinder->SetRelativeLocation(FVector(0,0,Height*.5f)); Cylinder->SetRelativeScale3D(FVector(70,50,Height/100));
   Cylinder->SetMaterial(0,Material(*It,FLinearColor(.075f,.12f,.105f))); Cylinder->RegisterComponent();
  }
  Cliff->SetVisibility(!Legacy,false); Cliff->SetCollisionEnabled(Legacy?ECollisionEnabled::NoCollision:ECollisionEnabled::QueryAndPhysics);
  Cylinder->SetVisibility(Legacy); Cylinder->SetCollisionProfileName(TEXT("BlockAll")); Cylinder->SetCollisionEnabled(Legacy?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
  ++Count;
 }
 return Count;
}
#endif
