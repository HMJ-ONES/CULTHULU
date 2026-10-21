#include "CultLivingLandscape.h"
#include "ProceduralMeshComponent.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"

ACultLivingLandscape::ACultLivingLandscape()
{
 PrimaryActorTick.bCanEverTick=false; Tags.Add(TEXT("CultTerrain"));
 Ground_=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("LivingGround")); RootComponent=Ground_;
 Caves_=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("CavernVaults")); Caves_->SetupAttachment(Ground_);
 for(auto* C:{Ground_,Caves_}) { C->SetCollisionProfileName(TEXT("BlockAll")); C->bUseComplexAsSimpleCollision=true; C->bUseAsyncCooking=false; }
}
const TArray<FVector>& ACultLivingLandscape::CaveSites() { static TArray<FVector> Sites={FVector(-65000,-55000,0),FVector(90000,-45000,0),FVector(30000,110000,0)}; return Sites; }
float ACultLivingLandscape::Height(float X,float Y) const
{
 const FVector2D P(X,Y);
 const float N=FMath::PerlinNoise2D(P/60000.f+NoiseOffset_);
 const float Detail=FMath::PerlinNoise2D(P/18000.f+NoiseOffset_)*1800.f;
 const float Ridge=FMath::Pow(FMath::Abs(FMath::PerlinNoise2D(P/85000.f+NoiseOffset_+FVector2D(9,17))),1.5f)*33000.f;
 float H=N*6500.f+Ridge+Detail;
 // Broad connecting vales and safe founding grounds; mountains retain steep silhouettes.
 const float Vale=FMath::Exp(-FMath::Square((Y-18000.f*FMath::Sin(X/55000.f))/19000.f));
 H=FMath::Lerp(H,300.f+Detail*.15f,Vale*.8f);
 const TArray<FVector> Havens={FVector(0,0,0),FVector(75000,65000,0),FVector(-85000,70000,0)};
 for(const auto& S:Havens) H*=FMath::SmoothStep(18000.f,30000.f,float(FVector2D::Distance(P,FVector2D(S))));
 for(const auto& S:CaveSites()) H=FMath::Lerp(1600.f,H,FMath::SmoothStep(12000.f,20000.f,float(FVector2D::Distance(P,FVector2D(S)))));
 // Elevated boundary ridges read as the edge of the world, rather than a cut plane.
 H+=FMath::Square(FMath::Clamp((FMath::Max(FMath::Abs(X),FMath::Abs(Y))-155000.f)/25000.f,0.f,1.f))*18000.f;
 return H;
}
void ACultLivingLandscape::Generate(int32 Seed)
{
 FRandomStream R(Seed); NoiseOffset_=FVector2D(R.FRandRange(20,200),R.FRandRange(20,200));
 TArray<FVector> V,N; TArray<int32> T; TArray<FVector2D> UV; TArray<FLinearColor> Colors;
 constexpr int Grid=180; constexpr float Step=Extent*2/Grid;
 for(int Y=0;Y<=Grid;++Y) for(int X=0;X<=Grid;++X) {
  const float PX=-Extent+X*Step,PY=-Extent+Y*Step,H=Height(PX,PY);
  const FVector Normal=FVector(Height(PX-100,PY)-Height(PX+100,PY),Height(PX,PY-100)-Height(PX,PY+100),200).GetSafeNormal();
  V.Add(FVector(PX,PY,H)); N.Add(Normal); UV.Add(FVector2D(PX/5000,PY/5000));
  Colors.Add(FMath::Lerp(FLinearColor(.11,.16,.13),FLinearColor(.42,.40,.39),FMath::Clamp(H/18000.f+(1-Normal.Z),0.f,1.f)));
  if(X<Grid&&Y<Grid) { const int I=Y*(Grid+1)+X; T.Append({I,I+Grid+2,I+1,I,I+Grid+1,I+Grid+2}); }
 }
 Ground_->CreateMeshSection_LinearColor(0,V,T,N,UV,Colors,TArray<FProcMeshTangent>(),true);
 if(auto* M=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CultUlhu/Materials/M_LivingGround"))) Ground_->SetMaterial(0,M);
 V.Reset(); N.Reset(); T.Reset(); UV.Reset(); Colors.Reset();
 // True open tunnels: 60 m high, 90 m wide, 160 m deep. Ground remains walkable inside.
 for(const auto& Site:CaveSites()) {
  const int Base=V.Num();
  for(int D=0;D<=8;++D) for(int A=0;A<=16;++A) {
   const float Angle=A*PI/16.f;
   const float W=4500.f*(1.f+.07f*FMath::Sin(D*2.f+A));
   V.Add(Site+FVector(FMath::Cos(Angle)*W,-8000+D*2000,1600+FMath::Sin(Angle)*6000));
   N.Add(FVector(-FMath::Cos(Angle),0,-FMath::Sin(Angle))); UV.Add(FVector2D(A/4.f,D/2.f)); Colors.Add(FLinearColor(.16,.17,.19));
   if(D<8&&A<16) { int I=Base+D*17+A; T.Append({I,I+17,I+18,I,I+18,I+1,I+18,I+17,I,I+1,I+18,I}); }
  }
 }
 Caves_->CreateMeshSection_LinearColor(0,V,T,N,UV,Colors,TArray<FProcMeshTangent>(),true);
 Caves_->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CultUlhu/Materials/M_LivingCavern")));
 UE_LOG(LogTemp,Log,TEXT("[CultUlhu] Living landscape: 3.6 km across, %d ground triangles, 3 walk-through caverns, seed %d"),Grid*Grid*2,Seed);
}
FBox ACultLivingLandscape::GroundBounds() const { return Ground_->Bounds.GetBox(); }
ACultLivingLandscape* ACultLivingLandscape::Find(UWorld* W) { for(TActorIterator<ACultLivingLandscape> It(W);It;++It) return *It; return nullptr; }
bool ACultLivingLandscape::Surface(UWorld* W,FVector P,FVector& Out,float MinNormal)
{
 auto* Land=Find(W); if(!Land||FMath::Abs(P.X)>Extent-3000||FMath::Abs(P.Y)>Extent-3000) return false;
 FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(CultLandGround),true);
 // Trace the ground component directly, so roofs and cavern ceilings cannot become spawn floors.
 if(!Land->Ground_->LineTraceComponent(H,FVector(P.X,P.Y,70000),FVector(P.X,P.Y,-30000),Q)||H.ImpactNormal.Z<MinNormal) return false;
 Out=H.ImpactPoint; return true;
}
