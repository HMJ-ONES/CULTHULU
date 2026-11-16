#pragma once
#include "CultMobaRoute.h"

// Four mirrored plateaus between the lanes. Heights vary by side, not by team.
struct FCultMobaJungle {
 static constexpr int Camps=4;
 static FVector Ground(int Camp) { return FVector(Camp%2?11500:-11500,Camp/2?6500:-6500,Camp/2?3500:2500); }
 static FVector Entry(int Camp,bool Outer) { const auto C=Ground(Camp); return FVector(C.X,Outer?(C.Y>0?13000:-13000):0,0); }
 static FVector Crest(int Camp,bool Outer) { auto C=Ground(Camp); C.Y+=(C.Y>0?1:-1)*(Outer?2300:-2300); C.Z+=20; return C; }
 static int PlateauAt(FVector Position) {
  for(int I=0;I<Camps;++I) { const auto D=Position-Ground(I); if((Position.Z>1000&&FMath::Square(D.X/3800)+FMath::Square(D.Y/2900)<1.1)||(Position.Z>350&&FMath::Abs(D.X)<1600&&FMath::Abs(D.Y)<6500)) return I; }
  return -1;
 }
 static FVector Approach(int Camp,int Lane,FVector Position) {
  const bool Outer=Lane==(Camp/2?2:0); const auto Gate=Entry(Camp,Outer);
  if(PlateauAt(Position)==Camp||FVector::Dist2D(Position,Gate)<750) return Ground(Camp);
  return Gate;
 }
 static FVector Exit(int Camp,int Lane,FVector Position) {
  const bool Outer=Lane==(Camp/2?2:0); const auto Top=Crest(Camp,Outer);
  return Position.Z>Ground(Camp).Z-400&&FVector::Dist2D(Position,Top)>900?Top:Entry(Camp,Outer);
 }
 static TArray<FVector> ReturnToLane(int Team,int Lane,FVector Landing) {
  TArray<FVector> Path; const int Camp=PlateauAt(Landing);
  if(Camp<0) { Path.Add(FCultMobaRoute::Project(Team,Lane,Landing).Point); return Path; }
  const bool Outer=Lane==(Camp/2?2:0);
  Path.Add(Crest(Camp,Outer)); Path.Add(Entry(Camp,Outer));
  if(!Outer&&Lane!=1) {
   // A throw across the map's centre must use the other plateau's ramps too.
   const int Across=Camp<2?Camp+2:Camp-2;
   Path.Add(Crest(Across,false)); Path.Add(Ground(Across)); Path.Add(Crest(Across,true)); Path.Add(Entry(Across,true));
  }
  Path.Add(FCultMobaRoute::Project(Team,Lane,Path.Last()).Point); return Path;
 }
};
