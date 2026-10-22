#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CultLivingLandscape.generated.h"
class UProceduralMeshComponent;
UCLASS()
class CULTULHU_API ACultLivingLandscape : public AActor
{
 GENERATED_BODY()
public:
 ACultLivingLandscape();
 void Generate(int32 Seed);
 static constexpr float Extent=180000.f;
 float Height(float X,float Y) const;
 FBox GroundBounds() const;
 static ACultLivingLandscape* Find(UWorld* W);
 static bool Surface(UWorld* W,FVector P,FVector& Out,float MinNormal=.65f);
 static const TArray<FVector>& CaveSites();
private:
 UPROPERTY() UProceduralMeshComponent* Ground_;
 UPROPERTY() UProceduralMeshComponent* Caves_;
 FVector2D NoiseOffset_;
};
