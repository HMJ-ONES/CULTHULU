#include "CultMatchDirector.h"
#include "CultMobaRoute.h"
#include "CultMobaJungle.h"
#include "CultDominationRoute.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/PointLight.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/TextRenderComponent.h"
#include "EngineUtils.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/SkyLightComponent.h"
#include "Components/LightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"

void ACultMatchDirector::BuildArena(UWorld* W,ECultPlayMode Mode) {
 const bool Moba=Mode==ECultPlayMode::Dotgoo;
 // A stable exposure keeps dark stone dark instead of lifting every courtyard
 // to white. Scope this presentation to matches, preserving the authored level.
 auto* Grade=W->SpawnActor<APostProcessVolume>(); Grade->bUnbound=true; Grade->Priority=100;
 auto& Settings=Grade->Settings;
 Settings.bOverride_AutoExposureMethod=true; Settings.AutoExposureMethod=AEM_Manual;
 Settings.bOverride_AutoExposureApplyPhysicalCameraExposure=true; Settings.AutoExposureApplyPhysicalCameraExposure=false;
 Settings.bOverride_AutoExposureBias=true; Settings.AutoExposureBias=.75f;
 Settings.bOverride_BloomIntensity=true; Settings.BloomIntensity=.25f;
 Settings.bOverride_VignetteIntensity=true; Settings.VignetteIntensity=.18f;
 bool Moon=false;
 for(TActorIterator<ADirectionalLight> It(W);It;++It) {
  auto* Light=It->GetLightComponent(); Light->SetIntensity(Moon?0.f:2.5f);
  Light->SetLightColor(FLinearColor(.65f,.78f,.85f)); Moon=true;
 }
 for(TActorIterator<ASkyLight> It(W);It;++It) It->GetLightComponent()->SetIntensity(.6f);
 for(TActorIterator<ASkyAtmosphere> It(W);It;++It) It->GetComponent()->SetVisibility(false);
 bool Fog=false;
 auto SetFog=[](UExponentialHeightFogComponent* C) {
  C->SetFogDensity(.004f); C->SetFogHeightFalloff(.04f); C->SetStartDistance(14000);
  C->SetFogInscatteringColor(FLinearColor(.022f,.038f,.043f));
 };
 for(TActorIterator<AExponentialHeightFog> It(W);It;++It) { SetFog(It->GetComponent()); Fog=true; }
 if(!Fog) SetFog(W->SpawnActor<AExponentialHeightFog>()->GetComponent());
 auto Mesh=[&](const TCHAR* Shape,FVector Position,FVector Scale,FLinearColor Color,bool Collision) {
  auto* A=W->SpawnActor<AStaticMeshActor>(Position,FRotator::ZeroRotator); auto* C=A->GetStaticMeshComponent(); C->SetMobility(EComponentMobility::Movable);
  C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,Shape)); C->SetWorldScale3D(Scale);
  auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CultUlhu/Materials/M_ArenaSurface"));
  if(!Base) Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
  auto* Material=UMaterialInstanceDynamic::Create(Base,A); Material->SetVectorParameterValue(TEXT("Color"),Color); C->SetMaterial(0,Material);
  C->SetCollisionEnabled(Collision?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision); if(Collision) C->SetCollisionProfileName(TEXT("BlockAll")); A->Tags.Add(TEXT("MatchArena")); return A;
 };
 // Domination's two entrances are actual holes, rather than ramps buried
 // beneath a single collision plane. Shared tile edges avoid cracks.
 auto FloorTile=[&](FVector Center,FVector Size) {
  auto* A=Mesh(TEXT("/Engine/BasicShapes/Cube"),Center,Size/100,FLinearColor(.055f,.085f,.10f),true);
  A->Tags.Add(TEXT("CultMatchFloor")); A->Tags.Add(TEXT("CultTerrain")); return A;
 };
 if(Moba) FloorTile(FVector(0,0,-150),FVector(80000,46000,300));
 else {
  const float Xs[]={-32000,-14000,-3500,3500,14000,32000},Ys[]={-30000,-24000,-15500,-14000,-9500,9500,14000,15500,24000,30000};
  for(int X=0;X<UE_ARRAY_COUNT(Xs)-1;++X) for(int Y=0;Y<UE_ARRAY_COUNT(Ys)-1;++Y) {
   const float AX=FMath::Abs((Xs[X]+Xs[X+1])*.5f),AY=FMath::Abs((Ys[Y]+Ys[Y+1])*.5f);
   const bool EndEntrance=AX<3500&&AY>14000&&AY<24000;
   const bool KeepEntrance=AX>3500&&AX<14000&&AY>9500&&AY<15500;
   if(EndEntrance||KeepEntrance) continue;
   FloorTile(FVector((Xs[X]+Xs[X+1])*.5f,(Ys[Y]+Ys[Y+1])*.5f,-150),FVector(Xs[X+1]-Xs[X],Ys[Y+1]-Ys[Y],300));
  }
  FloorTile(FVector(0,0,-5150),FVector(7000,28000,300));
 }

 for(int Team=0;Team<2;++Team) {
  float X=(Team==0?-1:1)*(Moba?30000:23000); FLinearColor Color=Team?FLinearColor(.55f,.13f,.055f):FLinearColor(.04f,.26f,.4f);
  Mesh(TEXT("/Engine/BasicShapes/Cylinder"),FVector(X,0,4),FVector(68,68,.08f),Color,false);
  // Temple silhouettes frame the spawn without blocking exits.
  for(int I:{-1,1}) { Mesh(TEXT("/Engine/BasicShapes/Cylinder"),FVector(X,I*4500,2200),FVector(8,8,44),Color,true); Mesh(TEXT("/Engine/BasicShapes/Cone"),FVector(X,I*4500,4700),FVector(14,14,12),Color,false); }
 }
 if(Moba) {
  for(int Lane=0;Lane<3;++Lane) for(int I=0;I<FCultMobaRoute::Segments;++I) {
   const FVector A=FCultMobaRoute::Knot(Lane,I),B=FCultMobaRoute::Knot(Lane,I+1),D=B-A;
   auto* Road=Mesh(TEXT("/Engine/BasicShapes/Cube"),(A+B)*.5f+FVector(0,0,4),FVector((D.Size()+80)/100,32,.08f),FLinearColor(.11f,.17f,.18f),false);
   Road->SetActorRotation(D.Rotation()); Road->Tags.Add(TEXT("CultMobaRoad"));
   // Overlapping round shoulders hide segment seams without affecting collision.
   Mesh(TEXT("/Engine/BasicShapes/Cylinder"),A+FVector(0,0,4),FVector(32,32,.08f),FLinearColor(.11f,.17f,.18f),false);
  }
  // Lane roads stay flat; their intervening jungle is a set of true cliffs.
  // Enter by broad ramps from mid or the outer lane instead of walking through scenery.
  for(int Camp=0;Camp<FCultMobaJungle::Camps;++Camp) {
   const auto Ground=FCultMobaJungle::Ground(Camp);
   auto* Mesa=Mesh(TEXT("/Engine/BasicShapes/Cylinder"),FVector(Ground.X,Ground.Y,Ground.Z*.5f),FVector(70,50,Ground.Z/100),FLinearColor(.075f,.12f,.105f),true);
   Mesa->Tags.Add(TEXT("CultMatchWalkSurface")); Mesa->Tags.Add(TEXT("CultMobaJungleMesa"));
   for(bool Outer:{false,true}) {
    const auto Start=FCultMobaJungle::Entry(Camp,Outer),End=FCultMobaJungle::Crest(Camp,Outer),D=End-Start;
    auto* Ramp=Mesh(TEXT("/Engine/BasicShapes/Cube"),(Start+End)*.5f-FVector(0,0,150),FVector(D.Size()/100,28,3),FLinearColor(.12f,.17f,.14f),true);
    Ramp->SetActorRotation(D.Rotation()); Ramp->Tags.Add(TEXT("CultMatchWalkSurface")); Ramp->Tags.Add(TEXT("CultMobaJungleRamp"));
   }
   for(int XSide:{-1,1}) for(int YSide:{-1,1}) {
    auto* Rock=Mesh(TEXT("/Engine/BasicShapes/Sphere"),Ground+FVector(XSide*2600,YSide*1200,1000),FVector(10,13,20),FLinearColor(.09f,.13f,.14f),true);
    Rock->Tags.Add(TEXT("CultMobaGeology")); Rock->GetStaticMeshComponent()->SetVisibility(false);
   }
  }
 } else {
  const FLinearColor Stone(.12f,.16f,.18f),Trim(.31f,.38f,.36f),Seal(.58f,.52f,.29f);
  auto Box=[&](FVector Center,FVector Size,FLinearColor Color,bool Solid=true) { return Mesh(TEXT("/Engine/BasicShapes/Cube"),Center,Size/100,Color,Solid); };
  auto WalkSurface=[&](FVector Center,FVector Size) { auto* A=Box(Center,Size,Stone); A->Tags.Add(TEXT("CultMatchWalkSurface")); return A; };
  // Objective paint is exactly the scoring rectangle, including its corners.
  const auto E=CaptureHalfExtent();
  Box(FVector(0,0,4),FVector(E.X*2,E.Y*2,8),FLinearColor(.14f,.20f,.22f),false);
  for(int Side:{-1,1}) {
   Box(FVector(Side*E.X,0,16),FVector(65,E.Y*2,24),Seal,false);
   Box(FVector(0,Side*E.Y,16),FVector(E.X*2,65,24),Seal,false);
  }
  // Broken nave walls split the exposed direct approach from two wide flanks.
  // Gaps are scaled for the largest 30m entity, rather than human-sized doors.
  for(int Side:{-1,1}) {
   for(int End:{-1,1}) Box(FVector(Side*10500,End*2400,2300),FVector(1000,2200,4600),Stone);
   for(int End:{-1,1}) Box(FVector(Side*4300,End*6500,1800),FVector(3400,1300,3600),Stone);
  }
  // Four ramped keeps. The roof is a fighting deck; the spacious crypt beneath
  // is a covered through-route, open at both X ends. No invisible blocking art.
  for(int Team:{-1,1}) for(int Side:{-1,1}) {
   FVector Center(Team*10500,Side*12500,5500);
   auto* Deck=WalkSurface(Center,FVector(10000,8000,1000)); Deck->Tags.Add(TEXT("CultDominationDeck"));
   for(int Edge:{-1,1}) {
    Box(FVector(Center.X,Center.Y+Edge*3500,2500),FVector(10000,1000,5000),Stone);
    // Low crenellations provide cover without hiding the central landmark.
    for(int C=0;C<4;++C) if(Edge!=Side||C==0||C==3) Box(FVector(Center.X-3900+C*2600,Center.Y+Edge*3500,6600),FVector(1300,1000,1200),Trim);
   }
   // Outer ramp climbs 60m over 100m: reachable with normal walking, no jump.
   const FVector Start(Center.X,Side*26500,0),End(Center.X,Side*16500,6000);
   const FVector Along=End-Start; const float Length=Along.Size();
   auto* Ramp=WalkSurface((Start+End)*.5f-FVector(0,0,250),FVector(Length,6000,500));
   Ramp->SetActorRotation(Along.Rotation()); Ramp->Tags.Add(TEXT("CultDominationRamp"));
   // Overpass joins the pair of keeps across the outer flank. 50m headroom
   // makes a real sheltered route underneath for even the tallest roster hero.
  }
  for(int Side:{-1,1}) {
   auto* Bridge=WalkSurface(FVector(0,Side*12500,5500),FVector(11000,5000,1000)); Bridge->Tags.Add(TEXT("CultDominationOverpass"));
   for(int End:{-1,1}) Box(FVector(End*3500,Side*15500,2500),FVector(1000,1000,5000),Stone);
  }
  // The catacomb has 47m clear height under the surface slab. Both ends
  // rise at 26.6 degrees, allowing the entire roster to walk out normally.
  for(int Side:{-1,1}) {
   const FVector Start(0,Side*24000,0),End(0,Side*14000,-5000),Along=End-Start;
   const FVector Normal=FVector(-Along.Z*Along.X,-Along.Z*Along.Y,Along.SizeSquared2D()).GetSafeNormal();
   auto* Ramp=WalkSurface((Start+End)*.5f-Normal*150,FVector(Along.Size(),7000,300));
   Ramp->SetActorRotation(Along.Rotation()); Ramp->Tags.Add(TEXT("CultDominationTunnelRamp"));
   for(const FVector2D Span:{FVector2D(-24000,-15500),FVector2D(-9500,9500),FVector2D(15500,24000)})
    Box(FVector(Side*3900,(Span.X+Span.Y)*.5f,-2650),FVector(800,Span.Y-Span.X,4700),Stone);
   // Edge guides identify the descent from the outer courtyard.
   for(int Edge:{-1,1}) Box(FVector(Edge*3300,Side*24000,80),FVector(90,1600,160),Seal,false);
  }
  for(int Side:{-1,1}) for(int End:{-1,1}) {
   const float Y=Side*FCultDominationRoute::BranchY;
   const FVector Start(End*FCultDominationRoute::JunctionX,Y,FCultDominationRoute::Depth),Finish(End*FCultDominationRoute::CrestX,Y,0),D=Finish-Start;
   const FVector Normal=FVector(-D.Z*D.X,0,D.SizeSquared2D()).GetSafeNormal();
   const auto Color=End<0?FLinearColor(.07f,.24f,.3f):FLinearColor(.32f,.16f,.09f);
   auto* Ramp=WalkSurface((Start+Finish)*.5f-Normal*150,FVector(D.Size(),6000,300));
   Ramp->SetActorRotation(D.Rotation()); Ramp->Tags.Add(TEXT("CultDominationCryptRamp"));
   FloorTile(FVector(End*4000,Y,-5150),FVector(1000,6000,300));
   for(int Edge:{-1,1}) {
    Box(FVector(End*8750,Y+Edge*3400,-2650),FVector(10500,800,4700),Stone);
    auto* Guide=Box((Start+Finish)*.5f+FVector(0,Edge*2700,12),FVector(D.Size(),70,14),Color,false);
    Guide->SetActorRotation(D.Rotation());
   }
   // Distinctive portal framing, without shrinking the 60m clear opening.
   Box(FVector(End*14300,Y,4300),FVector(500,6000,400),Trim);
   for(int Edge:{-1,1}) {
    auto* Pillar=Box(FVector(End*14300,Y+Edge*2750,2100),FVector(500,500,4200),Trim);
    // Retained column art supplies the surface; the matching proxy supplies collision.
    Pillar->GetStaticMeshComponent()->SetVisibility(false);
   }
  }
  // Recessed side buttresses interrupt long views without blocking the main walk.
  for(int Side:{-1,1}) {
   auto* Buttress=Box(FVector(Side*2850,Side*5500,-3000),FVector(900,900,4000),Trim);
   Buttress->GetStaticMeshComponent()->SetVisibility(false);
  }
  for(int Y=-12000;Y<=12000;Y+=6000) {
   // Recessed cool lights keep enemies readable below the opaque roof.
   auto* Light=W->SpawnActor<APointLight>(FVector(0,Y,-1200),FRotator::ZeroRotator);
   auto* C=Light->PointLightComponent.Get(); C->SetMobility(EComponentMobility::Movable);
   C->SetIntensity(18000); C->SetAttenuationRadius(8500); C->SetLightColor(FLinearColor(.2f,.65f,.72f)); C->SetCastShadows(false);
   Light->Tags.Add(TEXT("MatchArena"));
   for(int Side:{-1,1}) Box(FVector(Side*3420,Y,-2600),FVector(60,1200,90),FLinearColor(.3f,.7f,.75f),false);
  }
  // Solid perimeter keeps the player on the arena; safety relocation is a fallback.
  for(int Side:{-1,1}) {
   Box(FVector(Side*32000,0,5000),FVector(1600,60000,10000),Stone);
   Box(FVector(0,Side*30000,5000),FVector(64000,1600,10000),Stone);
  }
 }
 auto Art=[&](const FString& Name,FVector Ground,FVector Dimensions,float Yaw) {
  FString Path=TEXT("/Game/CultUlhu/World/")+Name+TEXT("/StaticMeshes/")+Name;
  auto* Asset=LoadObject<UStaticMesh>(nullptr,*Path); if(!Asset) return;
  FVector Size=Asset->GetBoundingBox().GetSize(); FVector Scale(Dimensions.X/FMath::Max(1.,Size.X),Dimensions.Y/FMath::Max(1.,Size.Y),Dimensions.Z/FMath::Max(1.,Size.Z));
  Ground.Z-=Asset->GetBoundingBox().Min.Z*Scale.Z;
  auto* A=W->SpawnActor<AStaticMeshActor>(Ground,FRotator(0,Yaw,0)); auto* C=A->GetStaticMeshComponent(); C->SetMobility(EComponentMobility::Movable); C->SetForceDisableNanite(true); C->SetStaticMesh(Asset); C->SetWorldScale3D(Scale); C->SetCollisionEnabled(ECollisionEnabled::NoCollision); A->Tags.Add(TEXT("MatchArena"));
 };
 if(!Moba) {
  for(int Side:{-1,1}) for(int End:{-1,1}) for(int Edge:{-1,1})
   Art(TEXT("ruined-column-b"),FVector(End*14300,Side*12500+Edge*2750,0),FVector(500,500,4200),0);
  for(int Side:{-1,1}) Art(TEXT("ruined-column-b"),FVector(Side*2850,Side*5500,-5000),FVector(900,900,4000),0);
 }
 if(Moba) for(int Camp=0;Camp<FCultMobaJungle::Camps;++Camp) for(int XSide:{-1,1}) for(int YSide:{-1,1}) {
  Art(TEXT("cave-rock-tall-a"),FCultMobaJungle::Ground(Camp)+FVector(XSide*2600,YSide*1200,0),FVector(1000,1300,2200),XSide*35+YSide*15);
 }
 for(int Team=0;Team<2;++Team) {
  float X=(Team?1:-1)*(Moba?36000:29000); Art(TEXT("arch-dungeon"),FVector(X,0,0),FVector(2000,7800,6500),90);
  for(int Side:{-1,1}) { Art(TEXT("tower-square-mid"),FVector(X,Side*6500,0),FVector(2300,2300,10000),0); Art(TEXT("crypt-ruined"),FVector(X,Side*10000,0),FVector(5000,5000,6000),Team?180:0); }
 }
 for(float X:{-14000.f,14000.f}) for(int Side:{-1,1}) Art(TEXT("ruined-column-b"),FVector(X,Side*19500,0),FVector(900,900,6500),0);
 for(int I=0;I<24;++I) { float A=I*2*PI/24; Mesh(TEXT("/Engine/BasicShapes/Cone"),FVector(FMath::Cos(A)*(Moba?42000:34500),FMath::Sin(A)*(Moba?25000:32500),3500),FVector(35,35,100),FLinearColor(.08f,.11f,.14f),false); }
}

