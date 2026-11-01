#if WITH_DEV_AUTOMATION_TESTS
// Explicit command-line test harness. It is dormant during ordinary play.
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultPopulation.h"
#include "CultDestructible.h"
#include "CultLivingLandscape.h"
#include "CultWorldDirector.h"
#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "EngineUtils.h"
#include "Engine/NetDriver.h"
#include "Engine/NetConnection.h"
#include "Kismet/GameplayStatics.h"

namespace {
struct FCoopProbe {
 TWeakObjectPtr<UCultUlhuGameInstance> Game; bool Host=false; int Phase=0;
 double Start=FPlatformTime::Seconds(),At=0,LastReport=0; TWeakObjectPtr<ACultPopulation> Target;
 TWeakObjectPtr<ACultUlhuPlayerPawn> Guest; FVector Position;
 void Proof(const FString& JSON) {
  FString File=FPaths::ProjectSavedDir()/TEXT("Automation")/(Host?TEXT("CoopHostProof.json"):TEXT("CoopClientProof.json"));
  IFileManager::Get().MakeDirectory(*FPaths::GetPath(File),true); FFileHelper::SaveStringToFile(JSON,*File);
  UE_LOG(LogTemp,Display,TEXT("[CultCoopProbe] %s"),*JSON);
 }
 bool Tick(float) {
  auto* G=Game.Get(); auto* W=G?G->GetWorld():nullptr; if(!G) return false;
  double Now=FPlatformTime::Seconds(); if(Now-Start>180) { Proof(TEXT("{\"ok\":false,\"reason\":\"timeout\"}")); return false; }
  if(!W||!W->GetName().Contains(TEXT("L_DarkWilds"))) return true;
  if(Now-LastReport>5) {
   LastReport=Now; int32 Buildings=0,People=0,Targets=0,Players=0;
   for(TActorIterator<ACultDestructible> It(W);It;++It) ++Buildings;
   if(Host) for(TActorIterator<ACultDestructible> It(W);It;++It) if(!It->ActorHasTag(TEXT("CultLegacyGeometry"))) { UE_LOG(LogTemp,Display,TEXT("[CultCoopProbe] building=%s replicated=%d remoteRole=%d hidden=%d location=%s"),*It->GetName(),int32(It->GetIsReplicated()),int32(It->GetRemoteRole()),int32(It->IsHidden()),*It->GetActorLocation().ToCompactString()); break; }
   for(TActorIterator<ACultPopulation> It(W);It;++It) { ++People; if(It->ActorHasTag(TEXT("CoopProbeTarget"))) ++Targets; }
   for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It) ++Players;
   if(auto* Driver=W->GetNetDriver()) {
    if(Host&&!Driver->ClientConnections.IsEmpty()) if(auto* Connection=Driver->ClientConnections[0].Get()) {
     UE_LOG(LogTemp,Display,TEXT("[CultCoopProbe] bandwidth queued=%d in=%d out=%d"),Connection->QueuedBits,Connection->InBytesPerSecond,Connection->OutBytesPerSecond);
     for(TActorIterator<ACultDestructible> It(W);It;++It) if(!It->ActorHasTag(TEXT("CultLegacyGeometry"))) { UE_LOG(LogTemp,Display,TEXT("[CultCoopProbe] building registered=%d channel=%d"),int32(Driver->FindNetworkObjectInfo(*It)!=nullptr),int32(Connection->FindActorChannelRef(*It)!=nullptr)); break; }
    }
   }
   UE_LOG(LogTemp,Display,TEXT("[CultCoopProbe] %s phase=%d ready=%d buildings=%d people=%d targets=%d controllers=%d pawn=%s"),Host?TEXT("host"):TEXT("client"),Phase,int32(G->FreeRoamReady),Buildings,People,Targets,Players,*GetNameSafe(UGameplayStatics::GetPlayerPawn(W,0)));
  }
  if(Host) {
   if(Phase==0&&G->FreeRoamReady) {
    for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It) if(auto* PC=It->Get()) if(!PC->IsLocalController()) Guest=Cast<ACultUlhuPlayerPawn>(PC->GetPawn());
    if(!Guest.IsValid()||!Guest->ActorHasTag(TEXT("CoopPlayerReady"))) return true;
    FVector Ground; if(!ACultLivingLandscape::Surface(W,Guest->FeetLocation()+FVector(1500,0,0),Ground)) return true;
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Target=W->SpawnActor<ACultPopulation>(Ground+FVector(0,0,88),FRotator::ZeroRotator,Params); Target->Tags.Add(TEXT("CoopProbeTarget")); Target->SetActorTickEnabled(false);
    Position=Guest->GetActorLocation(); At=Now; Phase=1;
   }
   if(Phase==1&&!Target.IsValid()&&Guest.IsValid()&&FVector::Dist2D(Position,Guest->GetActorLocation())>100&&Now-At>3) {
    bool Saved=G->SaveFreeRoam();
    Proof(FString::Printf(TEXT("{\"ok\":%s,\"guestConnected\":true,\"serverConfirmedMeleeKill\":true,\"serverConfirmedMovement\":true,\"worldSaved\":%s,\"seed\":%d}"),Saved?TEXT("true"):TEXT("false"),Saved?TEXT("true"):TEXT("false"),G->FreeRoamSeed)); return false;
   }
  } else {
   auto* P=Cast<ACultUlhuPlayerPawn>(UGameplayStatics::GetPlayerPawn(W,0)); auto* Land=ACultLivingLandscape::Find(W);
   if(!P||!Land||!G->FreeRoamReady) return true;
   if(Phase==0) {
    for(TActorIterator<ACultPopulation> It(W);It;++It) if(It->ActorHasTag(TEXT("CoopProbeTarget"))) Target=*It;
    if(!Target.IsValid()) return true;
    int32 Buildings=0,People=0; for(TActorIterator<ACultDestructible> It(W);It;++It) if(It->GetHealth()>0) ++Buildings;
    for(TActorIterator<ACultPopulation> It(W);It;++It) ++People;
    if(Buildings<20||People<10) return true;
    auto* Director=ACultWorldDirector::Find(W); if(!Director) return true;
    if(Director->RequestBelief(0,1)||!Director->FaithAction(TEXT("Pray"),nullptr,P->FeetLocation()).StartsWith(TEXT("The host"))) { Proof(TEXT("{\"ok\":false,\"reason\":\"guest mutated host faith\"}")); return false; }
    FVector Origin=P->GetActorLocation(),Direction=(Target->GetActorLocation()-Origin).GetSafeNormal();
    P->ServerCombatInput(uint8(ECultPlayerInput::Primary),true,Origin,Direction);
    P->ServerCombatInput(uint8(ECultPlayerInput::Primary),false,Origin,Direction);
    Phase=1; At=Now;
   }
   if(Phase==1&&!Target.IsValid()) { P->TryDash(FVector::RightVector); Position=P->GetActorLocation(); At=Now; Phase=2; }
   if(Phase==2&&Now-At>3&&FVector::Dist2D(Position,P->GetActorLocation())>100) {
    Proof(FString::Printf(TEXT("{\"ok\":true,\"replicatedBuildings\":true,\"replicatedPopulation\":true,\"observedServerDeath\":true,\"dashMovement\":true,\"sharedFaithAuthoritative\":true,\"seed\":%d}"),Land->WorldSeed)); return false;
   }
  }
  return true;
 }
};
}
void StartCultCoopProbe(UCultUlhuGameInstance* G) {
 FString Role; if(!FParse::Value(FCommandLine::Get(),TEXT("CultCoopProbe="),Role)) return;
 G->bPlayChosen=true; G->SelectedMode=ECultPlayMode::FreeRoam; G->FreeRoamSeed=314159; G->SetInitialBeliefs({1,2,3}); G->FreeRoamSaveSlot=TEXT("Automation_CoopProbe");
 auto Probe=MakeShared<FCoopProbe>(); Probe->Game=G; Probe->Host=Role.Equals(TEXT("Host"),ESearchCase::IgnoreCase);
 FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Probe](float Dt) { return Probe->Tick(Dt); }),.1f);
}
#endif
