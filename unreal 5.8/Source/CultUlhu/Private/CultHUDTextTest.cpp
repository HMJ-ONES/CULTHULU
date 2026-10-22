#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#if WITH_EDITOR
#include "CultAutomationPIE.h"
#endif
#include "CultHUDTextProbe.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultMatchDirector.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Slate/SceneViewport.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HighResScreenshot.h"
#include "HAL/FileManager.h"

class FCultHUDTextObservation : public IAutomationLatentCommand {
 FAutomationTestBase* Test; ECultPlayMode Mode; FString Character; double Began=FPlatformTime::Seconds(),At=0;
 int32 Phase=0,Shape=0; bool ResetRequested=false,Events=false,Seeded=false,PreviouslyPaused=false; FString Shot,SavedAnnouncement;
 TWeakObjectPtr<UGameViewportClient> Client;
 FHighResScreenshotConfig PreviousCapture; uint32 PreviousX=0,PreviousY=0;
 const TArray<FIntPoint> Sizes={FIntPoint(640,360),FIntPoint(1280,720),FIntPoint(1920,720)};
 FString ModeName() const { return FString(Mode==ECultPlayMode::FreeRoam?TEXT("FreeRoam"):Mode==ECultPlayMode::Domination?TEXT("Domination"):TEXT("Moba"))+(Character==TEXT("mi_go")?TEXT(""):TEXT("_Cthulhu")); }
public:
 explicit FCultHUDTextObservation(FAutomationTestBase* In,ECultPlayMode Choice,FString Hero=TEXT("mi_go"),bool EventChecks=false):Test(In),Mode(Choice),Character(MoveTemp(Hero)),Events(EventChecks) { PreviousCapture=GetHighResScreenshotConfig(); PreviousX=GScreenshotResolutionX; PreviousY=GScreenshotResolutionY; CultHUDTextProbe::Enabled=true; }
 ~FCultHUDTextObservation() {
  GetHighResScreenshotConfig()=PreviousCapture; GScreenshotResolutionX=PreviousX; GScreenshotResolutionY=PreviousY;
  CultHUDTextProbe::Enabled=false; CultHUDTextProbe::Samples.Reset(); CultHUDTextProbe::Captured.Reset();
  if(Events&&Seeded&&Client.IsValid()) UGameplayStatics::SetGamePaused(Client->GetWorld(),PreviouslyPaused);
 }
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>150) { Test->AddError(TEXT("Live HUD text-render check timed out")); return true; }
  UWorld* World=nullptr; for(auto& Context:GEngine->GetWorldContexts()) if(Context.World()&&Context.World()->IsGameWorld()) World=Context.World();
  if(!World||World->bIsTearingDown) return false;
  if(World->GetName().Contains(TEXT("L_MainMenu"))) {
   if(!Phase) { auto* Game=Cast<UCultUlhuGameInstance>(World->GetGameInstance()); Game->SelectedCharacterId=Character; Game->OpenCharacterSelect(Mode); if(Mode==ECultPlayMode::FreeRoam) { Game->SetInitialBeliefs({5,6,11}); Game->StartPlay(); } else Game->OpenBeliefSelect(); Phase=1; }
   return false;
  }
  if(Phase==0) { if(!ResetRequested) { UGameplayStatics::OpenLevel(World,FName(TEXT("/Game/Maps/L_MainMenu"))); ResetRequested=true; } return false; }
  if(World->GetTimeSeconds()<3||!World->GetFirstPlayerController()) return false;
  if(Phase==1) {
   Client=World->GetGameViewport(); if(!Client.IsValid()||!Client->GetGameViewport()) return false;
   if(Events&&!Seeded) {
    auto* Match=ACultMatchDirector::Find(World); if(!Match) return false;
    auto* Player=Cast<ACultUlhuPlayerPawn>(World->GetFirstPlayerController()->GetPawn());
    SavedAnnouncement=Match->Announcement();
    for(int I=5;I<10;++I) Match->HeroDied(Match->Fighters()[I].Pawn.Get(),I==8?nullptr:Player);
    Test->TestEqual(TEXT("Burst eliminations retain only four recent events"),Match->Eliminations().Num(),4);
    Test->TestTrue(TEXT("Uncredited deaths have an explicit environment source"),Match->Eliminations().ContainsByPredicate([](const auto& E){return E.Attacker==TEXT("ENVIRONMENT")&&E.AttackerTeam==-1;}));
    Test->TestEqual(TEXT("Five deaths do not replace the original objective announcement"),Match->Announcement(),SavedAnnouncement);
    const int Kills=Match->Fighters()[0].Kills; Match->HeroDied(Match->Fighters()[9].Pawn.Get(),Player);
    Test->TestEqual(TEXT("A repeated dead-vessel event cannot award another kill"),Match->Fighters()[0].Kills,Kills);
    Match->TogglePause(); Match->Tick(7.f);
    Test->TestEqual(TEXT("Pause preserves objective copy"),Match->Announcement(),SavedAnnouncement);
    Test->TestEqual(TEXT("Pause preserves elimination lifetime"),Match->Eliminations().Num(),4);
    Match->TogglePause(); PreviouslyPaused=UGameplayStatics::IsGamePaused(World); UGameplayStatics::SetGamePaused(World,true); Seeded=true;
   }
   Shot=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/HUDText_%s%s_%dx%d.png"),*ModeName(),Events?TEXT("_Events"):TEXT(""),Sizes[Shape].X,Sizes[Shape].Y));
   IFileManager::Get().MakeDirectory(*FPaths::GetPath(Shot),true); IFileManager::Get().Delete(*Shot);
   CultHUDTextProbe::Captured.Reset(); CultHUDTextProbe::CaptureFrame=MAX_uint64;
   auto& Capture=GetHighResScreenshotConfig(); Capture.SetHDRCapture(false); Capture.SetMaskEnabled(false); Capture.SetForce128BitRendering(false);
   if(!Capture.SetResolution(Sizes[Shape].X,Sizes[Shape].Y)) { Test->AddError(TEXT("Requested HUD render target resolution was rejected")); return true; }
   Capture.SetFilename(Shot);
   if(!Client->GetGameViewport()->TakeHighResScreenShot()) { Test->AddError(TEXT("Dedicated HUD capture could not start")); return true; }
   At=FPlatformTime::Seconds(); Phase=2; return false;
  }
  if(Phase==2) {
   if(FPlatformTime::Seconds()-At<.4) return false;
   if(IFileManager::Get().FileSize(*Shot)<24) return false;
   TArray<uint8> Png; if(!FFileHelper::LoadFileToArray(Png,*Shot)||Png.Num()<24) return false;
   auto Dimension=[&](int AtByte) { return (uint32(Png[AtByte])<<24)|(uint32(Png[AtByte+1])<<16)|(uint32(Png[AtByte+2])<<8)|uint32(Png[AtByte+3]); };
   Test->TestEqual(TEXT("Saved framebuffer PNG has the requested actual width"),Dimension(16),uint32(Sizes[Shape].X));
   Test->TestEqual(TEXT("Saved framebuffer PNG has the requested actual height"),Dimension(20),uint32(Sizes[Shape].Y));
   const auto* Hint=CultHUDTextProbe::Captured.FindByPredicate([this](const auto& S){return S.Text.StartsWith(Mode==ECultPlayMode::FreeRoam?TEXT("B  BELIEFS"):TEXT("TAB SCORE"));});
   if(!Hint||!Hint->Canvas.Equals(FVector2D(Sizes[Shape]),1)) return false;
   Test->AddInfo(FString::Printf(TEXT("HUD %s %dx%d: points=%.3f DPI=%.3f cache=%d position=%s measured=%s measuredAtDPI=%s drawn=%s lines=%d"),*ModeName(),Sizes[Shape].X,Sizes[Shape].Y,Hint->Points,Hint->DPI,Hint->CacheType,*Hint->Position.ToString(),*Hint->Measured.ToString(),*Hint->MeasuredAtDPI.ToString(),*Hint->Drawn.ToString(),Hint->Text.Contains(TEXT("\n"))?2:1));
   Test->TestTrue(TEXT("Measured control hints match actual Canvas glyph advances"),FMath::Abs(Hint->MeasuredAtDPI.X-Hint->Drawn.X)<=2);
   const float Scale=FMath::Clamp(FMath::Min(Hint->Canvas.X/1600.,Hint->Canvas.Y/900.),.55,2.);
   const FBox2D HintBox(Hint->Position*Hint->DPI,Hint->Position*Hint->DPI+Hint->Drawn);
   Test->TestTrue(TEXT("Rendered controls retain the intended right-hand margin"),HintBox.Max.X<=(Hint->Canvas.X-42*Scale)*Hint->DPI+2);
   Test->TestTrue(TEXT("Rendered control text stays inside the actual viewport"),HintBox.Min.X>=0&&HintBox.Min.Y>=0&&HintBox.Max.Y<Hint->Canvas.Y*Hint->DPI);
   for(const auto& Text:CultHUDTextProbe::Captured) if(Text.Text==TEXT("THE DARK WILDS")||Text.Text==TEXT("DOMINATION")||Text.Text==TEXT("GREAT OLD ONES")||Text.Text.StartsWith(TEXT("POWER "))) {
    const FBox2D Other(Text.Position*Text.DPI,Text.Position*Text.DPI+Text.Drawn);
    Test->TestFalse(TEXT("Control hints never overlap the actual mode/power glyphs"),HintBox.Intersect(Other));
   }
   if(Mode!=ECultPlayMode::FreeRoam) {
    const float MatchScale=FMath::Clamp(FMath::Min(Hint->Canvas.X/1600.,Hint->Canvas.Y/900.),.65,2.);
    const FBox2D Score(FVector2D(Hint->Canvas.X*.5-205*MatchScale,12*MatchScale)*Hint->DPI,FVector2D(Hint->Canvas.X*.5+205*MatchScale,76*MatchScale)*Hint->DPI);
    Test->TestFalse(TEXT("The backed control hints remain clear of the competitive score panel"),HintBox.ExpandBy(FMath::Max(6.f,8*Scale)*Hint->DPI).Intersect(Score));
   }
   const auto Box=[](const auto& Text) { return FBox2D(Text.Position*Text.DPI,Text.Position*Text.DPI+Text.Drawn); };
   if(Events) {
    const CultHUDTextProbe::Sample* Alert=nullptr; TArray<const CultHUDTextProbe::Sample*> Feed,Goals;
    for(const auto& T:CultHUDTextProbe::Captured) {
     if(T.Text.Replace(TEXT("\n"),TEXT(" "))==SavedAnnouncement) Alert=&T;
     if(T.Text.Contains(TEXT(" > "))&&T.Position.X>Hint->Canvas.X*.65) Feed.Add(&T);
     if(T.Text==TEXT("CONTROL SEAL")||T.Text==TEXT("PUSH ALL THREE LANES")||T.Text==TEXT("RALLY")||T.Text==TEXT("DESTROY NEXUS WARD")||T.Text==TEXT("ENEMY GOD - EXPOSED")) {
      Goals.Add(&T); Test->TestTrue(TEXT("Projected objective-label glyphs remain inside the viewport"),Box(T).Min.X>=0&&Box(T).Max.X<=Hint->Canvas.X&&Box(T).Min.Y>=0&&Box(T).Max.Y<Hint->Canvas.Y);
     }
    }
    Test->TestNotNull(TEXT("Objective announcement is actually rendered during elimination bursts"),Alert);
    if(Alert) Test->TestTrue(TEXT("Wrapped objective glyphs fit the actual viewport"),Box(*Alert).Min.X>=0&&Box(*Alert).Max.X<=Hint->Canvas.X&&Box(*Alert).Min.Y>=0&&Box(*Alert).Max.Y<Hint->Canvas.Y);
    if(Alert) for(const auto* Goal:Goals) Test->TestFalse(TEXT("Projected guidance stays separate from the objective announcement"),Box(*Goal).ExpandBy(3).Intersect(Box(*Alert)));
    Test->TestEqual(TEXT("Responsive elimination feed renders three normal rows or one compact row"),Feed.Num(),Sizes[Shape].X==640?1:3);
    for(const auto* E:Feed) {
     Test->TestTrue(TEXT("Feed glyphs fit the viewport"),Box(*E).Min.X>=0&&Box(*E).Max.X<=Hint->Canvas.X&&Box(*E).Max.Y<Hint->Canvas.Y);
     for(const auto& Other:CultHUDTextProbe::Captured) if(Other.Position.Y>Hint->Canvas.Y*.5&&Other.Text!=E->Text) Test->TestFalse(TEXT("Feed does not collide with ability or movement glyphs"),Box(*E).Intersect(Box(Other)));
     if(Alert) Test->TestFalse(TEXT("Eliminations and objective glyphs remain separate"),Box(*E).Intersect(Box(*Alert)));
    }
    TArray<FString> AlertLines; if(Alert) Alert->Text.ParseIntoArrayLines(AlertLines);
    Test->AddInfo(FString::Printf(TEXT("Match events %s %dx%d: %d actual feed rows; objective lines=%d"),*ModeName(),Sizes[Shape].X,Sizes[Shape].Y,Feed.Num(),AlertLines.Num()));
   }
   auto* Hero=Cast<ACultUlhuPlayerPawn>(World->GetFirstPlayerController()->GetPawn());
   if(Hero) {
    const auto Data=Hero->CombatHUD(); TArray<FBox2D> AbilityText;
    if(Character==TEXT("cthulhu_avatar")&&Mode==ECultPlayMode::Dotgoo) Test->TestTrue(TEXT("The requested Cthulhu body exposes its actual minion-RMB instruction"),Data.RmbHint.Contains(TEXT("minion"),ESearchCase::IgnoreCase));
    for(int I=0;I<3;++I) {
     const auto* Name=CultHUDTextProbe::Captured.FindByPredicate([&](const auto& Text){return Text.Position.Y>Hint->Canvas.Y*.5&&Text.Text.Replace(TEXT("\n"),TEXT(" "))==Data.AbilityNames[I];});
     Test->TestNotNull(TEXT("Actual canonical ability label is present"),Name); if(!Name) continue;
     const auto* State=CultHUDTextProbe::Captured.FindByPredicate([&](const auto& Text){return FMath::Abs(Text.Position.X-Name->Position.X)<1&&Text.Position.Y>Name->Position.Y&&(Text.Text==TEXT("READY")||Text.Text.Contains(TEXT("COOLDOWN"))||Text.Text.StartsWith(TEXT("LEVEL "))||Text.Text==TEXT("PICK THIS"));});
     Test->TestNotNull(TEXT("Each actual ability label has its state row"),State);
     Test->TestTrue(TEXT("Wrapped ability glyphs fit the card width"),Name->Drawn.X<=(148*Scale-2*FMath::Max(6.f,8*Scale))*Name->DPI+1);
     AbilityText.Add(Box(*Name));
     if(State) { Test->TestFalse(TEXT("Actual wrapped ability and state glyphs never collide"),Box(*Name).Intersect(Box(*State))); AbilityText.Add(Box(*State)); Test->AddInfo(FString::Printf(TEXT("HUD ability %s %dx%d slot=%d name=%s extent=%s state=%s extent=%s"),*ModeName(),Sizes[Shape].X,Sizes[Shape].Y,I,*Name->Position.ToString(),*Name->Drawn.ToString(),*State->Position.ToString(),*State->Drawn.ToString())); }
    }
    for(const auto& Text:CultHUDTextProbe::Captured) if(Text.Text==TEXT("TAP SHIFT DASH")||Text.Text.StartsWith(TEXT("DASH  "))||Text.Text==TEXT("LOW STAMINA")||Text.Text==TEXT("HOLD SHIFT  SPRINT")) {
     for(const auto& Ability:AbilityText) Test->TestFalse(TEXT("Movement instructions remain separate from ability glyphs"),Box(Text).Intersect(Ability));
     Test->TestTrue(TEXT("Movement instruction glyphs remain inside the viewport"),Box(Text).Min.X>=0&&Box(Text).Max.X<=Hint->Canvas.X*Hint->DPI&&Box(Text).Max.Y<=Hint->Canvas.Y*Hint->DPI);
    }
    if(!Data.RmbHint.IsEmpty()) {
     const auto* Rmb=CultHUDTextProbe::Captured.FindByPredicate([&](const auto& Text){return Text.Text==Data.RmbHint;}); Test->TestNotNull(TEXT("Native special-RMB instruction remains visible"),Rmb);
     if(Rmb) for(const auto& Ability:AbilityText) Test->TestFalse(TEXT("Special-RMB glyphs never collide with ability glyphs"),Box(*Rmb).Intersect(Ability));
    }
   }
   Phase=3; return false;
  }
  if(FScreenshotRequest::IsScreenshotRequested()) return false;
  Test->TestTrue(TEXT("Actual viewport HUD render is retained for visual inspection"),IFileManager::Get().FileExists(*Shot));
  if(++Shape<Sizes.Num()) { Phase=1; return false; }
  if(Events) { UGameplayStatics::SetGamePaused(World,PreviouslyPaused); auto* Match=ACultMatchDirector::Find(World); Match->Tick(6.1f); Test->TestEqual(TEXT("Old elimination events expire after six active seconds"),Match->Eliminations().Num(),0); }
  return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultHUDTextRuntimeTest,"CultUlhu.Runtime.HUDTextRendering",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCultHUDTextRuntimeTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultHUDTextObservation(this,ECultPlayMode::FreeRoam)); return true; }
