#pragma once
#include "CoreMinimal.h"

// One world-space route definition for the native map and the original rules
// adapter. Progress always runs from the chosen team's temple to its opponent.
struct FCultLaneProjection {
 FVector Point=FVector::ZeroVector,Tangent=FVector::ForwardVector;
 float Progress=0,Distance=0;
};
struct FCultMobaRoute {
 static constexpr int Segments=32;
 static FVector Knot(int Lane,int Index) {
  const float T=FMath::Clamp(Index,0,Segments)/float(Segments);
  return FVector(-30000+60000*T,(FMath::Clamp(Lane,0,2)-1)*17500*FMath::Sin(PI*T),0);
 }
 static FVector Point(int Team,int Lane,float Progress) {
  const float T=FMath::Clamp(Team?1-Progress:Progress,0.f,1.f)*Segments;
  const int I=FMath::Min(Segments-1,FMath::FloorToInt(T));
  return FMath::Lerp(Knot(Lane,I),Knot(Lane,I+1),T-I);
 }
 static FVector Waypoint(int Team,int Lane,int Index) { return Knot(Lane,Team?Segments-Index:Index); }
 static FCultLaneProjection Project(int Team,int Lane,FVector Position) {
  FCultLaneProjection Result; float Best=MAX_flt;
  for(int I=0;I<Segments;++I) {
   const FVector A=Knot(Lane,I),Delta=Knot(Lane,I+1)-A;
   const float Alpha=FMath::Clamp(float(FVector::DotProduct(Position-A,Delta)/Delta.SizeSquared2D()),0.f,1.f);
   const FVector Nearest=A+Alpha*Delta; const float D=FVector::DistSquared2D(Position,Nearest);
   if(D>=Best) continue; Best=D; Result.Point=Nearest; Result.Progress=(I+Alpha)/Segments;
   Result.Tangent=Delta.GetSafeNormal2D()*(Team?-1:1);
  }
  if(Team) Result.Progress=1-Result.Progress;
  Result.Distance=FMath::Sqrt(Best); return Result;
 }
 static FVector Follow(int Team,int Lane,FVector Position,float GoalProgress) {
  const auto At=Project(Team,Lane,Position);
  if(At.Distance>1600) return At.Point;
  return Point(Team,Lane,At.Progress+FMath::Clamp(GoalProgress-At.Progress,-.045f,.045f));
 }
};
