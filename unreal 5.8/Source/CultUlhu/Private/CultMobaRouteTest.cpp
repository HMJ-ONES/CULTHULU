#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "CultMobaRoute.h"
#include "CultMatchState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultMobaRouteTest,"CultUlhu.Moba.RouteGeometry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultMobaRouteTest::RunTest(const FString&) {
 cultulhu::EventBus Bus; cultulhu::GameClock Clock; cultulhu::RNG Rng(72241);
 FCultNativeMoba Rules(Bus,Clock,Rng); Rules.Setup();
 for(int Lane=0;Lane<3;++Lane) {
  TestTrue(TEXT("Both temples are joined"),FCultMobaRoute::Point(0,Lane,0).Equals(FVector(-30000,0,0),.1)&&FCultMobaRoute::Point(0,Lane,1).Equals(FVector(30000,0,0),.1));
  for(int I=1;I<FCultMobaRoute::Segments;++I) {
   const FVector A=FCultMobaRoute::Knot(Lane,I-1),B=FCultMobaRoute::Knot(Lane,I),C=FCultMobaRoute::Knot(Lane,I+1);
   TestTrue(TEXT("Lanes advance continuously without sharp right-angle corners"),FVector::DotProduct((B-A).GetSafeNormal(),(C-B).GetSafeNormal())>.985f);
   for(int Team=0;Team<2;++Team) {
    const FVector At=Rules.Waypoint(Team,Lane,I);
    const auto Projection=FCultMobaRoute::Project(Team,Lane,At);
    TestTrue(TEXT("Adapter waypoints and native lane projection agree"),Projection.Distance<.1f&&FMath::IsNearlyEqual(Projection.Progress,I/float(FCultMobaRoute::Segments),.0001f));
   }
  }
  const auto Center=FCultMobaRoute::Point(0,Lane,.5f);
  TestTrue(TEXT("Outer lanes bow around the jungle and mid stays direct"),FMath::Abs(Center.Y-(Lane-1)*17500)<.1);
 }
 int Count=0;
 for(const auto& Tower:Rules.Towers()) {
  const auto At=FVector(Tower.pos.x,Tower.pos.z,0)*200;
  TestTrue(TEXT("Every rules-owned tower stands on its actual curved lane"),FCultMobaRoute::Project(Tower.team,Tower.lane,At).Distance<.2f);
  TestEqual(TEXT("Tower durability is retained"),Tower.hp,3000.f); ++Count;
 }
 TestEqual(TEXT("Both teams retain all three pairs of lane towers"),Count,12);
 const FVector Miss(3500,20500,0); const auto Return=FCultMobaRoute::Project(1,2,Miss);
 TestTrue(TEXT("Thrown minion return projection reaches the curve, not the old constant-Y line"),Return.Point.Y>14000&&Return.Distance>2000);
 return true;
}
#endif
