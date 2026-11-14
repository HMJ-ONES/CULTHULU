#pragma once
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Tests/AutomationEditorCommon.h"
#include "LevelEditor.h"
#include "GameFramework/GameStateBase.h"

// FStartPIECommand only queues a request and immediately completes. A following
// observation can otherwise see the preceding test's still-live world, issue
// travel into it, then lose that travel when the new session replaces it.
class FCultStartFreshPIE : public IAutomationLatentCommand {
 int32 Phase=0; double Began=FPlatformTime::Seconds();
public:
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>60) {
   if(auto* Test=FAutomationTestFramework::Get().GetCurrentTest()) Test->AddError(TEXT("Fresh PIE session did not become ready"));
   return true;
  }
  if(Phase==0) { GEditor->RequestEndPlayMap(); Phase=1; return false; }
  if(Phase==1) {
   if(GEditor->PlayWorld) return false;
   FRequestPlaySessionParams Params;
   Params.DestinationSlateViewport=FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor")).GetFirstActiveViewport();
   GEditor->RequestPlaySession(Params); GEditor->StartQueuedPlaySessionRequest(); Phase=2; return false;
  }
  auto* World=GEditor->PlayWorld.Get();
  return World&&!World->bIsTearingDown&&World->AreActorsInitialized()&&World->GetGameState()&&World->GetGameState()->HasMatchStarted()&&World->GetFirstPlayerController();
 }
};
#endif