#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultHUDTextTest,"CultUlhu.Menu.HUDTextRendering",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultHUDTextTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultHUDTextObservation(this,ECultPlayMode::FreeRoam)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultHUDMatchTextTest,"CultUlhu.Menu.MatchTextRendering",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultHUDMatchTextTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultHUDTextObservation(this,ECultPlayMode::Domination)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultHUDMobaTextTest,"CultUlhu.Menu.MobaTextRendering",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultHUDMobaTextTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultHUDTextObservation(this,ECultPlayMode::Dotgoo)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultHUDMobaLiftTextTest,"CultUlhu.Menu.MobaLiftTextRendering",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultHUDMobaLiftTextTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultHUDTextObservation(this,ECultPlayMode::Dotgoo,TEXT("cthulhu_avatar"))); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultDominationEventsTest,"CultUlhu.Menu.DominationEventReadability",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultDominationEventsTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultHUDTextObservation(this,ECultPlayMode::Domination,TEXT("cthulhu_avatar"),true)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultMobaEventsTest,"CultUlhu.Menu.MobaEventReadability",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultMobaEventsTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultHUDTextObservation(this,ECultPlayMode::Dotgoo,TEXT("cthulhu_avatar"),true)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
#endif
#endif
