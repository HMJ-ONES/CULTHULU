#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CultInspectionLayout.h"
#include "CultPlayHUD.h"
#include "CultPopulation.h"
#include "Engine/World.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultInspectionLayoutTest,"CultUlhu.UI.InspectionLayout",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultInspectionLayoutTest::RunTest(const FString&)
{
 FCultInspectionHover Hover;
 // Isolated real actors exercise target identity without ticking the game world.
 auto* Fixture=UWorld::CreateWorld(EWorldType::Game,false);
 auto* First=Fixture->SpawnActor<ACultPopulation>(); auto* Second=Fixture->SpawnActor<ACultPopulation>();
 for(int I=0;I<14;++I) Hover.Update(First,.1f);
 TestFalse(TEXT("Short hover remains invisible"),Hover.Visible.IsValid());
 for(int I=0;I<5;++I) Hover.Update(First,.1f);
 TestTrue(TEXT("A continuous dwell fades in one target"),Hover.Visible.Get()==First&&Hover.Opacity>.99f);
 Hover.Update(Second,.05f); TestTrue(TEXT("Moving away begins a real fade"),Hover.Visible.Get()==First&&Hover.Opacity>0&&Hover.Opacity<1);
 for(int I=0;I<4;++I) Hover.Update(Second,.1f);
 TestFalse(TEXT("A new target cannot inherit the first dwell"),Hover.Visible.IsValid());
 for(int I=0;I<15;++I) Hover.Update(Second,.1f);
 TestTrue(TEXT("Second target eventually becomes the only card"),Hover.Visible.Get()==Second&&Hover.Opacity>.99f);
 for(int I=0;I<3;++I) Hover.Update(nullptr,.1f);
 TestTrue(TEXT("Leaving all cultists completes fade out"),!Hover.Visible.IsValid()&&Hover.Opacity==0);
 Fixture->DestroyWorld(false);
 for(const FVector2D View:{FVector2D(640,360),FVector2D(1280,720),FVector2D(3840,2160)}) {
  const float S=FMath::Clamp(FMath::Min(View.X/1280.f,View.Y/720.f),.8f,1.5f);
  const FBox2D Safe(FVector2D(8*S,FMath::Max(100.f,112*S)),FVector2D(View.X-8*S,View.Y-125*S));
  TArray<FBox2D> Occupied;
  const FBox2D Menu(FVector2D(View.X*.65f,Safe.Min.Y),Safe.Max); Occupied.Add(Menu);
  int Placed=0;
  for(int I=0;I<40;++I) {
   FBox2D Box; const FVector2D Anchor(View.X*.4f+(I%3)*8,View.Y*.6f);
   const bool Detailed=I%5==0;
   if(!CultPlaceInspection(Anchor,FVector2D((Detailed?300:220)*S,(Detailed?72:52)*S),Safe,Occupied,6*S,Box)) continue;
   TestTrue(TEXT("Inspection leaves the visible body below its head unobscured"),Box.Max.Y<=Anchor.Y);
   TestTrue(TEXT("Cards remain inside the usable viewport"),Box.Min.X>=Safe.Min.X&&Box.Min.Y>=Safe.Min.Y&&Box.Max.X<=Safe.Max.X&&Box.Max.Y<=Safe.Max.Y);
   for(const auto& Previous:Occupied) TestFalse(TEXT("Cards avoid other cards and command menus"),Box.Intersect(Previous));
   Occupied.Add(Box); ++Placed;
  }
  TestTrue(TEXT("Detailed cards leave room to inspect neighboring units"),Placed>=(View.X<1000?2:3));
 }
 return true;
}
#endif
