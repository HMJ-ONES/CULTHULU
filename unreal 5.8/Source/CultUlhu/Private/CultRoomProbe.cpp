#if WITH_DEV_AUTOMATION_TESTS
// Opt-in standalone, two-process integration probe. No effect on normal play.
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultUlhuPlayerController.h"
#include "CultMatchDirector.h"
#include "CultMatchUnit.h"
#include "CultPopulation.h"
#include "CultDestructible.h"
#include "CultLivingLandscape.h"
#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "HighResScreenshot.h"
namespace {
struct FRoomProbe {
 TWeakObjectPtr<UCultUlhuGameInstance> Game; bool Host=false; int Mode=0,Phase=0; double Began=FPlatformTime::Seconds(),At=0,LastLog=0;
 TWeakObjectPtr<ACultUlhuPlayerPawn> Guest; TWeakObjectPtr<AActor> Target; TWeakObjectPtr<ACultMatchUnit> Creep;
 FVector Position,CreepStart; float Clock=0; bool Moved=false,WaveMoved=false,Combat=false,LocalMenu=false,HostBot=false;
 FString File(const TCHAR* Suffix) const { return FPaths::ProjectSavedDir()/TEXT("Automation")/FString::Printf(TEXT("Room%d_%s"),Mode,Suffix); }
 void Signal(const TCHAR* Suffix) { FFileHelper::SaveStringToFile(TEXT("ready"),*File(Suffix)); }
 bool Has(const TCHAR* Suffix) const { return IFileManager::Get().FileExists(*File(Suffix)); }
 bool Finish(bool OK,const FString& Reason=FString()) {
  const FString JSON=FString::Printf(TEXT("{\"ok\":%s,\"mode\":%d,\"role\":\"%s\",\"phase\":%d,\"movement\":%s,\"creepsMove\":%s,\"combat\":%s,\"localMenu\":%s,\"hostReplacedByBot\":%s,\"reason\":\"%s\"}"),OK?TEXT("true"):TEXT("false"),Mode,Host?TEXT("host"):TEXT("guest"),Phase,Moved?TEXT("true"):TEXT("false"),WaveMoved?TEXT("true"):TEXT("false"),Combat?TEXT("true"):TEXT("false"),LocalMenu?TEXT("true"):TEXT("false"),HostBot?TEXT("true"):TEXT("false"),*Reason);
  FFileHelper::SaveStringToFile(JSON,*File(Host?TEXT("host.json"):TEXT("guest.json"))); UE_LOG(LogTemp,Display,TEXT("[CultRoomProbe] %s"),*JSON); FPlatformMisc::RequestExit(false); return false;
 }
 bool Tick(float) {
  auto* G=Game.Get(); auto* W=G?G->GetWorld():nullptr; if(!G) return false; const double Now=FPlatformTime::Seconds();
  if(Now-Began>150) return Finish(false,TEXT("timeout")); if(!W||W->bIsTearingDown) return true;
  auto* PC=W->GetFirstPlayerController(); auto* P=PC?Cast<ACultUlhuPlayerPawn>(PC->GetPawn()):nullptr; auto* M=ACultMatchDirector::Find(W);
  if(Now-LastLog>5) { LastLog=Now; UE_LOG(LogTemp,Display,TEXT("[CultRoomProbe] %s phase=%d map=%s net=%d pawn=%s match=%d fighters=%d slot=%d"),Host?TEXT("host"):TEXT("guest"),Phase,*W->GetName(),int(W->GetNetMode()),*GetNameSafe(P),M?int(M->Mode()):-1,M?M->Fighters().Num():0,M?M->Slot(P):-1); }
  if(W->GetName().Contains(TEXT("L_MainMenu"))) {
   if(Host&&Phase==0) { G->OpenModeEntry(ECultPlayMode(Mode)); FScreenshotRequest::RequestScreenshot(File(TEXT("entry.png")),true,false); At=Now; Phase=1; }
   if(Host&&Phase==1&&Now-At>1) { G->BeginRoomAction(Mode==1?ECultRoomAction::Private:ECultRoomAction::Host); G->SelectedCharacterId=TEXT("cthulhu_avatar"); if(!Mode) G->SetInitialBeliefs({1,2,3}); G->FreeRoamSaveSlot=TEXT("Automation_RoomProbe"); G->StartPlay(); Phase=2; }
   if(!Host&&Phase==0&&Has(TEXT("code.txt"))) {
    G->OpenModeEntry(ECultPlayMode(Mode)); if(Mode==2) G->BeginRoomAction(ECultRoomAction::Find);
    else { FString Code; FFileHelper::LoadFileToString(Code,*File(TEXT("code.txt"))); G->BeginRoomAction(ECultRoomAction::Join,Code); }
    Phase=1; At=Now;
   }
   if(!Host&&Phase==1&&G->RoomAction==ECultRoomAction::Join) { G->SelectedCharacterId=TEXT("mi_go"); G->StartPlay(); Phase=2; }
   if(!Host&&Phase==8) return Finish(true);
   return true;
  }
  if(Host&&Phase==2&&!G->RoomCode.IsEmpty()) { FFileHelper::SaveStringToFile(G->RoomCode,*File(TEXT("code.txt"))); Phase=3; }
  if(Mode&&(!M||M->Fighters().Num()!=10)) return true;
  if(!Mode&&!G->FreeRoamReady) return true;
  if(Host) {
   if(Phase==3) {
    for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It) if(auto* C=It->Get()) if(!C->IsLocalController()) Guest=Cast<ACultUlhuPlayerPawn>(C->GetPawn());
    if(!Guest.IsValid()||(Mode&&M->Slot(Guest.Get())<0)||(!Mode&&!Guest->ActorHasTag(TEXT("CoopPlayerReady")))||!Has(TEXT("ready"))||(Mode&&M->Warmup()>0)) return true;
    if(Mode&&Guest->GetFaction()!=1) return Finish(false,TEXT("unbalanced teams"));
    if(Mode) Guest->RelocateMatch(FVector(0,-11000,0));
    FVector Point=Guest->GetActorLocation()+FVector(1800,0,0); FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    if(Mode) { auto* U=W->SpawnActor<ACultMatchUnit>(Point,FRotator::ZeroRotator,Params); U->Team=0; U->Index=999; U->Health=1; U->Dps=U->Speed=0; U->SetOwner(M); Target=U; }
    else { auto* U=W->SpawnActor<ACultPopulation>(Point,FRotator::ZeroRotator,Params); U->Tags.Add(TEXT("RoomProbeTarget")); U->SetActorTickEnabled(false); Target=U; }
    Position=Guest->GetActorLocation(); At=Now; Phase=4;
    if(Mode) { M->TogglePause(); Clock=M->Elapsed(); } else G->ToggleFreeRoamMenu();
   }
   if(Phase==4) {
    Moved|=Guest.IsValid()&&FVector::Dist2D(Position,Guest->GetActorLocation())>150; Combat|=!Target.IsValid();
    LocalMenu=!UGameplayStatics::IsGamePaused(W)&&(!Mode||M->Elapsed()>Clock+1);
    if(Combat&&Moved&&LocalMenu&&Has(TEXT("combat"))) { if(Mode) { G->LeaveCompetitiveMatch(); HostBot=M->Fighters()[0].Pawn.IsValid()&&M->Fighters()[0].Pawn->IsBot()&&!PC->GetPawn(); } else { G->ResumeFreeRoam(); HostBot=true; } Clock=Mode?M->Elapsed():W->GetTimeSeconds(); Signal(TEXT("hostleft")); Phase=5; At=Now; }
   }
   if(Phase==5&&Has(TEXT("left"))&&Now-At>3) {
    int Humans=0; if(Mode) for(const auto& F:M->Fighters()) if(F.Pawn.IsValid()&&!F.Pawn->IsBot()) ++Humans;
    if(Mode&&Humans) return true;
    if(Mode&&M->Elapsed()<=Clock+1) return Finish(false,TEXT("host lobby stopped match"));
    return Finish(Moved&&Combat&&LocalMenu&&HostBot);
   }
  } else {
   if(!P) return true;
   if(Phase==2) {
    if(Mode&&(!M->Fighter(P)||M->Fighter(P)->Kit!=TEXT("mi_go")||P->GetFaction()!=1)) return true;
    if(!Mode) { int Buildings=0; for(TActorIterator<ACultDestructible> It(W);It;++It) if(It->GetHealth()>0) ++Buildings; if(Buildings<20||!ACultLivingLandscape::Find(W)) return true; }
    Signal(TEXT("ready")); Phase=3;
   }
   if(Phase==3) {
    if(Mode) { for(TActorIterator<ACultMatchUnit> It(W);It;++It) if(It->Index==999) Target=*It; }
    else { for(TActorIterator<ACultPopulation> It(W);It;++It) if(It->ActorHasTag(TEXT("RoomProbeTarget"))) Target=*It; }
    if(!Target.IsValid()) return true;
    P->ServerCombatInput(uint8(ECultPlayerInput::Primary),true,P->GetActorLocation(),(Target->GetActorLocation()-P->GetActorLocation()).GetSafeNormal());
    P->ServerCombatInput(uint8(ECultPlayerInput::Primary),false,P->GetActorLocation(),(Target->GetActorLocation()-P->GetActorLocation()).GetSafeNormal());
    At=Now; Phase=4;
   }
   if(Phase==4&&Target.IsValid()&&Now-At>1) {
    UE_LOG(LogTemp,Display,TEXT("[CultRoomProbe] attack pawn=%s target=%s distance=%.0f canAct=%d"),*P->GetActorLocation().ToCompactString(),*Target->GetActorLocation().ToCompactString(),FVector::Dist(P->GetActorLocation(),Target->GetActorLocation()),M?int(M->CanAct(P)):1);
    P->ServerCombatInput(uint8(ECultPlayerInput::Primary),true,P->GetActorLocation(),(Target->GetActorLocation()-P->GetActorLocation()).GetSafeNormal()); P->ServerCombatInput(uint8(ECultPlayerInput::Primary),false,P->GetActorLocation(),(Target->GetActorLocation()-P->GetActorLocation()).GetSafeNormal()); At=Now;
   }
   if(Phase==4&&!Target.IsValid()) { Combat=true; Position=P->GetActorLocation(); P->TryDash(FVector::RightVector); Phase=5; At=Now; }
   if(Phase==5) {
    Moved|=FVector::Dist2D(Position,P->GetActorLocation())>150;
    if(Mode==2) {
     if(!Creep.IsValid()) for(TActorIterator<ACultMatchUnit> It(W);It;++It) if(!It->IsStructure()&&It->Index!=999) { Creep=*It; CreepStart=It->GetActorLocation(); break; }
     if(Creep.IsValid()) WaveMoved|=FVector::Dist2D(CreepStart,Creep->GetActorLocation())>500;
    } else WaveMoved=true;
    if(Moved&&WaveMoved) { if(Mode) { PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::Escape,IE_Pressed,1.f,false,FPlatformTime::Cycles64())); Clock=M->Elapsed(); } else { G->ToggleFreeRoamMenu(); Clock=W->GetTimeSeconds(); } At=Now; Phase=6; }
   }
   if(Phase==6&&Now-At>2) {
    LocalMenu=!UGameplayStatics::IsGamePaused(W)&&(Mode?M->IsPaused()&&M->Elapsed()>Clock+1:W->GetTimeSeconds()>Clock+1);
    if(!LocalMenu) return Finish(false,TEXT("menu stopped guest clock"));
    FScreenshotRequest::RequestScreenshot(File(TEXT("menu.png")),true,false); Signal(TEXT("combat")); Phase=7;
   }
   if(Phase==7&&Has(TEXT("hostleft"))) {
    HostBot=!Mode||(M->Fighters()[0].Pawn.IsValid()&&M->Fighters()[0].Pawn->IsBot()); if(!HostBot) return true;
    Signal(TEXT("left")); if(Mode) G->LeaveCompetitiveMatch(); else G->ReturnToMenu(); Phase=8;
   }
  }
  return true;
 }
};
}
void StartCultRoomProbe(UCultUlhuGameInstance* G) {
 FString Role; if(!FParse::Value(FCommandLine::Get(),TEXT("CultRoomProbe="),Role)) return;
 auto Probe=MakeShared<FRoomProbe>(); Probe->Game=G; Probe->Host=Role.Equals(TEXT("Host"),ESearchCase::IgnoreCase); FParse::Value(FCommandLine::Get(),TEXT("CultRoomMode="),Probe->Mode);
 IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("Automation")),true);
 FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Probe](float Dt){return Probe->Tick(Dt);}),.1f);
}
#endif
