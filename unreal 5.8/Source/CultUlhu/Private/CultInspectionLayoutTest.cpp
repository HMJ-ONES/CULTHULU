#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CultInspectionLayout.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultInspectionLayoutTest,"CultUlhu.UI.InspectionLayout",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultInspectionLayoutTest::RunTest(const FString&)
{
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
