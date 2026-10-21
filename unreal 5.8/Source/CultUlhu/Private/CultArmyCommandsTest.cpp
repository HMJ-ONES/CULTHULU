#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CultWorldState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "UI/CultBeliefSelectWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "CultPlayHUD.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#if WITH_EDITOR
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#endif

struct FCultArmyTestAccess {
 struct Snapshot { TWeakObjectPtr<ACultPopulation> Actor; FString Action; FVector Point; TWeakObjectPtr<AActor> Target; float Time; bool Directive,Battle; };
 static TArray<Snapshot> SaveOrders(UWorld* W) {
  TArray<Snapshot> Out; for(TActorIterator<ACultPopulation> It(W);It;++It) Out.Add({*It,It->Activity_,It->OrderPoint_,It->OrderTarget_,It->OrderTime_,It->bDirective_,It->bPlayerBattle_}); return Out;
 }
 static void RestoreOrders(const TArray<Snapshot>& Saved) {
  for(const auto& S:Saved) if(S.Actor.IsValid()) { auto* P=S.Actor.Get(); P->bPlayerBattle_=false; P->Order(S.Action,S.Point,S.Target.Get(),S.Time,S.Directive); P->bPlayerBattle_=S.Battle; }
 }
 static TArray<bool> SaveWars(ACultWorldDirector* D) { TArray<bool> Out; for(int A=0;A<3;++A) for(int B=0;B<3;++B) Out.Add(A!=B&&D->AtWar(A,B)); return Out; }
 static void RestoreWars(ACultWorldDirector* D,const TArray<bool>& Saved) { for(int A=0;A<3;++A) for(int B=0;B<3;++B) if(A!=B) { if(Saved[A*3+B]) D->State_->Faiths[A]->Wars.declareWar(B+1); else D->State_->Faiths[A]->Wars.makePeace(B+1); } }
 static void RechargeConversion(ACultWorldDirector* D) { D->State_->Faiths[0]->Clock.advance(cultulhu::BeliefSystem::MASS_CONVERT_COOLDOWN); }
 static FString ConstructionReport(ACultWorldDirector* D) {
  FString Out; for(auto& B:D->State_->Builds) if(B.Site&&!B.Site->finished()) {
   Out+=FString::Printf(TEXT("Site faction %d progress %.2f point %s: "),B.Faction,B.Site->progress(),*B.Point.ToString());
   for(auto& W:B.Workers) if(W.IsValid()) Out+=FString::Printf(TEXT("[%s distance %.0f loyalty %.0f site mismatch %.0f] "),*W->GetActivity(),FVector::Dist2D(W->GetActorLocation(),B.Point),W->GetLoyalty(),FVector::Dist(W->GetCommandPoint(),B.Point));
  } return Out;
 }
 static void ReadySermon(ACultWorldDirector* D) { D->State_->NextPreach=0; }
 static AActor* Target(ACultPopulation* P) { return P->OrderTarget_.Get(); }
 static float Duration(ACultPopulation* P) { return P->OrderTime_; }
};
FString CultTestConstructionReport(ACultWorldDirector* D) { return FCultArmyTestAccess::ConstructionReport(D); }
class FCultArmyObservation:public IAutomationLatentCommand {
public:
 explicit FCultArmyObservation(FAutomationTestBase* T):Test(T),Started(FPlatformTime::Seconds()) {}
 bool Update() override {
  UWorld* W=nullptr; for(auto& C:GEngine->GetWorldContexts()) if(C.World()&&C.World()->IsGameWorld()) W=C.World();
  if(!W) { if(FPlatformTime::Seconds()-Started>30) { Test->AddError(TEXT("No game world for army commands")); return true; } return false; }
  auto* GI=Cast<UCultUlhuGameInstance>(W->GetGameInstance());
  if(W->GetName().Contains(TEXT("L_MainMenu"))) {
   if(!Entered) {
    GI->OpenCharacterSelect(ECultPlayMode::FreeRoam); GI->StartPlay();
    TArray<UUserWidget*> Screens; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Screens,UCultBeliefSelectWidget::StaticClass(),true);
    Test->TestEqual(TEXT("Entering without beliefs always opens the required picker"),Screens.Num(),1);
    if(Screens.Num()) {
     auto* Picker=Cast<UCultBeliefSelectWidget>(Screens[0]);
     Test->TestFalse(TEXT("Empty choices cannot enter"),Picker->CanEnter());
     Picker->ToggleChoice(5); Picker->ToggleChoice(6); Picker->ToggleChoice(11); Picker->ToggleChoice(1);
     Test->TestEqual(TEXT("Picker allows exactly three choices"),Picker->Choices().Num(),3);
     Test->TestTrue(TEXT("Three beliefs enable entering"),Picker->CanEnter());
    }
    Test->TestFalse(TEXT("Duplicate beliefs are rejected"),GI->SetInitialBeliefs({5,5,6}));
    Test->TestFalse(TEXT("Out of range beliefs are rejected"),GI->SetInitialBeliefs({5,6,12}));
    GI->SetInitialBeliefs({5,6,11}); GI->StartPlay(); Entered=true;
   }
   if(FPlatformTime::Seconds()-Started>40) { Test->AddError(TEXT("Army test did not leave the menu")); return true; } return false;
  }
  auto* D=ACultWorldDirector::Find(W); auto* P=Cast<ACultUlhuPlayerPawn>(UGameplayStatics::GetPlayerPawn(W,0));
  if(!D||!P||W->GetTimeSeconds()<2) return false;
  if(Executed) {
   if(W->GetTimeSeconds()-ShotAt<2) return false;
   if(!Captured) { FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots/ArmyCommands.png")),true,false); Captured=true; ShotAt=W->GetTimeSeconds(); return false; }
   P->AltReleased(); return true;
  }
  using A=cultulhu::MenuAction; using B=cultulhu::Belief;
  if(Entered) { Test->TestTrue(TEXT("Chosen initial Conversion is active"),D->Beliefs().isActive(B::Conversion)); Test->TestTrue(TEXT("Chosen initial War is active"),D->Beliefs().isActive(B::War)); Test->TestTrue(TEXT("Chosen initial Dreams is active"),D->Beliefs().isActive(B::Dreams)); Test->TestEqual(TEXT("Exactly three initial beliefs reached gameplay"),int(D->Beliefs().active().size()),3); }
  const auto SavedWars=FCultArmyTestAccess::SaveWars(D); const auto SavedOrders=FCultArmyTestAccess::SaveOrders(W); const auto SavedBeliefs=D->Beliefs().active();
  // Keep this isolated fixture above every procedural hill and structure.
  const FVector Origin=P->FeetLocation()+FVector(16000,-14000,100000);
  const FVector OldLocation=P->GetActorLocation(); P->SetActorLocation(Origin); P->GetCharacterMovement()->DisableMovement();
  FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  TArray<ACultPopulation*> Units;
  for(int I=0;I<12;++I) { auto* N=W->SpawnActor<ACultPopulation>(Origin+FVector(1000+I*120,800,0),FRotator::ZeroRotator,Params); N->RecruitTo(0); N->SetLoyalty(0,false); N->GetCharacterMovement()->DisableMovement(); Units.Add(N); }
  auto* Enemy=W->SpawnActor<ACultPopulation>(Origin+FVector(2500,0,0),FRotator::ZeroRotator,Params); Enemy->RecruitTo(1); Enemy->GetCharacterMovement()->DisableMovement();
  D->SelectFollowers({Units[0],Units[1]}); D->Execute(ACultWorldDirector::PrepareBattle,nullptr,Origin);
  Test->TestEqual(TEXT("Two selected cultists are prepared"),D->PreparedCount(),2);
  D->Execute(int(A::AttackTarget),Enemy,Enemy->GetActorLocation());
  Test->TestTrue(TEXT("Zero-loyalty cultists obey combat"),Units[0]->HasPlayerBattle()&&Units[1]->HasPlayerBattle());
  Test->TestFalse(TEXT("Unselected followers were not sent"),Units[2]->HasPlayerBattle());
  Units[0]->Order(TEXT("Pray"),Origin,nullptr,1); Test->TestEqual(TEXT("Autonomous jobs cannot steal an assigned fighter"),Units[0]->GetActivity(),FString(TEXT("Attack")));
  Test->TestTrue(TEXT("Battle assignment survives the old one-minute timeout"),FCultArmyTestAccess::Duration(Units[0])>1000000);
  for(int I=2;I<12;++I) {
   D->SelectFollowers({Units[I]}); D->Execute(ACultWorldDirector::PrepareBattle,nullptr,Origin);
   const FVector Front=Origin+FVector(I*1000,5000,0); D->Execute(int(A::RaidAt),nullptr,Front);
   Test->TestTrue(TEXT("Independent fronts are not capped at four operations"),Units[I]->HasPlayerBattle()&&Units[I]->GetCommandPoint().Equals(Front));
  }
  Test->TestTrue(TEXT("First group still has its original enemy"),FCultArmyTestAccess::Target(Units[0])==Enemy);
  D->Execute(ACultWorldDirector::PrepareBattle,nullptr,Origin); Test->TestFalse(TEXT("Preparation toggles off"),D->BattlePrepared());
  D->Execute(int(A::AttackTarget),Enemy,Enemy->GetActorLocation());
  Test->TestEqual(TEXT("General attack preserves another front"),Units[4]->GetActivity(),FString(TEXT("Raid")));
  D->SelectFollowers({Units[0]}); D->Execute(ACultWorldDirector::DisengageBattle,nullptr,Origin);
  Test->TestFalse(TEXT("Selected group can disengage independently"),Units[0]->HasPlayerBattle());
  Test->TestTrue(TEXT("Disengaging one group leaves the other fighting"),Units[1]->HasPlayerBattle());
  // A real ground site, but an explicitly disloyal selected worker must refuse construction.
  FHitResult H; FCollisionQueryParams Q;
  if(W->LineTraceSingleByObjectType(H,OldLocation+FVector(0,0,5000),OldLocation-FVector(0,0,20000),FCollisionObjectQueryParams(ECC_WorldStatic),Q)) {
   const FString Result=D->Execute(int(A::BuildAt),nullptr,H.ImpactPoint);
   Test->TestTrue(TEXT("Build refusal visibly identifies disobeying cultists"),Result.Contains(TEXT("disobeying")));
  } else Test->AddError(TEXT("No terrain available for construction refusal test"));
  D->Execute(ACultWorldDirector::GlobalRetreat,nullptr,Origin);
  for(auto* N:Units) { Test->TestFalse(TEXT("Global retreat releases every front"),N->HasPlayerBattle()); Test->TestEqual(TEXT("Retreat returns followers to their entity"),N->GetActivity(),FString(TEXT("Retreat"))); }
  D->Beliefs().restoreActive({B::Conversion,B::War,B::Dreams}); FCultArmyTestAccess::RechargeConversion(D);
  TArray<ACultPopulation*> Audience;
  for(int I=0;I<8;++I) { auto* N=W->SpawnActor<ACultPopulation>(P->FeetLocation()+FVector(1200+I*130,-500,350),FRotator::ZeroRotator,Params); N->GetCharacterMovement()->DisableMovement(); Audience.Add(N); }
  int Before=D->FaithEventCount(int(cultulhu::EventType::ConversionPerformed));
  const FString Sermon=D->Execute(ACultWorldDirector::Preach,nullptr,FVector::ZeroVector);
  int Joined=0; for(auto* N:Audience) Joined+=N->GetFaction()==0;
  Test->TestEqual(TEXT("Ready Conversion recruits exactly three LOCAL living civilians"),Joined,3);
  Test->TestEqual(TEXT("Mass sermon publishes one canonical conversion receipt"),D->FaithEventCount(int(cultulhu::EventType::ConversionPerformed)),Before+1);
  Test->TestFalse(TEXT("Mass conversion is now on its source cooldown"),D->Beliefs().canMassConvert());
  D->Execute(ACultWorldDirector::Preach,nullptr,Origin); int Again=0; for(auto* N:Audience) Again+=N->GetFaction()==0;
  Test->TestEqual(TEXT("Repeated clicks cannot bypass preaching cooldown"),Again,Joined);
  FCultArmyTestAccess::ReadySermon(D); D->Execute(ACultWorldDirector::Preach,nullptr,Origin); Again=0; for(auto* N:Audience) Again+=N->GetFaction()==0;
  Test->TestTrue(TEXT("An ordinary sermon recruits at most one civilian"),Again-Joined<=1);
  for(auto* N:Audience) N->Destroy(); for(auto* N:Units) N->Destroy(); Enemy->Destroy();
  FCultArmyTestAccess::RestoreWars(D,SavedWars); FCultArmyTestAccess::RestoreOrders(SavedOrders); D->Beliefs().restoreActive(SavedBeliefs); FCultArmyTestAccess::RechargeConversion(D); D->SelectFollowers({});
  P->SetActorLocation(OldLocation); P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
  P->AltPressed(); if(auto* HUD=Cast<ACultPlayHUD>(W->GetFirstPlayerController()->GetHUD())) { HUD->CommandClick(); HUD->CommandRelease(); }
  Test->AddInfo(Sermon); Executed=true; ShotAt=W->GetTimeSeconds(); return false;
 }
private:
 FAutomationTestBase* Test; double Started; bool Entered=false,Executed=false,Captured=false; float ShotAt=0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultArmyCommandsTest,"CultUlhu.ArmyCommands",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCultArmyCommandsTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultArmyObservation(this)); return true; }
#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultArmyCommandsEditorTest,"CultUlhu.Menu.ArmyCommands",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultArmyCommandsEditorTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false)); ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1)); ADD_LATENT_AUTOMATION_COMMAND(FCultArmyObservation(this)); return true; }
#endif
#endif
