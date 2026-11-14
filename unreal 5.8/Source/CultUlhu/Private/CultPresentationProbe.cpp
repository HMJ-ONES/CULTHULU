#if WITH_DEV_AUTOMATION_TESTS
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultMatchDirector.h"
#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/OutputDevice.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"

namespace {
// Measures the actual standalone menu/travel/initial-possession path, including
// errors occurring before the match director performs its first relocation.
struct FPresentationProbe:public FOutputDevice {
 TWeakObjectPtr<UCultUlhuGameInstance> Game;
 int32 Case=0,Phase=0,SpawnErrors=0,CaseErrors=0; float At=0;
 double Began=FPlatformTime::Seconds(); FVector Position;
 TArray<FString> Rows; bool Passed=true;
 FPresentationProbe() { if(GLog) GLog->AddOutputDevice(this); }
 ~FPresentationProbe() { if(GLog) GLog->RemoveOutputDevice(this); }
 virtual void Serialize(const TCHAR* Text,ELogVerbosity::Type Verbosity,const FName& Category) override {
  if(Verbosity<=ELogVerbosity::Error&&FString(Text).Contains(TEXT("No clear walkable spawn"))) ++SpawnErrors;
 }
 bool Finish() {
  const FString File=FPaths::ProjectSavedDir()/TEXT("Automation/PresentationSafety.json");
  IFileManager::Get().MakeDirectory(*FPaths::GetPath(File),true);
  FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"ok\":%s,\"cases\":[%s]}"),Passed?TEXT("true"):TEXT("false"),*FString::Join(Rows,TEXT(","))),*File);
  UE_LOG(LogTemp,Display,TEXT("[CultPresentationProbe] completed ok=%d modes=%d spawnErrors=%d"),int32(Passed),Rows.Num(),SpawnErrors);
  FPlatformMisc::RequestExitWithStatus(false,Passed?0:1); return false;
 }
 bool Tick(float) {
  auto* G=Game.Get(); if(!G) return false;
  if(FPlatformTime::Seconds()-Began>140) { Passed=false; return Finish(); }
  auto* W=G->GetWorld(); if(!W) return true;
  const ECultPlayMode Modes[]={ECultPlayMode::Domination,ECultPlayMode::Dotgoo,ECultPlayMode::FreeRoam};
  if(W->GetName().Contains(TEXT("L_MainMenu"))) {
   if(!Phase) {
    CaseErrors=SpawnErrors; G->OpenCharacterSelect(Modes[Case]); G->SelectedCharacterId=TEXT("cthulhu_avatar");
    if(Modes[Case]==ECultPlayMode::FreeRoam) G->SetInitialBeliefs({1,2,3});
    G->StartPlay(); Phase=1;
   }
   return true;
  }
  auto* PC=W->GetFirstPlayerController(); auto* P=PC?Cast<ACultUlhuPlayerPawn>(PC->GetPawn()):nullptr;
  if(!P||W->GetTimeSeconds()<.6f) return true;
  if(auto* Match=ACultMatchDirector::Find(W)) { if(Match->Warmup()>0) return true; }
  else if(!G->FreeRoamReady) return true;
  if(Phase==1) {
   Position=P->GetActorLocation(); At=W->GetTimeSeconds();
   PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::W,IE_Pressed,1.f,false,FPlatformTime::Cycles64()));
   Phase=2; return true;
  }
  if(Phase==2&&W->GetTimeSeconds()-At>.35f) {
   PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::W,IE_Released,0.f,false,FPlatformTime::Cycles64()));
   auto* Movement=P->GetCharacterMovement();
   const bool Walking=Movement->MovementMode==MOVE_Walking&&Movement->CurrentFloor.IsWalkableFloor();
   const bool Moved=FVector::Dist2D(Position,P->GetActorLocation())>50;
   const bool Focused=!PC->bShowMouseCursor&&!PC->IsMoveInputIgnored()&&!PC->IsLookInputIgnored();
   const bool OK=Walking&&Moved&&Focused&&SpawnErrors==CaseErrors; Passed=Passed&&OK;
   const TCHAR* Name=Case==0?TEXT("Domination"):Case==1?TEXT("Dotgoo"):TEXT("FreeRoam");
   Rows.Add(FString::Printf(TEXT("{\"mode\":\"%s\",\"ok\":%s,\"walking\":%s,\"movement_input\":%s,\"focused\":%s,\"initial_spawn_errors\":%d}"),Name,OK?TEXT("true"):TEXT("false"),Walking?TEXT("true"):TEXT("false"),Moved?TEXT("true"):TEXT("false"),Focused?TEXT("true"):TEXT("false"),SpawnErrors-CaseErrors));
   UE_LOG(LogTemp,Display,TEXT("[CultPresentationProbe] %s"),*Rows.Last());
   if(++Case==3) return Finish(); Phase=0; G->ReturnToMenu();
  }
  return true;
 }
};
}
void StartCultPresentationProbe(UCultUlhuGameInstance* Game) {
 if(!FParse::Param(FCommandLine::Get(),TEXT("CultPresentationProbe"))) return;
 Game->FreeRoamSaveSlot=TEXT("Automation_PresentationSafety");
 auto Probe=MakeShared<FPresentationProbe>(); Probe->Game=Game;
 FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Probe](float Dt) { return Probe->Tick(Dt); }),.05f);
}
#endif
