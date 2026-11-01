#include "CultRegionNavigation.h"
#include "CultLivingLandscape.h"
#include <queue>
#include <vector>

bool CultRegionNavigation::Route(UWorld* World,FVector From,FVector To,TArray<FVector>& Out)
{
 Out.Reset(); auto* Land=ACultLivingLandscape::Find(World); if(!Land) return false;
 constexpr int N=60,Total=N*N; constexpr float Step=6000,Extent=ACultLivingLandscape::Extent;
 auto Cell=[](FVector P) { return FMath::Clamp(FMath::FloorToInt((P.Y+Extent)/Step),0,N-1)*N+FMath::Clamp(FMath::FloorToInt((P.X+Extent)/Step),0,N-1); };
 auto Position=[](int I) { return FVector(-Extent+(I%N+.5f)*Step,-Extent+(I/N+.5f)*Step,0); };
 // Tiny shared height grid, rebuilt for a different world/seed. No navmesh bake or asset.
 static TWeakObjectPtr<ACultLivingLandscape> CachedLand; static int Seed=0; static TArray<float> Height;
 if(CachedLand.Get()!=Land||Seed!=Land->WorldSeed||Height.Num()!=Total) { CachedLand=Land; Seed=Land->WorldSeed; Height.SetNum(Total); for(int I=0;I<Total;++I) { const auto P=Position(I); Height[I]=Land->Height(P.X,P.Y); } }
 const int Start=Cell(From),Goal=Cell(To);
 TArray<float> Cost; Cost.Init(MAX_flt,Total); TArray<int> Parent; Parent.Init(INDEX_NONE,Total);
 struct Node { int Id; float Score; bool operator<(const Node& B) const { return Score>B.Score; } };
 std::priority_queue<Node> Open; Cost[Start]=0; Open.push({Start,0});
 while(!Open.empty()) {
  const int Current=Open.top().Id; Open.pop(); if(Current==Goal) break;
  for(int DY=-1;DY<=1;++DY) for(int DX=-1;DX<=1;++DX) {
   if(!DX&&!DY) continue; const int X=Current%N+DX,Y=Current/N+DY; if(X<1||Y<1||X>=N-1||Y>=N-1) continue;
   const int Next=Y*N+X; const float Distance=Step*(DX&&DY?1.414214f:1.f);
   if(FMath::Abs(Height[Next]-Height[Current])>Distance*.45f) continue;
   const FVector A=Position(Current),B=Position(Next); bool Walkable=true;
   for(float T:{.25f,.5f,.75f}) { const auto P=FMath::Lerp(A,B,T); const float H=Land->Height(P.X,P.Y); if(FMath::Abs(H-FMath::Lerp(Height[Current],Height[Next],T))>1000) { Walkable=false; break; } }
   if(!Walkable) continue;
   const float NewCost=Cost[Current]+Distance+FMath::Abs(Height[Next]-Height[Current])*2;
   if(NewCost>=Cost[Next]) continue; Cost[Next]=NewCost; Parent[Next]=Current;
   Open.push({Next,NewCost+float(FVector::Dist2D(Position(Next),Position(Goal)))});
  }
 }
 if(Start!=Goal&&Parent[Goal]<0) return false;
 TArray<int> Reverse; for(int I=Goal;I!=Start;I=Parent[I]) { if(I<0||Reverse.Num()>Total) return false; Reverse.Add(I); }
 for(int I=Reverse.Num()-1;I>=0;--I) { FVector P=Position(Reverse[I]); P.Z=Height[Reverse[I]]; Out.Add(P); }
 Out.Add(To); return true;
}
