#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "CultAutomationPIE.h"
#include "CultFreeRoamSave.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultWorldDirector.h"
#include "CultPopulation.h"
#include "CultDestructible.h"
#include "CultFeralBeast.h"
#include "CultCorpse.h"
#include "CultLivingLandscape.h"
#include "beliefs/BeliefSystem.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "UI/CultFreeRoamMenuWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "UObject/StrongObjectPtr.h"
#include "RenderingThread.h"
#include "ImageUtils.h"
#include "InputKeyEventArgs.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace {
constexpr auto PersistenceTestSlot=TEXT("Automation_FreeRoamPersistence");
template<class T> T* Tagged(UWorld* W,const TCHAR* Tag) { for(TActorIterator<T> It(W);It;++It) if(It->ActorHasTag(Tag)) return *It; return nullptr; }
class FFreeRoamObservation : public IAutomationLatentCommand {
 FAutomationTestBase* T; int32 Phase=0,Seed=0,Cities=0,In=0;
 double Began=FPlatformTime::Seconds(); float At=0,BuildingHP=0,Power=0,Adoption=0,SavedHP=0,SavedStamina=0,SavedCooldown=0,SavedDash=0; FVector Position;
 TArray<uint8> Data; TWeakObjectPtr<ACultCorpse> LongCorpse,ShortCorpse,DotCorpse;
public:
 explicit FFreeRoamObservation(FAutomationTestBase* Test):T(Test) {}
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>160) { T->AddError(TEXT("Free Roam persistence test timed out")); return true; }
  auto* W=GEditor->PlayWorld.Get(); if(!W||W->bIsTearingDown) return false;
  auto* G=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); if(!G) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) {
   if(Phase==0) { G->FreeRoamSaveSlot=PersistenceTestSlot; G->OpenCharacterSelect(ECultPlayMode::FreeRoam); G->SetInitialBeliefs({1,2,3}); G->StartPlay(); Phase=1; }
   else if(Phase==4) { T->TestTrue(TEXT("Continue reads the durable save after leaving the world"),G->ContinueFreeRoam()); Phase=5; }
   return false;
  }
  auto* P=Cast<ACultUlhuPlayerPawn>(UGameplayStatics::GetPlayerPawn(W,0)); auto* D=ACultWorldDirector::Find(W);
  if(!P||!D||!G->FreeRoamReady) return false;
  if(Phase==1) {
   D->SetActorTickEnabled(false);
   auto* Arm=P->FindComponentByClass<USpringArmComponent>();
   T->TestTrue(TEXT("Free Roam uses the competitive shoulder-camera distance"),Arm&&FMath::IsNearlyEqual(Arm->TargetArmLength/P->GetActorScale3D().X,260.f,.01f));
   T->TestTrue(TEXT("Free Roam also uses the same shoulder offset"),Arm&&Arm->SocketOffset.Equals(FVector(0,90,15)*P->GetActorScale3D().X,.1));
   ACultDestructible* Building=nullptr;
   for(TActorIterator<ACultDestructible> It(W);It;++It) if(!It->IsDestroyed()) { Building=*It; break; }
   if(!T->TestNotNull(TEXT("A live building exists"),Building)) return true;
   T->TestTrue(TEXT("Even the smallest building has the increased base resistance"),Building->GetMaxHealth()>=204.f);
   Building->Tags.Add(TEXT("PersistenceBuilding")); Building->ApplyDamage(55,Building->GetActorLocation()); BuildingHP=Building->GetHealth();
   FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
   auto* Follower=W->SpawnActor<ACultPopulation>(P->FeetLocation()+FVector(6000,0,100),FRotator::ZeroRotator,Params);
   Follower->Tags.Add(TEXT("PersistenceFollower")); Follower->RecruitTo(0); Follower->SetLoyalty(82,false); Follower->PlayerOrder(TEXT("Patrol"),Follower->GetActorLocation(),nullptr,true);
   Follower->SetActorTickEnabled(false);
   for(TActorIterator<ACultDestructible> It(W);It;++It) if(*It!=Building&&It->GetFaction()==1&&!It->IsDestroyed()) { It->Tags.Add(TEXT("PersistenceRuins")); It->ApplyDamage(It->GetHealth()+1,It->GetActorLocation(),0,P); break; }
   for(TActorIterator<ACultFeralBeast> It(W);It;++It) if(!It->IsDead()) { It->Tags.Add(TEXT("PersistenceCaptive")); It->Capture(P,0); It->SetBredHealth(333); It->SetActorTickEnabled(false); break; }
   P->ReceiveDamage(123); P->AbilityF(); P->TryDash(FVector::RightVector);
   P->GetCharacterMovement()->RemoveRootMotionSource(TEXT("CultDash")); P->GetCharacterMovement()->StopMovementImmediately();
   auto HUD=P->CombatHUD(); SavedHP=HUD.Health; SavedStamina=HUD.Stamina; SavedCooldown=HUD.Cooldowns[1]; SavedDash=HUD.DashCooldown;
   T->TestTrue(TEXT("The save includes an actual used ability cooldown"),SavedCooldown>0);
   T->TestTrue(TEXT("The save includes spent stamina and a dash cooldown"),SavedStamina<100&&SavedDash>0);
   D->DeclareWar(1,2);
   auto Active=D->Beliefs().active(); for(int32 B=0;B<12;++B) if(!D->Beliefs().isActive(cultulhu::Belief(B))) { In=B; break; }
   T->TestTrue(TEXT("A belief adoption starts before saving"),D->RequestBelief(In,int32(Active[0])));
   Adoption=D->Beliefs().adoptionRemaining(cultulhu::Belief(In));
   Cities=D->CityCount(); Power=D->FaithPower(); Seed=G->FreeRoamSeed; Position=P->GetActorLocation();
   FString Error;
   T->TestTrue(TEXT("Capture contains the world and core progression"),CultFreeRoamSave::Capture(W,Data,Error));
   auto Bad=Data; Bad[20]^=1;
   T->TestFalse(TEXT("Corrupt data is rejected before the world is touched"),CultFreeRoamSave::Restore(W,Bad,Error));
   T->TestTrue(TEXT("Rejected load preserves the existing actor"),IsValid(Building));
   Building->ApplyDamage(10,Building->GetActorLocation()); Follower->Desert();
   T->TestTrue(TEXT("In-place round trip restores a modified world"),CultFreeRoamSave::Restore(W,Data,Error));
   Check(W,G,P,D);
   // A dedicated test slot never overwrites the player's world.
   T->TestTrue(TEXT("Snapshot writes to disk"),CultFreeRoamSave::Write(Data,Error,PersistenceTestSlot));
   T->TestTrue(TEXT("Second write preserves a valid backup"),CultFreeRoamSave::Write(Data,Error,PersistenceTestSlot));
   const FString File=FPaths::ProjectSavedDir()/TEXT("SaveGames")/(FString(PersistenceTestSlot)+TEXT(".sav"));
   T->TestTrue(TEXT("Test can corrupt its isolated primary slot"),FFileHelper::SaveArrayToFile(Bad,*File));
   TArray<uint8> Recovered; T->TestTrue(TEXT("A damaged primary recovers from backup"),CultFreeRoamSave::Read(Recovered,Error,PersistenceTestSlot)); T->TestTrue(TEXT("Backup is byte-identical to the valid snapshot"),Recovered==Data);
   T->TestTrue(TEXT("The primary slot can be safely restored after recovery"),CultFreeRoamSave::Write(Data,Error,PersistenceTestSlot));
   W->GetFirstPlayerController()->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::Escape,IE_Pressed,1.f,false,FPlatformTime::Cycles64()));
   W->GetFirstPlayerController()->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::Escape,IE_Released,0.f,false,FPlatformTime::Cycles64()));
   Phase=10; return false;
  }
  if(Phase==10) {
   if(!G->FreeRoamMenuOpen) return false; // InputKey is dispatched on the next input frame.
   T->TestTrue(TEXT("Escape menu pauses solo play without changing maps"),G->FreeRoamMenuOpen&&UGameplayStatics::IsGamePaused(W)&&W->GetName().Contains(TEXT("L_DarkWilds")));
   TArray<UUserWidget*> Menus; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Menus,UCultFreeRoamMenuWidget::StaticClass(),true);
   if(T->TestEqual(TEXT("Only one Free Roam menu is visible"),Menus.Num(),1)) {
    auto* Menu=Menus[0]; Menu->RemoveFromParent(); FWidgetRenderer Renderer(true,true);
    for(FIntPoint Size:{FIntPoint(960,540),FIntPoint(1280,720)}) {
     const float DPI=GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Size);
     TSharedRef<SWidget> Root=SNew(SDPIScaler).DPIScale(DPI)[Menu->TakeWidget()];
     TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(FVector2D(Size),TF_Bilinear,true));
     for(int32 I=0;I<4;++I) { Menu->ForceLayoutPrepass(); Renderer.DrawWidget(Target.Get(),Root,FVector2D(Size),0.f); FlushRenderingCommands(); }
     Menu->WidgetTree->ForEachWidget([&](UWidget* Widget) { if(auto* Button=Cast<UButton>(Widget)) {
      auto& Geo=Button->GetCachedGeometry(); auto Top=Geo.GetAbsolutePosition(),Bottom=Top+Geo.GetLocalSize()*Geo.GetAccumulatedLayoutTransform().GetScale();
      T->TestTrue(TEXT("Every menu button fits inside the screen"),Top.X>=0&&Top.Y>=0&&Bottom.X<=Size.X+1&&Bottom.Y<=Size.Y+1);
     } });
     TArray<FColor> Pixels; FReadSurfaceDataFlags ReadFlags(RCM_UNorm); ReadFlags.SetLinearToGamma(false); Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,ReadFlags); TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(Size.X,Size.Y,MakeArrayView(Pixels),PNG);
     FString Shot=FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/FreeRoamMenu_%dx%d.png"),Size.X,Size.Y);
     IFileManager::Get().MakeDirectory(*FPaths::GetPath(Shot),true); T->TestTrue(TEXT("Native menu preview saves"),FFileHelper::SaveArrayToFile(PNG,*Shot));
    }
   }
   G->ResumeFreeRoam(); T->TestFalse(TEXT("Resume restores game input and time"),UGameplayStatics::IsGamePaused(W));
   auto Corpse=[&](ECultPlayMode Mode) { G->SelectedMode=Mode; auto* Body=W->SpawnActor<AStaticMeshActor>(); auto* C=ACultCorpse::Leave(Body); Body->Destroy(); return C; };
   LongCorpse=Corpse(ECultPlayMode::FreeRoam); ShortCorpse=Corpse(ECultPlayMode::Domination); DotCorpse=Corpse(ECultPlayMode::Dotgoo); G->SelectedMode=ECultPlayMode::FreeRoam;
   T->TestTrue(TEXT("Free Roam corpse lifetime is five seconds"),LongCorpse.IsValid()&&FMath::IsNearlyEqual(LongCorpse->GetLifeSpan(),5.f,.01f));
   T->TestTrue(TEXT("Domination corpse lifetime is two seconds"),ShortCorpse.IsValid()&&FMath::IsNearlyEqual(ShortCorpse->GetLifeSpan(),2.f,.01f));
   T->TestTrue(TEXT("DOTGOO corpse lifetime is two seconds"),DotCorpse.IsValid()&&FMath::IsNearlyEqual(DotCorpse->GetLifeSpan(),2.f,.01f));
   At=W->GetTimeSeconds(); Phase=2; return false;
  }
  if(Phase==2&&W->GetTimeSeconds()-At>2.2f) {
   T->TestFalse(TEXT("Domination corpse actually despawns"),ShortCorpse.IsValid()); T->TestFalse(TEXT("DOTGOO corpse actually despawns"),DotCorpse.IsValid()); T->TestTrue(TEXT("Free Roam corpse remains at two seconds"),LongCorpse.IsValid()); Phase=3;
  }
  if(Phase==3&&W->GetTimeSeconds()-At>5.3f) {
   T->TestFalse(TEXT("Free Roam corpse actually despawns"),LongCorpse.IsValid()); G->ReturnToMenu(); Phase=4; return false;
  }
  if(Phase==5) { D->SetActorTickEnabled(false); Check(W,G,P,D); T->AddInfo(FString::Printf(TEXT("Durable snapshot: %d bytes, seed %d, %d cities"),Data.Num(),Seed,Cities)); return true; }
  return false;
 }
 void Check(UWorld* W,UCultUlhuGameInstance* G,ACultUlhuPlayerPawn* P,ACultWorldDirector* D) {
  auto* B=Tagged<ACultDestructible>(W,TEXT("PersistenceBuilding")); auto* F=Tagged<ACultPopulation>(W,TEXT("PersistenceFollower"));
  T->TestTrue(TEXT("Damaged building health survives loading"),B&&FMath::IsNearlyEqual(B->GetHealth(),BuildingHP,.1f));
  T->TestTrue(TEXT("Cult membership and loyalty survive loading"),F&&F->IsCultist()&&F->GetFaction()==0&&FMath::IsNearlyEqual(F->GetLoyalty(),82.f,1.f));
  T->TestTrue(TEXT("Player battle assignments survive loading"),F&&F->HasPlayerBattle());
  auto* Ruins=Tagged<ACultDestructible>(W,TEXT("PersistenceRuins")); auto* Captive=Tagged<ACultFeralBeast>(W,TEXT("PersistenceCaptive"));
  T->TestTrue(TEXT("Destroyed structures stay destroyed"),Ruins&&Ruins->IsDestroyed()&&Ruins->GetHealth()<=0);
  T->TestTrue(TEXT("Captured creature allegiance and bred health survive loading"),Captive&&Captive->IsCaptured()&&Captive->GetCaptiveFaction()==0&&FMath::IsNearlyEqual(Captive->GetMaxHealth(),333.f)&&FMath::IsNearlyEqual(Captive->GetHealth(),333.f));
  auto HUD=P->CombatHUD();
  T->TestTrue(TEXT("Damaged player health survives loading"),FMath::IsNearlyEqual(HUD.Health,SavedHP,.1f));
  T->TestTrue(TEXT("Player stamina survives loading"),FMath::IsNearlyEqual(HUD.Stamina,SavedStamina,3.f));
  T->TestTrue(TEXT("Ability cooldown survives loading"),FMath::IsNearlyEqual(HUD.Cooldowns[1],SavedCooldown,.3f));
  T->TestTrue(TEXT("Dash cooldown survives loading"),FMath::IsNearlyEqual(HUD.DashCooldown,SavedDash,.3f));
  T->TestEqual(TEXT("City registry survives loading"),D->CityCount(),Cities);
  T->TestTrue(TEXT("Rival diplomacy survives loading"),D->AtWar(1,2)&&D->AtWar(2,1));
  T->TestTrue(TEXT("Player power survives loading"),FMath::IsNearlyEqual(D->FaithPower(),Power,.1f));
  T->TestTrue(TEXT("Belief adoption progress survives loading"),FMath::IsNearlyEqual(float(D->Beliefs().adoptionRemaining(cultulhu::Belief(In))),Adoption,2.f));
  T->TestEqual(TEXT("Procedural terrain seed survives loading"),G->FreeRoamSeed,Seed);
  T->TestTrue(TEXT("Player position survives loading"),P->GetActorLocation().Equals(Position,10.f));
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultFreeRoamPersistenceTest,"CultUlhu.FreeRoam.Persistence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultFreeRoamPersistenceTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FFreeRoamObservation(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
#endif
