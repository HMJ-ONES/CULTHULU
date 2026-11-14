#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "CultAutomationPIE.h"
#include "CultWardWarning.h"
#include "CultSkillAttack.h"
#include "CultMatchUnit.h"
#include "CultMatchDirector.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
class FCultWardWarningObservation:public IAutomationLatentCommand {
 FAutomationTestBase* Test; int Phase=0; double Began=FPlatformTime::Seconds(); float At=0;
 TWeakObjectPtr<ACultSkillAttack> Shot;
public:
 explicit FCultWardWarningObservation(FAutomationTestBase* T):Test(T) {}
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>90) { Test->AddError(TEXT("Ward warning fixture timed out")); return true; }
  auto* W=GEditor->PlayWorld.Get(); if(!W||W->bIsTearingDown) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) {
   if(!Phase) { auto* G=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); G->SelectedCharacterId=TEXT("cthulhu_avatar"); G->OpenCharacterSelect(ECultPlayMode::Dotgoo); G->OpenBeliefSelect(); Phase=1; } return false;
  }
  auto* M=ACultMatchDirector::Find(W); auto* PC=W->GetFirstPlayerController(); auto* P=PC?Cast<ACultUlhuPlayerPawn>(PC->GetPawn()):nullptr;
  if(!M||!P||W->GetTimeSeconds()<1) return false;
  if(Phase==1) {
   M->Tick(20); M->Tick(4); M->SetActorTickEnabled(false);
   for(TActorIterator<ACultSkillAttack> It(W);It;++It) It->Destroy();
   for(TActorIterator<ACultMatchUnit> It(W);It;++It) It->SetActorTickEnabled(false);
   Test->TestFalse(TEXT("No warning without a real shot"),CultWardWarning::Observe(P).IsVisible());
   const FTransform T(P->GetActorLocation()+FVector(2500,800,0));
   auto* Ward=W->SpawnActorDeferred<ACultMatchUnit>(ACultMatchUnit::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
   Ward->Kind=3; Ward->Team=1-P->GetFaction(); Ward->FinishSpawning(T); Ward->SetActorTickEnabled(false);
   auto* A=W->SpawnActorDeferred<ACultSkillAttack>(ACultSkillAttack::StaticClass(),T,Ward,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
   A->WardLocked=true; A->WardTarget=P; A->Windup=10; A->Duration=10; A->FinishSpawning(T); A->SetActorTickEnabled(false);
   auto Signal=CultWardWarning::Observe(P);
   Test->TestTrue(TEXT("Hostile lock warns the actual target during charge"),Signal.Shot.Get()==A&&!Signal.Incoming&&Signal.ReleaseIn>9);
   Test->TestTrue(TEXT("Direction uses the real ward"),Signal.Source.Equals(Ward->GetActorLocation()));
   A->WardTarget=Ward; Test->TestFalse(TEXT("Other targets do not warn this player"),CultWardWarning::Observe(P).IsVisible()); A->WardTarget=P;
   Ward->Team=P->GetFaction(); Test->TestFalse(TEXT("Friendly shot is suppressed"),CultWardWarning::Observe(P).IsVisible()); Ward->Team=1-P->GetFaction();
   Ward->Kind=1; Test->TestFalse(TEXT("Ordinary skill shots do not masquerade as tower locks"),CultWardWarning::Observe(P).IsVisible()); Ward->Kind=3;
   Ward->Health=0; Test->TestFalse(TEXT("Dead ward warning clears"),CultWardWarning::Observe(P).IsVisible()); Ward->Health=Ward->MaxHealth;
   A->WardChargeRemaining=0; Signal=CultWardWarning::Observe(P);
   Test->TestTrue(TEXT("Release switches to incoming without restarting client windup"),Signal.Incoming&&Signal.ReleaseIn==0);
   for(FVector2D Size:{FVector2D(640,360),FVector2D(960,540),FVector2D(1280,720),FVector2D(1920,720)}) {
    const float S=FMath::Clamp(FMath::Min(Size.X/1600.f,Size.Y/900.f),.55f,2.f);
    const auto Box=CultWardWarning::Bounds(Size,S,FVector2D(240*S,34*S));
    Test->TestTrue(TEXT("Panel stays on canvas and above target health"),Box.Min.X>=0&&Box.Min.Y>=0&&Box.Max.X<=Size.X&&Box.Max.Y<Size.Y*.5f-75*S);
   }
   A->Destroy(); Test->TestFalse(TEXT("Destroyed shot clears immediately"),CultWardWarning::Observe(P).IsVisible());
   Ward->FireAt(P);
   for(TActorIterator<ACultSkillAttack> It(W);It;++It) if(It->WardTarget.Get()==P) { Shot=*It; It->WardChargeRemaining=10; It->SetActorTickEnabled(false); }
   Test->TestTrue(TEXT("Actual FireAt generates warning metadata"),CultWardWarning::Observe(P).IsVisible());
   At=W->GetTimeSeconds(); Phase=2; return false;
  }
  if(Phase==2&&W->GetTimeSeconds()-At>.2f) { FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/WardWarning.png"),false,false); At=W->GetTimeSeconds(); Phase=3; return false; }
  if(Phase==3&&W->GetTimeSeconds()-At>.5f) { if(Shot.IsValid()) Shot->Destroy(); return true; }
  return false;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultWardWarningTest,"CultUlhu.Menu.WardWarning",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultWardWarningTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultWardWarningObservation(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
#endif
