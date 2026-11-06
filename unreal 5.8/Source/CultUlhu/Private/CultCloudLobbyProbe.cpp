#if WITH_DEV_AUTOMATION_TESTS
// Explicit opt-in, two processes on this PC. Private API lobby only; never a public test listing.
#include "CultLobbySubsystem.h"
#include "CultUlhuPlayerPawn.h"
#include "CultMatchDirector.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
namespace {
struct FCloudLobbyProbe {
 TWeakObjectPtr<UCultUlhuGameInstance> Game;
 bool Host=false,Started=false,Joined=false,Moved=false,Leaving=false;
 int32 Mode=2;
 double Began=FPlatformTime::Seconds(),JoinedAt=0,LeaveAt=0;
 FVector Initial;
 TWeakObjectPtr<ACultUlhuPlayerPawn> Guest;
 FString File(const TCHAR* Name) const { return FPaths::ProjectSavedDir()/TEXT("Automation/CloudLobby_")+Name; }
 void Signal(const TCHAR* Name,const FString& Text=TEXT("ready")) { FFileHelper::SaveStringToFile(Text,*File(Name)); }
 bool Has(const TCHAR* Name) const { return IFileManager::Get().FileExists(*File(Name)); }
 bool End(bool OK) {
  if(!Host) Signal(TEXT("guest-left")); // Failed joins must also let the host tear down its private test lobby.
  if(auto* G=Game.Get()) if(auto* L=G->GetSubsystem<UCultLobbySubsystem>()) L->LeaveLobby();
  Signal(Host?TEXT("host.json"):TEXT("guest.json"),FString::Printf(TEXT("{\"ok\":%s,\"joined\":%s,\"replicatedMovement\":%s,\"scope\":\"two processes, one Windows PC, private production API lobby\"}"),OK?TEXT("true"):TEXT("false"),Joined?TEXT("true"):TEXT("false"),Moved?TEXT("true"):TEXT("false")));
  FPlatformMisc::RequestExit(false); return false;
 }
 bool Tick(float) {
  auto* G=Game.Get(); auto* L=G?G->GetSubsystem<UCultLobbySubsystem>():nullptr; auto* W=G?G->GetWorld():nullptr;
  if(!G||!L) return false;
  const double Now=FPlatformTime::Seconds(); if(Now-Began>210||L->State==ECultLobbyState::Error) return End(false);
  if(!W||W->bIsTearingDown) return true;
  if(!Started&&W->GetName().Contains(TEXT("L_MainMenu"))) {
   G->SelectedMode=ECultPlayMode(Mode); G->SelectedCharacterId=Host?TEXT("cthulhu_avatar"):TEXT("mi_go");
   if(Host) {
    if(Mode==0) G->SetInitialBeliefs({1,2,3});
    G->FreeRoamSaveSlot=TEXT("Automation_CloudLobby");
    // Disposable descriptor for a same-PC transport test; does not claim to create/join a Radmin network.
    L->ConfigureRadminNetwork(TEXT("CULT-ULHU-LOCAL-PROBE"),FGuid::NewGuid().ToString()); L->HostGame(ECultPlayMode(Mode),true); Started=true;
   } else if(Has(TEXT("invite.txt"))) {
    FString Code; FFileHelper::LoadFileToString(Code,*File(TEXT("invite.txt"))); L->JoinByCode(Code,ECultPlayMode(Mode)); Started=true;
   }
  }
  if(!Host&&L->State==ECultLobbyState::WaitingForRadmin) L->ContinueRadminConnection();
  auto* M=ACultMatchDirector::Find(W);
  if(Host&&L->State==ECultLobbyState::Hosting&&!Has(TEXT("invite.txt"))) { Signal(TEXT("invite.txt"),L->InviteCode); Signal(TEXT("lobby-id.txt"),L->LobbyId); }
  if(Host&&L->State==ECultLobbyState::Hosting) {
   if(!Guest.IsValid()) for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It) if(auto* PC=It->Get()) if(!PC->IsLocalController()) if(auto* P=Cast<ACultUlhuPlayerPawn>(PC->GetPawn())) { Guest=P; Initial=P->GetActorLocation(); Joined=true; }
   if(Guest.IsValid()) Moved|=FVector::Dist2D(Initial,Guest->GetActorLocation())>150;
   if(Has(TEXT("guest-left"))&&!Leaving) { Leaving=true; LeaveAt=Now; }
   if(Leaving&&Now-LeaveAt>5) return End(Joined&&Moved);
  }
  if(!Host&&L->State==ECultLobbyState::Joined) {
   auto* P=Cast<ACultUlhuPlayerPawn>(W->GetFirstPlayerController()?W->GetFirstPlayerController()->GetPawn():nullptr);
   if(!P||(Mode!=0&&(!M||!M->Fighter(P)||M->Fighter(P)->Kit!=TEXT("mi_go")))) return true;
   if(Mode!=0&&M->Warmup()>0) return true;
   if(!Joined) { Joined=true; JoinedAt=Now; Initial=P->GetActorLocation(); }
   if(Now-JoinedAt>4&&Now-JoinedAt<8) P->SetEnhancedMove(FVector2D(1,0));
   if(Now-JoinedAt>=8) P->SetEnhancedMove(FVector2D::ZeroVector);
   Moved|=FVector::Dist2D(Initial,P->GetActorLocation())>150;
   if(Now-JoinedAt>10&&Moved) { Signal(TEXT("guest-left")); G->ReturnToMenu(); Leaving=true; LeaveAt=Now; }
  }
  if(!Host&&Leaving&&Now-LeaveAt>3) return End(Joined&&Moved);
  return true;
 }
};
}
void StartCultCloudLobbyProbe(UCultUlhuGameInstance* G) {
 FString Role; if(!FParse::Value(FCommandLine::Get(),TEXT("CultCloudLobbyProbe="),Role)) return;
 auto Probe=MakeShared<FCloudLobbyProbe>(); Probe->Game=G; Probe->Host=Role.Equals(TEXT("Host"),ESearchCase::IgnoreCase); FParse::Value(FCommandLine::Get(),TEXT("CultCloudLobbyMode="),Probe->Mode); Probe->Mode=FMath::Clamp(Probe->Mode,0,2);
 IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("Automation")),true);
 FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Probe](float Dt){return Probe->Tick(Dt);}),.1f);
}
#endif
