#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CultWorldState.h"
#include "CultBeliefCatalog.h"
#include "exertion/ActionExertionTable.h"

struct FCultFaithTestAccess {
 static auto& State(ACultWorldDirector* D) { return *D->State_; }
 static void Step(ACultWorldDirector* D,float Dt) { D->TickFaith(Dt); }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultFaithGameplayTest,"CultUlhu.Beliefs",
 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCultFaithGameplayTest::RunTest(const FString& Parameters)
{
 using B=cultulhu::Belief; using E=cultulhu::EventType; using A=cultulhu::AmbientAction;
 UWorld* W=nullptr; for(const auto& C:GEngine->GetWorldContexts()) if(C.World()&&C.World()->IsGameWorld()) W=C.World();
 auto* D=W?ACultWorldDirector::Find(W):nullptr; if(!TestNotNull(TEXT("Belief test has a living world"),D)) return false;
 auto& State=FCultFaithTestAccess::State(D); auto& Faith=*State.Faiths[0]; const auto Saved=Faith.Beliefs.active();
 for(int I=0;I<12;++I) { TestTrue(FString::Printf(TEXT("Belief %d has reviewable rules and original feeds"),I),D->BeliefReport(I).Contains(TEXT("Canonical exertion feeds"))); }
 // Freeze unrelated autonomous scheduling while testing physical fixtures. Restore it below.
 TArray<float> Work; for(auto& F:State.Faiths) { Work.Add(F->WorkTimer); F->WorkTimer=1e9; }
 for(auto& Row:State.People) if(Row.Actor.IsValid()) { Row.Action=-1; Row.Timer=1e9; Row.RestDue=1e9; Row.Actor->Order(TEXT("TestHold"),Row.Actor->GetActorLocation(),nullptr,1000,true); }
 Faith.Beliefs.restoreActive({}); TestTrue(TEXT("First pending slot"),D->RequestBelief(int(B::War)));
 TestTrue(TEXT("Second pending slot"),D->RequestBelief(int(B::Dreams))); TestTrue(TEXT("Third pending slot"),D->RequestBelief(int(B::Magic)));
 TestFalse(TEXT("Fourth pending slot is refused"),D->RequestBelief(int(B::Fear)));
 Faith.Beliefs.update(119); TestFalse(TEXT("Adoption waits for followers"),Faith.Beliefs.isActive(B::War)); Faith.Beliefs.update(1); TestTrue(TEXT("Adoption completes after 120s"),Faith.Beliefs.isActive(B::War));
 FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 TArray<ACultPopulation*> Fixtures;
 auto Person=[&](FVector P,bool Cult=true) { auto* N=W->SpawnActor<ACultPopulation>(P,FRotator::ZeroRotator,Params); Fixtures.Add(N); if(Cult) N->RecruitTo(0); return N; };
 const FVector Site(0,-60000,2000); auto* Healer=Person(Site); auto* Patient=Person(Site+FVector(150,0,0));
 Healer->Order(TEXT("Patrol"),Site,nullptr,1); Patient->Order(TEXT("TestHold"),Patient->GetActorLocation(),nullptr,1000,true);
 Patient->ReceiveAttack(40); Faith.Beliefs.restoreActive({B::Reconstruction});
 FCultFaithTestAccess::Step(D,0);
 const int Heals=D->FaithEventCount(int(E::HealPerformed)); const float Before=Patient->GetHealth(); const float Fervor=D->BeliefExertion(int(B::Reconstruction));
 TestTrue(TEXT("Tending selects a real nearby wounded patient"),D->BeginAmbient(Healer,int(A::TendWounded)));
 FCultFaithTestAccess::Step(D,29); TestEqual(TEXT("Tending does not reward work before completion"),D->FaithEventCount(int(E::HealPerformed)),Heals);
 FCultFaithTestAccess::Step(D,1); TestEqual(TEXT("Completed tending emits exactly one heal"),D->FaithEventCount(int(E::HealPerformed)),Heals+1);
 TestTrue(TEXT("The patient actually gains health"),Patient->GetHealth()>Before); TestTrue(TEXT("The heal feeds Reconstruction fervor"),D->BeliefExertion(int(B::Reconstruction))>Fervor);
 const float Healed=Patient->GetHealth(); FCultFaithTestAccess::Step(D,0); TestEqual(TEXT("A subsequent native snapshot preserves healing"),Patient->GetHealth(),Healed);
 Faith.Beliefs.restoreActive({B::Dreams}); const int Starts=D->FaithEventCount(int(E::RestStarted)); D->FaithAction(TEXT("Rest"),Healer,Site);
 TestEqual(TEXT("Ordered rest enters the real sleeper roster"),D->FaithEventCount(int(E::RestStarted)),Starts+1); const float Power=D->FaithPower();
 FCultFaithTestAccess::Step(D,1); TestTrue(TEXT("A real sleeper generates Dreams power"),D->FaithPower()>Power);
 Healer->Order(TEXT("TestHold"),Site,nullptr,1000,true); FCultFaithTestAccess::Step(D,0); TestEqual(TEXT("An overriding directive ends rest"),int(Faith.Dreams.restingCount()),0);
 Faith.Beliefs.restoreActive({B::Conversion}); int CivBefore=0; for(TActorIterator<ACultPopulation> It(W);It;++It) CivBefore+=!It->IsCultist()&&!It->IsSummoned();
 const int Converts=D->FaithEventCount(int(E::ConversionPerformed)); const FString Mass=D->FaithAction(TEXT("MassConvert"),nullptr,Site);
 int CivAfter=0; for(TActorIterator<ACultPopulation> It(W);It;++It) CivAfter+=!It->IsCultist()&&!It->IsSummoned();
 TestTrue(TEXT("Mass conversion can be used on the real population"),Mass.Contains(TEXT("Three real"))); TestEqual(TEXT("Exactly three real civilians change faction"),CivBefore-CivAfter,3);
 TestEqual(TEXT("Mass conversion publishes a single amount-three conversion"),D->FaithEventCount(int(E::ConversionPerformed)),Converts+1); TestFalse(TEXT("Mass conversion obeys its original cooldown"),Faith.Beliefs.canMassConvert());
 // A fresh fixture trap needs clear ground and an actual civilian crossing.
 FVector Ground; if(ACultLivingLandscape::Surface(W,FVector(25000,-50000,0),Ground)) {
  Faith.Beliefs.restoreActive({B::Trickery,B::Fear}); D->FaithAction(TEXT("Trap"),nullptr,Ground);
  auto* Captive=Person(Ground+FVector(0,0,100),false); FCultFaithTestAccess::Step(D,0);
  const int Springs=D->FaithEventCount(int(E::TrapSprung)); FCultFaithTestAccess::Step(D,.01f);
  TestTrue(TEXT("A trap captures a real civilian"),Captive->IsCaptive()); TestEqual(TEXT("One trap crossing gives exactly one reward"),D->FaithEventCount(int(E::TrapSprung)),Springs);
  const int Once=D->FaithEventCount(int(E::TrapSprung)); FCultFaithTestAccess::Step(D,1); TestEqual(TEXT("A sprung trap cannot repeatedly generate rewards"),D->FaithEventCount(int(E::TrapSprung)),Once);
 }
 // Use a known altar and nearby real participants; no abstract sacrificial people.
 ACultDestructible* Altar=nullptr; for(TActorIterator<ACultDestructible> It(W);It;++It) if(It->ActorHasTag(TEXT("FaithAltar"))&&It->GetFaction()==0&&!It->IsDestroyed()) { Altar=*It; break; }
 if(TestNotNull(TEXT("The player has an actual ritual altar"),Altar)) {
  auto* Prisoner=Person(Altar->GetActorLocation()+FVector(800,0,100),false); Prisoner->Capture(nullptr,0);
  Faith.Beliefs.restoreActive({B::Torture}); const int Tortures=D->FaithEventCount(int(E::TorturePerformed)); const float TorturePower=D->FaithPower();
  D->FaithAction(TEXT("Torture"),Prisoner,Altar->GetActorLocation()); TestEqual(TEXT("Torture operates on an actual captive"),D->FaithEventCount(int(E::TorturePerformed)),Tortures+1);
  TestTrue(TEXT("Torture grants the original per-victim power"),FMath::IsNearlyEqual(D->FaithPower()-TorturePower,6.4f,.01f));
  D->FaithAction(TEXT("Torture"),Prisoner,Altar->GetActorLocation()); TestEqual(TEXT("The same captive cannot be spammed for repeated per-victim rewards"),D->FaithEventCount(int(E::TorturePerformed)),Tortures+1);
  Faith.Beliefs.restoreActive({B::Chaos}); const int Imprisoned=D->FaithEventCount(int(E::CultistImprisoned)); D->FaithAction(TEXT("Imprison"),Patient,Patient->GetActorLocation());
  TestTrue(TEXT("Imprisonment immobilizes the actual cultist"),Patient->IsCaptive()); TestEqual(TEXT("Imprisonment feeds the original Chaos penalty"),D->FaithEventCount(int(E::CultistImprisoned)),Imprisoned+1);
  Faith.Beliefs.restoreActive({B::Breeding}); TArray<ACultFeralBeast*> Parents;
  for(int I=0;I<2;++I) { auto* Beast=W->SpawnActor<ACultFeralBeast>(Altar->GetActorLocation()+FVector(2000,I*800,100),FRotator::ZeroRotator,Params); Beast->Capture(nullptr,0); Parents.Add(Beast); }
  const int Births=D->FaithEventCount(int(E::MonstrosityBred)); const FString Birth=D->FaithAction(TEXT("Breed"),Altar,Altar->GetActorLocation());
  TestTrue(TEXT("Two owned captive beasts produce an actual native hybrid"),Birth.Contains(TEXT("was born"))); TestEqual(TEXT("One actual birth feeds Breeding once"),D->FaithEventCount(int(E::MonstrosityBred)),Births+1);
  for(auto* Beast:Parents) Beast->Destroy();
  auto* Priest=Person(Altar->GetActorLocation()+FVector(300,0,100)); auto* Victim=Person(Altar->GetActorLocation()+FVector(500,0,100),false); Victim->Capture();
  Faith.Beliefs.restoreActive({B::Sacrifice}); int Completed=D->FaithEventCount(int(E::SacrificeCompleted));
  TestTrue(TEXT("A sacrifice starts with actual participants"),D->FaithAction(TEXT("Sacrifice"),Victim,Altar->GetActorLocation()).Contains(TEXT("began")));
  FCultFaithTestAccess::Step(D,29); TestEqual(TEXT("Sacrifice waits for its 30-second rite"),D->FaithEventCount(int(E::SacrificeCompleted)),Completed);
  FCultFaithTestAccess::Step(D,1); TestEqual(TEXT("The completed rite reports exactly once"),D->FaithEventCount(int(E::SacrificeCompleted)),Completed+1); TestTrue(TEXT("The actual offered victim dies"),!IsValid(Victim));
  // Exercise the existing source volunteer hook without inventing a player-facing selection rule.
  auto* Donor=Person(Site+FVector(0,1000,0)); auto* Protected=Person(Site+FVector(0,1200,0)); FCultFaithTestAccess::Step(D,0);
  for(auto& Row:State.People) if(Row.Actor==Donor) Faith.Cult.at(Row.Index).setState(cultulhu::CultistState::Converted);
  const int Denied=D->FaithEventCount(int(E::DeathDenied)); Protected->ReceiveAttack(1000);
  TestEqual(TEXT("Source death denial fires exactly once"),D->FaithEventCount(int(E::DeathDenied)),Denied+1);
  TestEqual(TEXT("The real protected cultist revives at one-quarter health"),Protected->GetHealth(),25.f);
  TestTrue(TEXT("The real volunteer dies instead of a phantom soul"),!IsValid(Donor)); FCultFaithTestAccess::Step(D,0);
  TestEqual(TEXT("A native snapshot preserves the denied-death health"),Protected->GetHealth(),25.f);
  auto* Interrupted=Person(Altar->GetActorLocation()+FVector(500,0,100),false); Interrupted->Capture();
  D->FaithAction(TEXT("Sacrifice"),Interrupted,Altar->GetActorLocation()); Interrupted->Destroy(); const int Failed=D->FaithEventCount(int(E::SacrificeInterrupted));
  FCultFaithTestAccess::Step(D,1); TestEqual(TEXT("A missing participant interrupts the rite"),D->FaithEventCount(int(E::SacrificeInterrupted)),Failed+1);
  Faith.Beliefs.restoreActive({B::Magic}); const int Necro=D->FaithEventCount(int(E::NecromancyPerformed));
  TestTrue(TEXT("Necromancy starts at a real altar"),D->FaithAction(TEXT("Necromancy"),nullptr,Altar->GetActorLocation()).Contains(TEXT("began")));
  FCultFaithTestAccess::Step(D,44); TestEqual(TEXT("Necromancy waits 45 seconds"),D->FaithEventCount(int(E::NecromancyPerformed)),Necro); FCultFaithTestAccess::Step(D,1); TestEqual(TEXT("The original necromancy event occurs once"),D->FaithEventCount(int(E::NecromancyPerformed)),Necro+1);
  Faith.Beliefs.restoreActive({B::Reconstruction}); const float Max=Altar->GetMaxHealth(); Altar->ApplyDamage(100,Altar->GetActorLocation(),-1); FCultFaithTestAccess::Step(D,61);
  TestTrue(TEXT("Reconstruction repairs a real surviving owned building"),FMath::IsNearlyEqual(Altar->GetHealth(),Max,.1f));
 }
 auto* Civilian=Person(Site+FVector(0,500,0),false); Faith.Beliefs.restoreActive({B::Onslaught}); const int Slain=D->FaithEventCount(int(E::CivilianSlain)); Civilian->ReceiveAttack(1000,0);
 TestEqual(TEXT("An actual civilian death feeds Onslaught once"),D->FaithEventCount(int(E::CivilianSlain)),Slain+1);
 const int LostBefore=D->FaithEventCount(int(E::CultistLost)); Healer->TransferTo(1); FCultFaithTestAccess::Step(D,0); TestEqual(TEXT("A converted-away follower generates one loss, without a second snapshot penalty"),D->FaithEventCount(int(E::CultistLost)),LostBefore+1); Healer->Desert(); FCultFaithTestAccess::Step(D,0); TestFalse(TEXT("Faith mirrors safely track conversion and desertion"),Healer->IsCultist());
 for(auto& Row:State.People) if(Row.Faction==0&&Row.Actor.IsValid()) State.Faiths[0]->Cult.at(Row.Index).setDevotion(0);
 State.Faiths[0]->Cult.addRisk(100); State.Faiths[0]->WorkTimer=0; const int Revolts=D->FaithEventCount(int(E::Revolt)); FCultFaithTestAccess::Step(D,1);
 TestEqual(TEXT("A source revolt safely removes real followers before construction checks"),D->FaithEventCount(int(E::Revolt)),Revolts+1);
 for(auto* N:Fixtures) if(IsValid(N)) N->Destroy();
 for(TActorIterator<AActor> It(W);It;++It) if(It->ActorHasTag(TEXT("BredHybrid"))) It->Destroy();
 Faith.Beliefs.restoreActive(Saved); for(int I=0;I<3;++I) State.Faiths[I]->WorkTimer=Work[I];
 for(auto& Row:State.People) if(Row.Actor.IsValid()) { Row.Timer=30; Row.RestDue=90; if(Row.Actor->GetActivity()==TEXT("TestHold")) Row.Actor->Order(TEXT("Patrol"),Row.Actor->GetActorLocation(),nullptr,1); }
 FCultFaithTestAccess::Step(D,0); AddInfo(TEXT("Verified source adoption, physical healing/rest/conversions/traps/rituals/repair/deaths, and faction mirror transitions.")); return true;
}
#endif
