#include "CultBattlefieldLayout.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/PointLight.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace CultBattlefieldLayout {
void Build(UWorld* World,bool Moba) {
 auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube"));
 auto* Stone=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CultUlhu/Materials/M_ArenaSurface"));
 if(!Stone) Stone=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
 const FLinearColor Dark(.055f,.08f,.095f),Trim(.13f,.19f,.20f),Rune(.16f,.36f,.38f);
 auto Box=[&](FVector At,FVector Size,FLinearColor Color,bool Solid=true,const TCHAR* Tag=TEXT("CultExpansionCover")) {
  auto* Actor=World->SpawnActor<AStaticMeshActor>(At,FRotator::ZeroRotator);
  auto* Mesh=Actor->GetStaticMeshComponent(); Mesh->SetMobility(EComponentMobility::Movable); Mesh->SetStaticMesh(Cube);
  Mesh->SetWorldScale3D(Size/100); Mesh->SetCollisionProfileName(Solid?TEXT("BlockAll"):TEXT("NoCollision"));
  Mesh->SetCollisionEnabled(Solid?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
  Actor->SetActorEnableCollision(Solid);
  auto* Material=UMaterialInstanceDynamic::Create(Stone,Actor); Material->SetVectorParameterValue(TEXT("Color"),Color); Mesh->SetMaterial(0,Material);
  Actor->Tags.Append({TEXT("MatchArena"),TEXT("CultBattlefieldExpansion"),TEXT("CultMatchStoneSurface"),Tag});
  return Actor;
 };
 const auto Extent=HalfExtent(Moba);
 // Actual map edges move with the ground, not just the background art.
 if(Moba) for(int Side:{-1,1}) {
  Box(FVector(Side*Extent.X,0,8000),FVector(1600,Extent.Y*2,16000),Dark,true,TEXT("CultExpansionBoundary"));
  Box(FVector(0,Side*Extent.Y,8000),FVector(Extent.X*2,1600,16000),Dark,true,TEXT("CultExpansionBoundary"));
 }
 const float PassageY=OuterPassageY(Moba),Length=Moba?54000.f:66000.f;
 for(int Side:{-1,1}) {
  // Two-ended, 76m-wide vaults, with three side entrances in each wall.
  // 65m clear headroom admits the entire creature roster.
  auto* Roof=Box(FVector(0,Side*PassageY,6750),FVector(Length,7600,500),Dark,true,TEXT("CultExpansionRoof"));
  Roof->Tags.Add(TEXT("CultMatchWalkSurface"));
  for(int Edge:{-1,1}) for(int Segment=0;Segment<11;++Segment) {
   if(Segment==2||Segment==5||Segment==8) continue;
   const float Span=Length/11, X=-Length*.5f+(Segment+.5f)*Span;
   Box(FVector(X,Side*PassageY+Edge*4100,3250),FVector(Span,600,6500),Dark,true,TEXT("CultExpansionVaultWall"));
   Box(FVector(X,Side*PassageY+Edge*3760,1400),FVector(Span*.55f,45,90),Rune,false);
   Box(FVector(X,Side*PassageY+Edge*3500,7400),FVector(Span*.40f,650,800),Trim);
  }
  // Readable portal piers and broken masonry; the central passage stays open.
  for(int End:{-1,1}) for(int Edge:{-1,1})
   Box(FVector(End*(Length*.5f-350),Side*PassageY+Edge*3600,3250),FVector(700,500,6500),Trim);
  for(int K=-2;K<=2;++K) {
   Box(FVector(K*9500+4000,Side*PassageY+(K%2?1:-1)*2400,350),FVector(1900,1100,700),Trim,true,TEXT("CultExpansionVaultObstacle"));
   // Low blocks can be jumped; tall fragments break sight and stop projectiles.
   const FVector Court(K*10000+4000,Side*(Moba?32500.f:43000.f),0);
   Box(Court+FVector(0,0,500),FVector(2400,1400,1000),Trim);
   auto* Fragment=Box(Court+FVector(2700,-Side*2300,2100),FVector(1500,1700,4200),Dark);
   Fragment->SetActorRotation(FRotator(0,K*13+Side*9,0));
  }
  for(int Lamp=-2;Lamp<=2;++Lamp) {
   auto* Light=World->SpawnActor<APointLight>(FVector(Lamp*10500,Side*PassageY,4800),FRotator::ZeroRotator);
   auto* Component=Light->PointLightComponent.Get(); Component->SetMobility(EComponentMobility::Movable);
   Component->SetIntensity(90000); Component->SetAttenuationRadius(10500); Component->SetCastShadows(false);
   Component->SetLightColor(Lamp%2?FLinearColor(.24f,.55f,.64f):FLinearColor(.64f,.44f,.26f));
   Light->Tags.Append({TEXT("MatchArena"),TEXT("CultBattlefieldExpansion"),TEXT("CultExpansionLight")});
  }
  // Exterior stair-rubble reaches the deck without entering the low ceiling.
  for(int Home:{-1,1}) {
   const float X=Home*(Moba?24000.f:18000.f);
   const float InnerEdge=PassageY-3800;
   const FVector Start(X,Side*(InnerEdge-11500),0),End(X,Side*(InnerEdge-1000),7000),Delta=End-Start;
   const FVector Normal=FVector(0,-Delta.Z*Delta.Y,Delta.SizeSquared2D()).GetSafeNormal();
   auto* Ramp=Box((Start+End)*.5f-Normal*150,FVector(Delta.Size(),6000,300),Dark,true,TEXT("CultExpansionRamp"));
   Ramp->SetActorRotation(Delta.Rotation()); Ramp->Tags.Add(TEXT("CultMatchWalkSurface"));
   auto* Landing=Box(FVector(X,Side*(InnerEdge-150),6750),FVector(6000,2100,500),Dark,true,TEXT("CultExpansionRoof"));
   Landing->Tags.Add(TEXT("CultMatchWalkSurface"));
  }
  // Separate side courts, with open broken arches instead of a cover grid.
  for(int Home:{-1,1}) {
   const FVector Court(Home*(Moba?44000.f:40000.f),Side*(Moba?18000.f:17000.f),0);
   for(int Edge:{-1,1}) Box(Court+FVector(0,Edge*3400,2800),FVector(1600,1000,5600),Dark);
   Box(Court+FVector(0,0,5900),FVector(1600,7800,600),Trim);
   for(int I=0;I<3;++I) {
    auto* Rubble=Box(Court+FVector(Home*(3600+I*2500),Side*(I%2?2000:-2000),450+I*140),FVector(1800,1600,900+I*280),Trim);
    Rubble->SetActorRotation(FRotator(0,Home*(18+I*23),0));
   }
  }
 }
 UE_LOG(LogTemp,Display,TEXT("[CultBattlefield] %s expanded to %.0fm x %.0fm with roof routes, covered vaults, side portals and jump cover"),Moba?TEXT("DOTGOO"):TEXT("Domination"),Extent.X*.02,Extent.Y*.02);
}
}
