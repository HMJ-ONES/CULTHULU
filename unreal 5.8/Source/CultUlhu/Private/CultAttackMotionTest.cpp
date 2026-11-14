#if WITH_DEV_AUTOMATION_TESTS
#include "CultAttackMotion.h"
#include "Misc/AutomationTest.h"

namespace {
bool Different(const CultAttackMotion::FFrame& A,const CultAttackMotion::FFrame& B) {
 return !A.Body.Equals(B.Body,.01)||!A.Limbs.Equals(B.Limbs,.01)||!A.Details.Equals(B.Details,.01);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultAttackMotionTest,"CultUlhu.Art.AttackChoreography",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultAttackMotionTest::RunTest(const FString&) {
 using namespace CultAttackMotion;
 using FAttackFrame=CultAttackMotion::FFrame;
 static const TCHAR* Roster[]={
#define CULT_RELEASE_CHARACTER(Id) TEXT(Id),
#include "CultReleaseRoster.inl"
#undef CULT_RELEASE_CHARACTER
 };
 int32 Checked=0;
 for(const TCHAR* Id:Roster) {
  FAttackFrame Contacts[5];
  for(int32 Index=0;Index<5;++Index) {
   const FString Label=FString::Printf(TEXT("%s slot %d"),Id,Index);
   const FPlayback Motion{EMove(int(EMove::Primary)+Index),0,0,.25f,.72f,{}};
   const FAttackFrame Ready=Sample(Id,Motion,0);
   const FAttackFrame Gather=Sample(Id,Motion,.17f);
   const FAttackFrame Contact=Sample(Id,Motion,Motion.Contact);
   const FAttackFrame Carry=Sample(Id,Motion,.38f);
   const FAttackFrame Rest=Sample(Id,Motion,Motion.Duration);
   TestFalse(Label+TEXT(" begins neutral"),Different(Ready,{}));
   TestTrue(Label+TEXT(" visibly gathers before release"),Different(Gather,Ready));
   TestTrue(Label+TEXT(" moves from anticipation into contact"),Different(Contact,Gather));
   TestTrue(Label+TEXT(" has a separate follow-through"),Different(Carry,Contact));
   TestFalse(Label+TEXT(" settles smoothly"),Different(Rest,{}));
   for(int32 Prior=0;Prior<Index;++Prior) TestTrue(Label+TEXT(" has a distinct contact silhouette"),Different(Contact,Contacts[Prior]));
   Contacts[Index]=Contact;
   ++Checked;
  }
 }
 TestEqual(TEXT("All released characters have five checked moves"),Checked,int32(UE_ARRAY_COUNT(Roster))*5);

 const FPlayback Lift{EMove::RockLift,0,0,.30f,.64f,{}};
 const FAttackFrame Held=Sample(TEXT("cthulhu_avatar"),Lift,2.5f);
 TestTrue(TEXT("The raised guard holds after the lift finishes"),Held.Limbs.X<-35);
 const FPlayback Lower{EMove::Lower,0,2.5f,.12f,.46f,Held};
 TestFalse(TEXT("The drop starts continuously from the braced pose"),Different(Sample(TEXT("cthulhu_avatar"),Lower,2.5f),Held));
 TestFalse(TEXT("The drop settles to neutral"),Different(Sample(TEXT("cthulhu_avatar"),Lower,3.f),{}));
 return true;
}
#endif
