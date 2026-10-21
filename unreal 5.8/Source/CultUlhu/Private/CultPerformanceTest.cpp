#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultPopulation.h"
#include "CultKitRuntime.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "ShaderCompiler.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
CSV_DECLARE_CATEGORY_EXTERN(CultRuntime);

// Real Free Roam: population, war, building and bot ticks remain enabled.
// CSV timings measure this editor fixture; they are not a foreground FPS claim.
class FCultFreeRoamProfile : public IAutomationLatentCommand {
 FAutomationTestBase* Test; int Phase=0,Frames=0; double Began=FPlatformTime::Seconds();
 TSharedFuture<FString> Written;
 bool Paired=false; int Segment=0; float Before=0,SettleUntil=0;
public:
 explicit FCultFreeRoamProfile(FAutomationTestBase* T,bool Compare=false):Test(T),Paired(Compare) {
  Before=IConsoleManager::Get().FindConsoleVariable(TEXT("cult.Population.DecisionInterval"))->GetFloat();
  if(Paired) IConsoleManager::Get().FindConsoleVariable(TEXT("cult.Population.DecisionInterval"))->Set(0.f,ECVF_SetByCode);
 }
 ~FCultFreeRoamProfile() { if(Paired) IConsoleManager::Get().FindConsoleVariable(TEXT("cult.Population.DecisionInterval"))->Set(Before,ECVF_SetByCode); }
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>180) { Test->AddError(TEXT("Free Roam profile timed out")); if(FCsvProfiler::Get()->IsCapturing()) FCsvProfiler::Get()->EndCapture(); return true; }
  UWorld* W=nullptr; for(auto& C:GEngine->GetWorldContexts()) if(C.World()&&C.World()->IsGameWorld()) W=C.World(); if(!W) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) {
   if(!Phase) { auto* G=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); G->SelectedCharacterId=TEXT("mi_go"); G->OpenCharacterSelect(ECultPlayMode::FreeRoam); G->SetInitialBeliefs({5,6,11}); G->StartPlay(); Phase=1; } return false;
  }
  if(Phase==1) {
   if(W->GetTimeSeconds()<FMath::Max(18.f,SettleUntil) || (GShaderCompilingManager&&GShaderCompilingManager->IsCompiling())) return false;
   int Population=0,Heroes=0,Actors=0;
   for(TActorIterator<AActor> It(W);It;++It) ++Actors;
   for(TActorIterator<ACultPopulation> It(W);It;++It) ++Population;
   for(TActorIterator<ACultUlhuPlayerPawn> It(W);It;++It) ++Heroes;
   Test->AddInfo(FString::Printf(TEXT("Warmed Free Roam profile: actors=%d population=%d heroes=%d worldTime=%.2f segment=%d interval=%.2f"),Actors,Population,Heroes,W->GetTimeSeconds(),Segment,IConsoleManager::Get().FindConsoleVariable(TEXT("cult.Population.DecisionInterval"))->GetFloat()));
   Test->TestTrue(TEXT("Profiling retains a populated live world"),Population>30&&Heroes>=3);
   const FString Dir=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Profiling/CultRuntime")); IFileManager::Get().MakeDirectory(*Dir,true);
   FCsvProfiler::Get()->BeginCapture(-1,Dir,(Paired?FString::Printf(TEXT("PopulationDecision_%d_"),Segment):FString(TEXT("FreeRoam_")))+FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S"))+TEXT(".csv")); Phase=2; return false;
  }
  if(Phase==2) {
   int Count=0; for(TActorIterator<ACultPopulation> It(W);It;++It) ++Count;
   CSV_CUSTOM_STAT(CultRuntime, ProfilePopulation,Count,ECsvCustomStatOp::Set);
   if(++Frames<(Paired?120:180)) return false;
   Written=FCsvProfiler::Get()->EndCapture(); Phase=3; return false;
  }
  if(Phase==3) {
   if(!Written.IsReady()) return false;
   const auto File=Written.Get(); Test->TestTrue(TEXT("Warmed gameplay CSV was written"),IFileManager::Get().FileSize(*File)>0); Test->AddInfo(TEXT("Runtime profile: ")+File);
   if(Paired&&++Segment<4) {
    const float Interval=Segment==3?0.f:.1f;
    IConsoleManager::Get().FindConsoleVariable(TEXT("cult.Population.DecisionInterval"))->Set(Interval,ECVF_SetByCode);
    Frames=0; SettleUntil=W->GetTimeSeconds()+1; Phase=1; return false;
   }
   // Public delivery paths, real native target, and overlapping caster sources.
   // Run after capture so the fixture cannot distort its measured workload.
   ACultUlhuPlayerPawn* Player=nullptr; for(TActorIterator<ACultUlhuPlayerPawn> It(W);It;++It) if(!It->IsBot()) Player=*It;
   if(!Player) { Test->AddError(TEXT("No player for effect expiry regression")); return true; }
   const FTransform Transform(Player->GetActorLocation()+FVector(10000,0,0));
   auto* Victim=W->SpawnActorDeferred<ACultUlhuPlayerPawn>(ACultUlhuPlayerPawn::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
   if(!Victim) { Test->AddError(TEXT("Cannot create effect expiry target")); return true; }
   Victim->ConfigureBot(TEXT("mi_go"),1); Victim->FinishSpawning(Transform); Victim->SetActorTickEnabled(false);
   {
    FCultKitRuntime Delivery(Player); Test->TestTrue(TEXT("Idle-path kit initializes"),Delivery.Initialize(TEXT("mi_go")));
    Delivery.SetCooldown(0,3); Delivery.Tick(.25f); Test->TestTrue(TEXT("Idle synchronization skip still advances cooldowns"),FMath::IsNearlyEqual(Delivery.Cooldown(0),2.75f));
    cultulhu::SpellDef Stun; Stun.effectKind="stun"; Stun.effectPower=.5f;
    Delivery.ResolveSpatial(Stun,Victim,Victim->GetActorLocation(),0); Victim->Tick(.001f);
    Test->TestTrue(TEXT("Freshly hit target receives real native control state"),Victim->IsIncapacitated());
    Delivery.Tick(.1f); Victim->Tick(.001f);
    Test->TestTrue(TEXT("Active effects keep synchronizing after an idle frame"),Victim->IsIncapacitated());
    Victim->SetControlState(true,false,0,Player->GetActorLocation(),true,-952);
    Delivery.Tick(10); Victim->Tick(.001f);
    Test->TestTrue(TEXT("Expiring one caster does not clear a second caster's stun"),Victim->IsIncapacitated());
    Victim->SetControlState(false,false,1,Player->GetActorLocation(),false,-952); Victim->Tick(.001f);
    Test->TestFalse(TEXT("Final active frame explicitly clears its expired effect"),Victim->IsIncapacitated());
    Delivery.Tick(.01f); Victim->Tick(.001f);
    Test->TestFalse(TEXT("Returning to idle never resurrects stale control state"),Victim->IsIncapacitated());
   }
   Victim->Destroy(); return true;
  }
  return false;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultFreeRoamPerformanceTest,"CultUlhu.Menu.PerformanceFreeRoam",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultFreeRoamPerformanceTest::RunTest(const FString&) {
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false)); ADD_LATENT_AUTOMATION_COMMAND(FCultFreeRoamProfile(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultPopulationDecisionProfileTest,"CultUlhu.Menu.PopulationDecisionProfile",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultPopulationDecisionProfileTest::RunTest(const FString&) {
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false)); ADD_LATENT_AUTOMATION_COMMAND(FCultFreeRoamProfile(this,true)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
#endif
