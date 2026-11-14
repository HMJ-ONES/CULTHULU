#include "CultWorldState.h"
#include "CultRegionMap.h"
#include "CultRegionNavigation.h"
#include "CultMatchDirector.h"

FString ACultWorldDirector::RegionOrder(int32 Action,FVector Point)
{
 if(!HasAuthority()||!State_||ACultMatchDirector::IsMatch(GetWorld())) return TEXT("Expeditions are available only in Free Roam.");
 const int Region=CultRegionMap::Index(Point);
 if(Region<0||Point.ContainsNaN()||Action<ExploreRegion||Action>RaidRegion) return TEXT("Choose a region inside the world map.");
 if(!BattlePrepared()||PreparedCount()<=0) return TEXT("Select followers and Prepare for battle before sending an expedition.");
 Point=CultRegionMap::Center(Region); int CityFaction=-1;
 if(Action==RaidRegion) {
  ACultDestructible* City=nullptr; float Best=MAX_flt;
  for(TActorIterator<ACultDestructible> It(GetWorld());It;++It) if(It->ActorHasTag(TEXT("LivingCity"))&&!It->IsDestroyed()&&It->GetFaction()!=0&&CultRegionMap::Index(It->GetActorLocation())==Region) {
   const float Dist=FVector::DistSquared(Point,It->GetActorLocation()); if(Dist<Best) { Best=Dist; City=*It; }
  }
  if(!City) return TEXT("No living enemy city remains in this region. Explore or hunt instead.");
  Point=City->GetActorLocation(); CityFaction=City->GetFaction();
 }
 FVector Floor=FVector::ZeroVector; bool Safe=false;
 for(int Ring=0;Ring<5&&!Safe;++Ring) for(int I=0;I<(Ring?8:1)&&!Safe;++I) {
  const FVector Test=Point+FVector(FMath::Cos(I*PI/4)*Ring*1800,FMath::Sin(I*PI/4)*Ring*1800,0);
  Safe=CultRegionMap::Index(Test)==Region&&ACultLivingLandscape::Surface(GetWorld(),Test,Floor);
 }
 if(!Safe) return TEXT("No safe expedition destination in this region. Choose another region.");
 const FString Verb=Action==ExploreRegion?TEXT("Explore"):Action==HuntRegion?TEXT("Hunt"):TEXT("Raid");
 int Assigned=0;
 for(auto& Weak:State_->Prepared) if(auto* P=Weak.Get()) if(P->GetHealth()>0&&P->IsCultist()&&P->GetFaction()==0&&!P->IsCaptive()) { TArray<FVector> Route; if(!CultRegionNavigation::Route(GetWorld(),P->GetActorLocation(),Floor,Route)) continue; P->PlayerOrder(Verb,Floor,nullptr,true); ++Assigned; }
 if(!Assigned) return TEXT("No prepared troops have a safe terrain route to this region. Try a neighboring region.");
 if(CityFaction>0) DeclareWar(0,CityFaction);
 return FString::Printf(TEXT("%d troops dispatched to %s: %s. They march and remain assigned; ALT + reselect to redirect, or Global retreat."),Assigned,*CultRegionMap::Name(Region),*Verb);
}
