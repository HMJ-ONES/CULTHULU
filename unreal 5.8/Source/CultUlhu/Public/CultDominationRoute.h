#pragma once
#include "CoreMinimal.h"
// Match geometry and bot decisions share these connections. Coordinates are feet.
struct FCultDominationRoute {
 static constexpr float BranchY=12500,Depth=-5000,CrestX=14000,JunctionX=4500;
 static TArray<FVector> Flank(int Team,int Side) {
  const float Home=Team?1.f:-1.f,Other=-Home,Y=Side*BranchY;
  return {FVector(Home*17000,Y,0),FVector(Home*15000,Y,0),FVector(0,Y,Depth),FVector(Other*15000,Y,0),FVector(Other*17000,Y,0),FVector(Other*17000,Side*6500,0),FVector(0,Side*1800,0)};
 }
 static TArray<FVector> Escape(FVector Feet,int Team,FVector Base) {
  const int Side=Feet.Y>=0?1:-1;
  const float Exit=FMath::Abs(Feet.X)>3500?FMath::Sign(Feet.X):(Team?1.f:-1.f),Y=Side*BranchY;
  TArray<FVector> Path;
  if(FMath::Abs(Feet.X)<4000||FMath::Abs(Feet.Y-Y)>2000) Path.Add(FVector(0,Y,Depth));
  Path.Append({FVector(Exit*15000,Y,0),FVector(Exit*17000,Y,0),FVector(Exit*17000,Side*6500,0),Base});
  return Path;
 }
};
